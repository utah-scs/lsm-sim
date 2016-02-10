#include <cassert>

#include "shadowlru.h"

shadowlru::shadowlru()
  : policy{0}
  , position_curve{}
  , size_curve{}
  , queue{}
{
}

shadowlru::~shadowlru() {
} 

int64_t shadowlru::proc(const request *r, bool warmup) {
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

  return 0;
}

void shadowlru::log_header() {
}

void shadowlru::log() {
  position_curve.dump_cdf("shadowlru-position-curve.data");
  size_curve.dump_cdf("shadowlru-size-curve.data");
}
