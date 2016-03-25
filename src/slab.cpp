#include <iostream>
#include <cassert>
#include <fstream>

#include "slab.h"
#include "lru.h"
#include "mc.h"

slab::slab(stats stat)
  : policy{stat}
  , slabs{}
  , slab_for_key{}
  , slab_count{}
  , mem_in_use{}
{
  if (stat.memcachier_classes)
    slab_count = 15;
  else
    slab_count = slabs_init(stat.gfactor);
  slabs.resize(slab_count);

  if (stat.memcachier_classes) {
    std::cerr << "Initialized with memcachier slab classes" << std::endl;
  } else {
    std::cerr << "Initialized with " << slab_count
              <<" slab classes" << std::endl;
  }
}

slab::~slab () {
}

size_t slab::proc(const request *r, bool warmup) {
  assert(r->size() > 0);

   if (stat.appid == ~0u)
    stat.appid = r->appid;
  assert(r->appid == stat.appid);

  if (!warmup)
    ++stat.accesses;

  uint64_t class_size = 0;
  uint64_t klass = 0;
  std::tie(class_size, klass) = get_slab_class(r->size());
  if (klass == PROC_MISS) {
    // We need to count these as misses in case different size to class
    // mappings don't all cover the same range of object sizes. If some
    // slab class configuration doesn't handle some subset of the acceses
    // it must penalized. In practice, memcachier and memcached's policies
    // cover the same range, so this shouldn't get invoked anyway.
    return PROC_MISS;
  }
  assert(klass < slabs.size());

  // See if slab assignment already exists for this key.
  // Check if change in size (if any) requires reclassification
  // to a different slab. If so, remove from current slab.
  auto csit = slab_for_key.find(r->kid);

  if (csit != slab_for_key.end() && csit->second != klass) { 
    lru& sclass = slabs.at(csit->second);
    sclass.remove(r);
    slab_for_key.erase(r->kid);
  }
 
  lru& slab_class = slabs.at(klass);

  // Round up the request size so the per-class LRU holds the right
  // size.
  request copy{*r};
  copy.key_sz   = 0;
  copy.val_sz   = class_size;
  copy.frag_sz  = class_size - r->size();

  size_t outcome = slab_class.proc(&copy, warmup);
  if (outcome == PROC_MISS) {
    // Expand class and retry.
    if (mem_in_use < stat.global_mem) {
      mem_in_use += SLABSIZE;
      slab_class.expand(SLABSIZE);
      outcome = slab_class.proc(&copy, warmup);
    }
    // Count compulsory misses.
    return PROC_MISS;
  }

  // Proc didn't miss, re-validate key/slab pair.
  slab_for_key.insert(std::pair<uint32_t,uint32_t>(r->kid, klass));

  if (!warmup)
    ++stat.hits;
 
  return 1;
}


size_t slab::get_bytes_cached() const {
  size_t b = 0;  
  for (const auto &s : slabs)
    b += s.get_bytes_cached(); 
  return b; 
}

void slab::log() {
  std::ofstream out{"slab" + stat.filename_suffix + ".data"};
  out << "app policy global_mem segment_size cleaning_width hits accesses hit_rate"
      << std::endl;
  out << stat.appid << " "
      << "slab" << " "
      << stat.global_mem << " "
      << 0 << " "
      << 0 << " "
      << stat.hits << " "
      << stat.accesses << " "
      << double(stat.hits) / stat.accesses
      << std::endl;
}

std::pair<uint64_t, uint64_t> slab::get_slab_class(uint32_t size) {
  uint64_t class_size = 64;
  uint64_t klass = 0;

  if (stat.memcachier_classes) {
    while (true) {
      if (size < class_size)
        return {class_size, klass};
      class_size <<= 1;
      ++klass;
      if (klass == slab_count) {
        return {PROC_MISS, PROC_MISS};
      }
    }
  } else {
    std::tie(class_size, klass) = slabs_clsid(size);
    // Object too big for chosen size classes.
    if (size > 0 && klass == 0)
      return {PROC_MISS, PROC_MISS};
    --klass;
    return {class_size, klass};
  }
}
