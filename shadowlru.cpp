#include <cassert>

#include "shadowlru.h"

shadowlru::shadowlru()
  : policy{0}
  , bytes_cached{}
  , class_size{}
  , size_curve{}
  , queue{}
{
}

shadowlru::~shadowlru() {
}

// Removes an item from the chain and returns its
// distance in the chain (bytes).
int64_t shadowlru::remove(const request *r) {

  // Sum the sizes of all requests up until we reach 'r'.
  size_t stack_dist = 0;

  auto it = queue.begin();
  while (it++ != queue.end()) {
    if (it->kid == r->kid) {
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

  size_t size_distance = PROC_MISS;
  for (auto it = queue.begin(); it != queue.end(); ++it) {
    request& item = *it;
    size_distance += item.size();
    if (item.kid == r->kid) {
      bytes_cached -= item.size();
      queue.erase(it);
      break;
    }
  }

  bytes_cached += r->size();
  queue.emplace_front(*r);

  if (!warmup) {
    if (size_distance != PROC_MISS) {
      size_curve.hit(size_distance);
    } else {
      // Don't count compulsory misses.
      //size_curve.miss();
    }
  }

  return size_distance;
}

size_t shadowlru::get_bytes_cached() {
  return bytes_cached;
}

// Iterate over requests summing the associated overhead
// with each request. Upon reaching the end of each 1MB 
// page, stash the total page overhead into the vector.
std::vector<size_t> shadowlru::get_class_frags(size_t slab_size) const {
  size_t page_dist = 0, frag_sum = 0;
  std::vector<size_t> frags{};
  for (const request& r : queue) {
    // If within the current page just sum.
    // otherwise record OH for this page and reset sums
    // to reflect sums at first element of new page. 
    if (page_dist + r.size() > slab_size) {
      frags.push_back(frag_sum);
      page_dist = 0;
      frag_sum = 0;
    }
    page_dist += r.size();
    frag_sum += r.get_frag();
  }

  // Make sure to count possible final incomlete page missed above. 
  if (page_dist != 0) 
    frags.push_back(frag_sum);

  return frags; 
}


void shadowlru::log() {
  size_curve.dump_cdf("shadowlru-size-curve.data");
}
