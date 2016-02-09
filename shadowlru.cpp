#include <cassert>

#include "shadowlru.h"

shadowlru::shadowlru(uint64_t size)
  : policy{size}
  , position_curve{}
  , size_curve{}
  , queue{}
{

}

shadowlru::~shadowlru() {
} 

void shadowlru::proc(const request *r, bool warmup) {
  assert(r->size() > 0);

  size_t position = ~0lu;
  size_t k = 0;
  uint64_t size_distance = 0;
  for (auto it = queue.begin(); it != queue.end(); ++it) {
    request& item = *it;
    k++;
    size_distance += item.size();
    if (item.kid == r->kid) {
      position = k;
      queue.erase(it);
      break;
    }
  }

  queue.emplace_front(*r);

  if (!warmup && position != ~0lu) {
    position_curve.hit(position);
    size_curve.hit(size_distance);
  }
}

uint32_t shadowlru::get_slab_class(uint32_t size) {
  if (size < 64)
    return 64;
  --size;
  size |= size >> 1;
  size |= size >> 2;
  size |= size >> 4;
  size |= size >> 8;
  size |= size >> 16;
  return size + 1;
}

void shadowlru::log_header() {
}

void shadowlru::log() {
  std::cout << "dist_is_size distance cumfrac" << std::endl;
  position_curve.dump_cdf(0);
  size_curve.dump_cdf(1);
}
