#include <cassert>

#include "shadowlru.h"

shadowlru::shadowlru()
  : policy{0}
  , bytes_cached{}
  , position_curve{}
  , size_curve{}
  , queue{}
{
}

shadowlru::~shadowlru() {
}

// Removes an item from the chain and returns its
// distance in the chain (bytes).
int64_t shadowlru::remove (const request *r) {

  // Sum the sizes of all requests up until we reach 'r'.
  size_t stack_dist = 0;

  auto it = queue.begin();
  while (it++ != queue.end()) {
    if( it->kid == r->kid) {
      bytes_cached -= it->size();
      queue.erase(it); 
      break;
    } 
    stack_dist += it->size();
  }
  return stack_dist;
}

 

size_t shadowlru::proc(const request *r, bool warmup) {
  assert(r->size() > 0);

  size_t position = PROC_MISS;
  size_t k = 0;
  size_t size_distance = PROC_MISS;
  for (auto it = queue.begin(); it != queue.end(); ++it) {
    request& item = *it;
    k++;
    size_distance += item.size();
    if (item.kid == r->kid) {
      position = k;
      bytes_cached -= item.size();
      queue.erase(it);
      break;
    }
  }

  bytes_cached += r->size();
  queue.emplace_front(*r);

  if (!warmup && position != PROC_MISS) {
    position_curve.hit(position);
    size_curve.hit(size_distance);
  }

  return size_distance;
}

size_t shadowlru::get_bytes_cached() {
  return bytes_cached;
}


void shadowlru::log() {
  position_curve.dump_cdf("shadowlru-position-curve.data");
  size_curve.dump_cdf("shadowlru-size-curve.data");
}
