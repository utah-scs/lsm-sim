#include "ripq.h"

static bool file_defined = false;
static double lastRequest = 0;

ripq::ripq(stats stat,size_t block_size,size_t num_sections,size_t flash_size)
  : Policy{stat}
  , sections{}
  , num_sections(num_sections)
  , section_size{}
  , warmup{}
  , map{}
  , out{}
{
  assert(block_size);
  assert(num_sections && flash_size);
  section_size = flash_size / num_sections;
  assert(section_size);
  
  for (uint32_t i = 0; i < num_sections; i++) {
    section_ptr curr_section(new section(i, stat));
    sections.push_back(curr_section);
    global_bid = 0;

    //initiate active blocks for each section
    block_ptr new_phy_block(new block(sections[i],false));
    sections[i]->active_phy_block = new_phy_block;

    block_ptr new_vir_block(new block(sections[i],true));
    sections[i]->active_vir_block = new_vir_block;
  }
}

ripq::~ripq () {
}

// Simply returns the current number of bytes cached.
size_t ripq::get_bytes_cached() const { return stat.bytes_cached; }

// Public accessors for hits and accesses.
size_t ripq::get_hits() { return stat.hits; }
size_t ripq::get_accs() { return stat.accesses; }

//add new Request to section_id
void ripq::add(const Request *r, int section_id) {

  //First make enough space in the target active physical block, then write
  section_ptr target_section = sections[section_id];
  while (target_section->active_phy_block->filled_bytes + r->size() > stat.block_size) {
    //Seal active blocks and write to flash 
    //We increase the writes stat by block_size, but write to flash only the real block size
    assert((uint32_t)r->size() < stat.block_size);
    if (!warmup)
      stat.flash_bytes_written += stat.block_size;
    target_section->seal_phy_block();
    target_section->seal_vir_block();
    balance(section_id);
    while (sections[num_sections-1]->filled_bytes > section_size) {
      evict();
    }
  }
  item_ptr new_item = target_section->add(r);

  map[r->kid] = new_item;
}

//add item to the active virtual block of section_id
void ripq::add_virtual(item_ptr it, int section_id) {
  section_ptr target_section = sections[section_id];

  //We first add the item to the virtual active block, then balance and evict. We 
  //use this order to avoid the case where we first make enough space - but then 
  //may evict an item which has no valid virtual block yet (i.e. we evict the 
  //pyshical block of the item before allocating a new virtual blockS
  target_section->add_virtual(it); 
  while (target_section->active_vir_block->filled_bytes/* + it->req.size()*/ > stat.block_size) {
    assert((uint32_t)it->req.size() < stat.block_size);
    target_section->seal_vir_block();
    balance(section_id);
    while (sections[num_sections-1]->filled_bytes > section_size) {
      evict();
    }
  }
}

void ripq::balance(int start) {
  for (uint32_t i = start; i < num_sections-1; i++) {
    while (sections[i]->filled_bytes  > section_size) {
      ripq::block_ptr evicted_block = sections[i]->evict_block();
      assert(evicted_block);
      sections[i+1]->add_block(evicted_block);
    }
  }
}

int ripq::section::count_filled_bytes() {
  int count = 0;
  for (block_list::const_iterator it = blocks.begin(); it != blocks.end(); it++) {
      if ((*it)->active) continue;
      count += (*it)->filled_bytes;
  }
  return count;
}

void ripq::evict() {
/*  for (uint32_t i = 0; i < num_sections; i++) {
    assert(sections[i]->count_filled_bytes() == (int)sections[i]->filled_bytes);
  }*/
  section_ptr tail_section = sections.back();
  assert(!tail_section->blocks.empty());

  //We first have to evict the block from the section, because the reallocations might evict blocks too
  block_ptr evicted_block = tail_section->evict_block();
  assert(!evicted_block->active);
  assert(!evicted_block->is_virtual || evicted_block->num_items == 0);
  assert(stat.bytes_cached > evicted_block->filled_bytes);
  stat.bytes_cached -= evicted_block->filled_bytes;

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

    item_ptr evicted_item = evicted_block->items.back();
    assert(evicted_item->physical_block == evicted_block);

    block_ptr virtual_block = evicted_item->virtual_block;
    assert(!evicted_block->active);

    //First pop the item out of the block
    evicted_block->items.pop_back(); 

    //Item can become a ghost if the key was updated with a new a value
    if (!evicted_item->is_ghost)
      map.erase(evicted_item->req.kid);

    //Check if we should reallocate in another section
    if (virtual_block != evicted_item->physical_block) { //reallocate in flash
      assert(!evicted_item->physical_block->is_virtual);
      assert(!evicted_item->is_ghost);
      add(&evicted_item->req, virtual_block->s->id);
/*    for (uint32_t i = 0; i < num_sections; i++) {
        assert(sections[i]->count_filled_bytes() == (int)sections[i]->filled_bytes);
      }*/
    }
  }
 
  balance();
}

size_t ripq::process_request(const Request *r, bool warmup) {
  assert(r->size() > 0);
  lastRequest = r->time;
  this->warmup = warmup;
  if (!warmup)
    ++stat.accesses;

  auto it = map.find(r->kid);

  if (it != map.end()) {

    auto item_it = it->second;

    if (item_it->req.size() == r->size()) {
      //HIT

      if (!warmup)
        ++stat.hits;

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

      assert(item_it->virtual_block->s->id <= item_it->physical_block->s->id);
      return 1;
    } else {
      //UPDATE 
      item_it->virtual_block->remove(item_it);
      item_it->virtual_block = item_it->physical_block;
      item_it->is_ghost = true;
      map.erase(r->kid);
      assert(item_it->virtual_block->s->id <= item_it->physical_block->s->id);
      assert(stat.bytes_cached > (uint32_t)item_it->req.size());
      stat.bytes_cached -= (uint32_t)item_it->req.size();
    }
  } else {
    //MISS
  }
  stat.bytes_cached += (uint32_t)r->size();
  if (!warmup) {
      stat.missed_bytes += (size_t)r->size();
  }

  add(r, num_sections-1);
  return PROC_MISS;
}

ripq::item_ptr ripq::block::add(const Request *req) {
  assert(active);
  filled_bytes += req->size();
  num_items++;
  item_ptr new_item(new item(*req, shared_from_this())); //Items are added also to virtual sections for debug purposes
  items.push_front(new_item);
  return items.front();
}

void ripq::block::remove(item_ptr victim) {
//  assert(s->count_filled_bytes() == (int)s->filled_bytes);
  assert((int)filled_bytes >= (int)victim->req.size());
  assert(num_items>0);
  filled_bytes -= victim->req.size();
  num_items -= 1;
  if (is_virtual) { 
    item_list::iterator iter = std::find(items.begin(), items.end(), victim);
    assert(iter != items.end());
    items.erase(iter);
  }

  if (!active) { //decrease section size only if the Request belongs to a sealed block
    assert((int)s->filled_bytes >= (int)victim->req.size());
    s->filled_bytes -= victim->req.size();
  }
}

void ripq::section::seal_phy_block() {
  active_phy_block->active = false;
  filled_bytes += active_phy_block->filled_bytes; //We count the block filled bytes only when it becomes sealed
  block_ptr new_phy_block(new block(shared_from_this(),false));
  blocks.push_front(active_phy_block);
  active_phy_block = new_phy_block;
}

void ripq::section::seal_vir_block() {
  active_vir_block->active = false;
  block_ptr new_vir_block(new block(shared_from_this(), true));
  filled_bytes += active_vir_block->filled_bytes;
  blocks.push_front(active_vir_block);
  active_vir_block = new_vir_block;
}

ripq::item_ptr ripq::section::add(const Request *req) {
  //If physical block is big enough, seal it together with the virtual block and commit to section block list
  assert(active_phy_block && active_vir_block && active_vir_block->is_virtual);
  assert(active_phy_block->filled_bytes <= stat.block_size);
  assert(active_phy_block->filled_bytes + (uint32_t)req->size() <= stat.block_size);

  //Add Request to physical block
  return active_phy_block->add(req);
}

void ripq::section::add_virtual(item_ptr it) {
  active_vir_block->filled_bytes += it->req.size();
  active_vir_block->num_items++;
  active_vir_block->items.push_front(it); //Items are added also to virtual sections for debug purposes
  it->virtual_block = active_vir_block;
//  assert(count_filled_bytes() == (int)filled_bytes);
}

void ripq::section::add_block(block_ptr new_block) {
  assert(!new_block->active);
  filled_bytes += new_block->filled_bytes;
  new_block->s = shared_from_this();

  blocks.push_front(new_block);
}

ripq::block_ptr ripq::section::evict_block() {
  assert(!blocks.empty());
  block_ptr last_block = blocks.back();
  assert(!last_block->active);
  assert (filled_bytes >= last_block->filled_bytes);
  filled_bytes -= last_block->filled_bytes;
  blocks.pop_back();
  return last_block;
}

void ripq::dump_stats(void) {
	std::string filename{stat.policy
		+ "-block_size" + std::to_string(stat.block_size)
		+ "-flash_size" + std::to_string(stat.flash_size)
		+ "-num_sections" + std::to_string(stat.num_sections)};
	if(!file_defined) {
		out.open(filename);
		file_defined = true;
	}
	out << "Last Request was at :" << lastRequest << std::endl;
	stat.dump(out);
	out << std::endl;
}
