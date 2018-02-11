#include <iostream>

#include "fifo.h"
#include "common.h"

fifo::fifo(stats stat)
  : Policy{stat}
  , current_size{}
  , hash{}
  , queue{}
{
}

fifo::~fifo () {
}

// checks the hashmap for membership, if the key is found
// returns a hit, otherwise the key is added to the hash
// and to the FIFO queue and returns a miss.
size_t fifo::process_request(const Request *r, bool warmup) {
  if (!warmup)
    ++stat.accesses;
  
  // Keep track of initial condition of cache.
  int64_t init_bytes = current_size;

  auto it = hash.find(r->kid);
  if (it != hash.end()) {
    Request* prior_request = it->second;
    if (prior_request->size() == r->size()) {
      if (!warmup)
        ++stat.hits;
      return 0;
    } else {
      // Size has changed. Even though it is in cache it must have already been
      // evicted or shotdown. Since then it must have already been replaced as
      // well. This means that there must have been some intervening get miss
      // for it. So we need to count an extra access here (but not an extra
      // hit). We do need to remove prior_Request from the hash table, but
      // it gets overwritten below anyway when r gets put in the cache.

      // Count the get miss that came between r and prior_Request.
      if (!warmup)
        ++stat.accesses;
      // Finally, we need to really put the correct sized value somewhere
      // in the FIFO queue. So fall through to the evict+insert clauses.
    }
  }

  // Throw out enough junk to make room for new record.
  while (stat.global_mem - current_size < uint32_t(r->size())) {
    // If the queue is already empty, then we are in trouble. The cache
    // just isn't big enough to hold this object under any circumstances.
    // Though, you probably shouldn't be setting the cache size smaller
    // than the max memcache object size.
    if (queue.empty())
      return 0;

    Request* victim = &queue.back();
    current_size -= victim->size();
    hash.erase(victim->kid);
    queue.pop_back();
  }

  // Add the new Request.
  queue.emplace_front(*r);
  hash[r->kid] = &queue.front();
  current_size += r->size();
 
  // Count this Request as a hit.
  if (!warmup)
    ++stat.hits;

  // Cache has been modified, return the difference.
  return current_size - init_bytes;
}

size_t fifo::get_bytes_cached() const {
  size_t cached = 0;
  for (const auto& r: queue)
    cached += r.size();
  return cached;
}
