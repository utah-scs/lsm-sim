#include <cassert>
#include <tuple>

#include "shadowslab.h"

shadowslab::shadowslab()
  : policy{0}
  , slabs{slab_count}
  , slabids{slab_count}
  , next_slabid{0}
  , size_curve{}
{
}

shadowslab::~shadowslab() {
} 

size_t shadowslab::proc(const request *r, bool warmup) {
  assert(r->size() > 0);

  uint32_t class_size = 0;
  uint32_t klass = 0;
  std::tie(class_size, klass) = get_slab_class(r->size());

  shadowlru& slab_class = slabs.at(klass);

  request copy{*r};
  copy.key_sz = 0;
  copy.val_sz = class_size;

  size_t size_distance = slab_class.proc(&copy, warmup);

  // Determine if we need to 'grow' the slab class by giving it more slabs.
  size_t max_slabid_index = slab_class.get_bytes_cached() / slab_size;
  std::vector<size_t>& class_ids = slabids.at(klass);
  while (class_ids.size() < max_slabid_index)
    class_ids.emplace_back(next_slabid++);

  // Figure out where in the space of slabids this access hit.
  size_t slabid_index = size_distance / slab_size;
  size_t slabid = class_ids.at(slabid_index);

  size_t approx_global_size_distance = slabid * slab_size;
  size_curve.hit(approx_global_size_distance);

  return 0;
}

size_t shadowslab::get_bytes_cached() {
  return 0;
}

std::pair<uint32_t, uint32_t> shadowslab::get_slab_class(uint32_t size) {
  uint32_t class_size = 64;
  uint32_t klass = 0;
  while (true) {
    if (size < class_size)
      return {class_size, klass};
    class_size <<= 1;
    ++klass;
  }
}

void shadowslab::log() {
  size_curve.dump_cdf("shadowslab-size-curve.data");
}