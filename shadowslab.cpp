#include <cassert>
#include <tuple>

#include "shadowslab.h"

shadowslab::shadowslab()
  : policy{0}
  , slabs{slab_count}
{
}

shadowslab::~shadowslab() {
} 

int64_t shadowslab::proc(const request *r, bool warmup) {
  assert(r->size() > 0);

  uint32_t class_size = 0;
  uint32_t klass = 0;
  std::tie(class_size, klass) = get_slab_class(r->size());

  request copy{*r};
  copy.key_sz = 0;
  copy.val_sz = class_size;

  slabs.at(klass).proc(&copy, warmup);
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

void shadowslab::log_header() {
}

void shadowslab::log() {
  {
    hit_rate_curve position_curve{};
    for (const shadowlru& slab : slabs)
      position_curve.merge(*slab.get_position_curve());
    position_curve.dump_cdf("shadowslab-position-curve.data");
  }

  {
    hit_rate_curve size_curve{};
    for (const shadowlru& slab : slabs)
      size_curve.merge(*slab.get_size_curve());
    size_curve.dump_cdf("shadowslab-size-curve.data");
  }
}
