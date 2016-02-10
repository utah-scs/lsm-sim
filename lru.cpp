#include <iostream>

#include "lru.h"

lru::lru(uint64_t size)
  : policy(size)
  , accesses{}
  , hits{}
  , bytes_cached{}
  , hash{}
  , queue{}
{
}

lru::~lru () {
}

// Simply returns the current number of bytes cached.
size_t lru::get_bytes_cached() { return bytes_cached; }

// Public accessors for hits and accesses.
size_t lru::get_hits() { return hits; }
size_t lru::get_accs() { return accesses; }


// checks the hashmap for membership, if the key is found
// returns a hit, otherwise the key is added to the hash
// and to the LRU queue and returns a miss. Returns absolute
// number of bytes added to the cache.
size_t lru::proc(const request *r, bool warmup) {
  if (!warmup)
    ++accesses;

  // Keep track of initial condition of cache.
  int64_t bytes_init = bytes_cached;
  
  auto it = hash.find(r->kid);
  if (it != hash.end()) {
    auto& list_it = it->second;
    request& prior_request = *list_it;
    if (prior_request.size() == r->size()) {
      if (!warmup)
        ++hits;

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
      ++accesses;      
      // Finally, we need to really put the correct sized value somewhere
      // in the LRU queue. So fall through to the evict+insert clauses.
    }
  }

  // Throw out enough junk to make room for new record.
  while (global_mem - bytes_cached < uint32_t(r->size())) {
    // If the queue is already empty, then we are in trouble. The cache
    // just isn't big enough to hold this object under any circumstances.
    // Though, you probably shouldn't be setting the cache size smaller
    // than the max memcache object size.
    if (queue.empty())
      return 0;

    request* victim = &queue.back();
    bytes_cached -= victim->size();
    hash.erase(victim->kid);
    queue.pop_back();
  }

  // Add the new request.
  queue.emplace_front(*r);
  hash[r->kid] = queue.begin();
  bytes_cached += r->size();
 
  // Count this request as a hit.
  if (!warmup)
    ++hits;

  // Cache size has changed, return the difference.
  return bytes_cached - bytes_init;
}

void lru::log() {
  std::cout << double(bytes_cached) / global_mem << " "
            << double(bytes_cached) / global_mem << " "
            << global_mem << " "
            << double(hits) / accesses << std::endl;
}
