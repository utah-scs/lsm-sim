#include <cassert>
#include <tuple>

#include "shadowlru.h"
#include "shadowslab.h"
#include "mc.h"

shadowslab::shadowslab(double factor, bool memcachier_classes)
  : policy{0}
  , slabs{}
  , slabids{}
  , slab_for_key{}
  , next_slabid{0}
  , size_curve{}
  , memcachier_classes{memcachier_classes}
  , slab_count{}
{
  if (memcachier_classes)
    slab_count = 15;
  else
    slab_count = slabs_init(factor);
  slabs.resize(slab_count);
  slabids.resize(slab_count);

  if (memcachier_classes) {
    std::cerr << "Initialized with memcachier slab classes" << std::endl;
  } else {
    std::cerr << "Initialized with " << slab_count
              <<" slab classes" << std::endl;
  }
}

shadowslab::~shadowslab() {
} 

size_t shadowslab::proc(const request *r, bool warmup) {
  assert(r->size() > 0);

  uint64_t class_size = 0;
  uint64_t klass = 0;
  std::tie(class_size, klass) = get_slab_class(r->size());
  if (klass == PROC_MISS) {
    if (!warmup)
      size_curve.miss();
    return PROC_MISS;
  }

  // See if slab assignment already exists for this key.
  // Check if change in size (if any) requires reclassification
  // to a different slab. If so, remove from current slab.
  auto csit = slab_for_key.find(r->kid);

  if (csit != slab_for_key.end() && csit->second != klass) { 
    shadowlru& sclass = slabs.at(csit->second);
    sclass.remove(r);
    slab_for_key.erase(r->kid);
  }
 
  shadowlru& slab_class = slabs.at(klass);

  request copy{*r};
  copy.key_sz   = 0;
  copy.val_sz   = class_size;
  copy.frag_sz  = class_size - r->size();

  size_t size_distance = slab_class.proc(&copy, warmup);
  if (size_distance == PROC_MISS) {
    //if (!warmup)
      //size_curve.miss();
    return PROC_MISS;
  }

  // Proc didn't miss, re-validate key/slab pair.
  slab_for_key.insert(std::pair<uint32_t,uint32_t>(r->kid, klass));
  
  // Determine if we need to 'grow' the slab class by giving it more slabs.
  size_t max_slabid_index = slab_class.get_bytes_cached() / SLABSIZE; 
  std::vector<size_t>& class_ids = slabids.at(klass);
  while (class_ids.size() < max_slabid_index + 1)
    class_ids.emplace_back(next_slabid++);

  // Figure out where in the space of slabids this access hit.
  size_t slabid_index = size_distance / SLABSIZE;
  size_t slabid = class_ids.at(slabid_index);

  size_t approx_global_size_distance =
     (slabid * SLABSIZE) + (size_distance % SLABSIZE);
  if (!warmup)
    size_curve.hit(approx_global_size_distance);

  return 0;
}

size_t shadowslab::get_bytes_cached() {
  return 0;
}

std::pair<uint64_t, uint64_t> shadowslab::get_slab_class(uint32_t size) {
  uint64_t class_size = 64;
  uint64_t klass = 0;

  if (memcachier_classes) {
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

void shadowslab::log() {
  size_curve.dump_cdf("shadowslab-size-curve.data");
}


void shadowslab::dump_util () {

  


}
