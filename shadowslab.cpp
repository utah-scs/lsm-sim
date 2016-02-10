#include <cassert>

#include "shadowslab.h"

shadowslab::shadowslab()
  : policy{0}
  , slabs{slab_count}
{
}

shadowslab::~shadowslab() {
} 

void shadowslab::proc(const request *r, bool warmup) {
  assert(r->size() > 0);

  uint32_t klass = get_slab_class(r->size());
  slabs.at(klass).proc(r, warmup);
}

uint32_t shadowslab::get_slab_class(uint32_t size) {
  uint32_t class_size = 64;
  uint32_t klass = 0;
  while (true) {
    if (size < class_size)
      return klass;
    class_size <<= 1;
    ++klass;
  }
}

void shadowslab::log_header() {
}

void shadowslab::log() {
  std::cout << "dist_is_size distance cumfrac" << std::endl;
  {
    hit_rate_curve position_curve{};
    for (const shadowlru& slab : slabs)
      position_curve.merge(*slab.get_position_curve());
    position_curve.dump_cdf(0);
  }

  {
    hit_rate_curve size_curve{};
    for (const shadowlru& slab : slabs)
      size_curve.merge(*slab.get_size_curve());
    size_curve.dump_cdf(1);
  }

}
