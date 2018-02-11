#include <cassert>

#include "shadowlru.h"

shadowlru::shadowlru()
  : Policy{{"", {}, 0}}
  , class_size{}
  , size_curve{}
  , queue{}
  , part_of_slab_allocator{true}
{
}

shadowlru::shadowlru(const stats& stat)
  : Policy{stat}
  , class_size{}
  , size_curve{}
  , queue{}
  , part_of_slab_allocator{false}
{
}

shadowlru::~shadowlru() {
}

// Removes an item from the chain and returns its
// distance in the chain (bytes).
size_t shadowlru::remove(const Request *r) {

  // Sum the sizes of all Requests up until we reach 'r'.
  size_t stack_dist = 0;

  auto it = queue.begin();
  while (it++ != queue.end()) {
    if (it->kid == r->kid) {
      stat.bytes_cached -= it->size();
      queue.erase(it); 
      break;
    } 
    stack_dist += it->size();
  }
  return stack_dist;
}

 
size_t shadowlru::process_request(const Request *r, bool warmup) {
  assert(r->size() > 0);

  size_t size_distance = PROC_MISS;
  for (auto it = queue.begin(); it != queue.end(); ++it) {
    Request& item = *it;
    size_distance += item.size();
    if (item.kid == r->kid) {
      stat.bytes_cached -= item.size();
      queue.erase(it);
      break;
    }
  }

  stat.bytes_cached += r->size();
  queue.emplace_front(*r);

  if (!warmup) {
    if (size_distance != PROC_MISS) {
      if (!part_of_slab_allocator)
        size_curve.hit(size_distance);
    } else {
      // Compulsory miss must have come before this new get hit.
      size_curve.miss();
    }
  }

  return size_distance;
}

size_t shadowlru::get_bytes_cached() const {
  return stat.bytes_cached;
}

// Iterate over Requests summing the associated overhead
// with each Request. Upon reaching the end of each 1MB 
// page, stash the total page overhead into the vector.
std::vector<size_t> shadowlru::get_class_frags(size_t slab_size) const {
  size_t page_dist = 0, frag_sum = 0;
  std::vector<size_t> frags{};
  for (const Request& r : queue) {
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


void shadowlru::log_curves() {

  std::string app_ids = "";

  for(auto &a : *stat.apps)
    app_ids += std::to_string(a);

  std::string filename_suffix{"-app" + app_ids
                             + (stat.memcachier_classes ?
                                 "-memcachier" : "-memcached")};
  size_curve.dump_cdf("shadowlru-size-curve" + filename_suffix + ".data");
}
