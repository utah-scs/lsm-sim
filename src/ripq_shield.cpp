#include "ripq_shield.h"

ripq_shield::ripq_shield(stats stat,size_t block_size,size_t num_sections, size_t dram_size, int num_dsections, size_t flash_size)
  : ripq(stat, block_size, num_sections, flash_size)
  , dsections{}
  , num_dsections(num_dsections)
  , dsection_size{}
  , dram_map{}
{
  dsection_size = dram_size / num_dsections;
  assert(dsection_size && num_dsections && flash_size);
  
  for (int i = 0; i < num_dsections; i++) {
    dsection_ptr curr_section(new dram_section(i));
    dsections.push_back(curr_section);
  }
}


ripq_shield::~ripq_shield () {
}

//add new Request to dsection_id
void ripq_shield::dram_add(const Request *r, int dsection_id) {
  assert(uint32_t(r->size()) < dsection_size);
  auto it = dram_map.find(r->kid);  //FIXME delete
  assert(it == dram_map.end());

  dsection_ptr target_dsection = dsections[dsection_id];
  item_ptr new_item = target_dsection->add(r);
  dbalance(target_dsection->id);
  while (dsections[num_dsections-1]->filled_bytes > dsection_size) {
    dram_evict();
  }

  dram_map[r->kid] = new_item;
}

ripq_shield::item_ptr ripq_shield::dram_section::add(const Request *req) {
  filled_bytes += req->size();
  num_items++;
  item_ptr new_item(new item(*req, shared_from_this()));
  items.push_front(new_item);
  new_item->dram_it = items.begin();
  return new_item;
}

void ripq_shield::dbalance(int start) {
  for (uint32_t i = start; i < num_dsections-1; i++) {
    while (dsections[i]->filled_bytes > dsection_size) {
      const Request *evicted_req = dsections[i]->evict();
      assert(evicted_req);
      dram_map[evicted_req->kid] = dsections[i+1]->add(evicted_req);
    }
  }
}

void ripq_shield::dram_section::remove(item_ptr curr_item) {
//  assert(std::find(items.begin(), items.end(), curr_item) != items.end());

  assert (filled_bytes >= (uint32_t)curr_item->req.size());
  filled_bytes -= (uint32_t)curr_item->req.size();
  num_items--;
  items.erase(curr_item->dram_it);
}

const Request* ripq_shield::dram_section::evict() {
  assert(!items.empty());
  item_ptr last_item = items.back();
  assert (filled_bytes >= (uint32_t)last_item->req.size());
  filled_bytes -= (uint32_t)last_item->req.size();
  num_items--;
  items.pop_back();
  return &last_item->req;
}

void ripq_shield::dram_evict() {
  const Request *curr_req = dsections[num_dsections-1]->evict();
  stat.bytes_cached -= curr_req->size();
  dram_map.erase(curr_req->kid);
}

//Flash eviction function
void ripq_shield::evict() {
  for (uint32_t i = 0; i < num_sections; i++) {
    assert(sections[i]->count_filled_bytes() == (int)sections[i]->filled_bytes);
  }
  section_ptr tail_section = sections.back();
  assert(!tail_section->blocks.empty());

  //We first have to evict the block from the section, because the reallocations might evict blocks too
  block_ptr evicted_block = tail_section->evict_block();
  assert(!evicted_block->active);
  assert(!evicted_block->is_virtual || evicted_block->num_items == 0);
  assert(stat.bytes_cached > evicted_block->filled_bytes);

  //Remove all items from their corresponding virtual blocks before reallocating them to their new sections. Otherwise we get corner cases where we evict virtual blocks with size>0
  for(item_list::iterator iter = evicted_block->items.begin(); iter != evicted_block->items.end(); iter++) {
    if ((*iter)->virtual_block != (*iter)->physical_block) {
      block_ptr virtual_block = (*iter)->virtual_block;
      virtual_block->remove(*iter);
    }
  }

  //Go through all items of the evcited block (last block in tail section), reallocate new items
  //in other sections according to their virtual block
  while (!evicted_block->items.empty()) {
    if (evicted_block->is_virtual) break;

    ripq::item_ptr evicted_item = evicted_block->items.back();
    assert(evicted_item->physical_block == evicted_block);

    block_ptr virtual_block = evicted_item->virtual_block;
    assert(!evicted_block->active);

    //First pop the item out of the block
    evicted_block->items.pop_back();

    //Item can become a ghost if the key was updated with a new a value
    if (!evicted_item->is_ghost)
      map.erase(evicted_item->req.kid);

    //Check if we should reallocate in another section
    if (virtual_block != evicted_item->physical_block) { //Reallocate in flash
      assert(!evicted_item->physical_block->is_virtual);
      assert(!evicted_item->is_ghost);

      add(&evicted_item->req, virtual_block->s->id);

      for (uint32_t i = 0; i < num_sections; i++) {
        assert(sections[i]->count_filled_bytes() == (int)sections[i]->filled_bytes);
      }
    } else if (!evicted_item->is_ghost) {
      dram_add(&evicted_item->req, 0); //Reallocate to dram klru head
    }
  }

  balance();
}

size_t ripq_shield::proc(const Request *r, bool warmup) {

  assert(r->size() > 0);

  this->warmup = warmup;
  if (!warmup)
    ++stat.accesses;

  item_ptr item_it = NULL;

  std::unordered_map<uint32_t, item_ptr>::iterator it_dram = dram_map.find(r->kid);
  if (it_dram != dram_map.end()) {
    item_it = std::static_pointer_cast<item>(it_dram->second);
    assert(item_it->in_dram);
  }
  auto it_flash = map.find(r->kid);
  if (it_flash != map.end()) {
    assert(it_dram == dram_map.end());
    item_it = std::static_pointer_cast<item>(it_flash->second);
    assert(!item_it->in_dram);
  }
  
  if (item_it) {
    if (item_it->req.size() == r->size()) {
      //HIT

      if (!warmup)
        ++stat.hits;

      if (item_it->in_dram) {
        //HIT in DRAM
        item_it->ds->remove(item_it);
        dram_map.erase(item_it->req.kid);
        if (item_it->ds->id == 0) {
          add(r, num_sections - 1); //allocate to flash  
        } else {
          dram_add(r, item_it->ds->id - 1); //allocate to higer queue in dram
        }
        if (!warmup)
          ++stat.hits_dram;
      } else {
        //HIT in FLASH
        //update virtual block
        section_ptr s = item_it->virtual_block->s;
        int new_section_id = (s->id > 0) ? (s->id - 1) : s->id;

        item_it->virtual_block->remove(item_it);
        if (item_it->physical_block->active) {
          assert(item_it->physical_block == item_it->virtual_block);
          block_ptr active_block = item_it->physical_block;
          item_list::iterator iter = std::find(active_block->items.begin(), active_block->items.end(), item_it);
          assert(iter != active_block->items.end());
          active_block->items.erase(iter);
          map.erase(r->kid); 
          add(r, new_section_id);
        } else {
          add_virtual(item_it, new_section_id);
        }

        if (!warmup)
          ++stat.hits_flash;
        assert(item_it->virtual_block->s->id <= item_it->physical_block->s->id);
      }
      return 1;
    } else {
      //UPDATE
      if (item_it->in_dram) {
        item_it->ds->remove(item_it);
        dram_map.erase(item_it->req.kid);
      } else {
        map.erase(item_it->req.kid);
        item_it->virtual_block->remove(item_it);
        item_it->virtual_block = item_it->physical_block;
        item_it->is_ghost = true;
        assert(item_it->virtual_block->s->id <= item_it->physical_block->s->id);
      }
      assert(stat.bytes_cached > (uint32_t)item_it->req.size());
      stat.bytes_cached -= (uint32_t)item_it->req.size();
    }
  } else {
    //MISS
  }
  stat.bytes_cached += (uint32_t)r->size();
  dram_add(r, num_dsections-1);

  return PROC_MISS;
}
