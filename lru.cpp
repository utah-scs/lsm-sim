#include <iostream>

#include "lru.h"

lru::lru(uint64_t size)
  : policy(size)
  , accesses{}
  , hits{}
  , current_size{}
  , hash{}
  , queue{}
{
}

lru::~lru () {
}

// checks the hashmap for membership, if the key is found
// returns a hit, otherwise the key is added to the hash
// and to the LRU queue and returns a miss. Returns absolute
// number of bytes added to the cache.
int64_t lru::proc(const request *r, bool warmup) {
  if (!warmup)
    inc_acss();

  // Keep track of initial condition of cache.
  int64_t bytes_init = current_size;
  
  auto it = hash.find(r->kid);
  if (it != hash.end()) {
    auto& list_it = it->second;
    request& prior_request = *list_it;
    if (prior_request.size() == r->size()) {
      if (!warmup)
        inc_hits();

      // Promote this item to the front.
      queue.emplace_front(prior_request);
      queue.erase(list_it);
      hash[r->kid] = queue.begin();

      return 0;
    } else {
      // Size has changed. Even though it is in cache it must have already been
      // evicted or shotdown. Since then it must have already been replaced as
      // well. This means that there must have been some intervening get miss
      // for it. So we need to count an extra access here (but not an extra
      // hit). We do need to remove prior_request from the hash table, but
      // it gets overwritten below anyway when r gets put in the cache.

      // Count the get miss that came between r and prior_request.
      inc_acss();
      // Finally, we need to really put the correct sized value somewhere
      // in the LRU queue. So fall through to the evict+insert clauses.
    }
  }

  // Throw out enough junk to make room for new record.
  while (global_mem - current_size < uint32_t(r->size())) {
    // If the queue is already empty, then we are in trouble. The cache
    // just isn't big enough to hold this object under any circumstances.
    // Though, you probably shouldn't be setting the cache size smaller
    // than the max memcache object size.
    if (queue.empty())
      return 0;

    request* victim = &queue.back();
    current_size -= victim->size();
    hash.erase(victim->kid);
    queue.pop_back();
  }

  // Add the new request.
  queue.emplace_front(*r);
  hash[r->kid] = queue.begin();
  current_size += r->size();
 
  // Count this request as a hit.
  if (!warmup)
    inc_hits();

  // Cache size has changed, return the difference.
  return current_size - bytes_init;
}

// Simply returns the current number of bytes cached.
uint32_t lru::get_size() {
  return current_size;
}

void lru::log_header() {
  std::cout << "util util_oh cachesize hitrate" << std::endl;
}

void lru::log() {
  std::cout << double(current_size) / global_mem << " "
            << double(current_size) / global_mem << " "
            << global_mem << " "
            << double(hits) / accesses << std::endl;
}
