#include <iostream>
#include <fstream>
#include <cassert>

#include "lru.h"

lru::lru()
  : policy{stats{"",{},0}}
  , map{}
  , queue{}
{
  stat.apps = {};
}  

lru::lru(stats stat)
  : policy{stat}
  , map{}
  , queue{}
{
}

lru::~lru () {
}

// Simply returns the current number of bytes cached.
size_t lru::get_bytes_cached() const { return stat.bytes_cached; }

// Public accessors for hits and accesses.
size_t lru::get_hits() { return stat.hits; }
size_t lru::get_accs() { return stat.accesses; }

// Removes a request with matching key from the chain and
// updates bytes_cached accordingly.
// Returns the 'stack distance' which is calc's by summing
// the bytes for all requests in the chain until 'r' is reached.
int64_t lru::remove (const request *r) {

  // If r is not in the map something weird has happened.
  auto it = map.find(r->kid); 
  if(it == map.end())
    return -1; 

  // Sum the sizes of all requests up until we reach 'r'.
  size_t stack_dist  = 0;
  for ( const auto &i : queue ) {
    if (i.kid != r->kid)
      break;
    stack_dist += i.size();
  }

  // Adjust the local bytes_cached value, remove 'r'
  // from the hash table, and from the LRU chain.
  auto& list_it = it->second;
  stat.bytes_cached -= list_it->size();
  queue.erase(list_it);
  map.erase(it);

  return stack_dist;
}


void lru::expand(size_t bytes) {
  stat.global_mem += bytes;
}

bool lru::would_cause_eviction(const request* r) {
  return (map.find(r->kid) == map.end()) &&
         (stat.bytes_cached + size_t(r->size()) > stat.global_mem);
}

bool lru::would_hit(const request *r) {
  auto it = map.find(r->kid);
  return it != map.end();
}

void lru::add(const request *r) {
  auto it = map.find(r->kid);
  assert(it == map.end());

  // Throw out enough junk to make room for new record.
  while (stat.bytes_cached + size_t(r->size()) > stat.global_mem) {
    // If the queue is already empty, then we are in trouble. The cache
    // just isn't big enough to hold this object under any circumstances.
    // Though, you probably shouldn't be setting the cache size smaller
    // than the max memcache object size.
    //
    // This case can occur when an lru is part of a slab allocator, since
    // initially all lrus start with 0 bytes and expand as the slab pool
    // allows.
    if (queue.empty())
      return;

    request* victim = &queue.back();
    stat.bytes_cached -= victim->size();
    ++stat.evicted_items;
    stat.evicted_bytes += victim->size();
    map.erase(victim->kid);
    queue.pop_back();
  }

  // Add the new request.
  queue.emplace_front(*r);
  map[r->kid] = queue.begin();
  stat.bytes_cached += r->size();
}

bool lru::try_add_tail(const request *r) {
  auto it = map.find(r->kid);
  assert(it == map.end());

  if (stat.bytes_cached + size_t(r->size()) > stat.global_mem)
    return false;

  // Add the new request.
  queue.emplace_back(*r);
  auto back_it = queue.end();
  --back_it;
  map[r->kid] = back_it;
  stat.bytes_cached += r->size();

  return true;
}

// checks the map for membership, if the key is found
// returns a hit, otherwise the key is added to the map
// and to the LRU queue and returns a miss. Returns absolute
// number of bytes added to the cache.
size_t lru::proc(const request *r, bool warmup) {
  assert(r->size() > 0);

  if (stat.apps.empty())
    stat.apps.insert(r->appid);
  //assert(stat.apps.count(r->appid) == 1);

  if (!warmup)
    ++stat.accesses;

  auto it = map.find(r->kid);
  if (it != map.end()) {
    auto list_it = it->second;
    request& prior_request = *list_it;

    if (prior_request.size() == r->size()) {
      // Promote this item to the front.
      queue.erase(list_it);
      queue.emplace_front(*r);
      map[r->kid] = queue.begin();
      if (!warmup) {stat.hits++; }
      return 1;
    } else {
      // If the size changed, then we have to put it in head. Just
      // falling through to the code below handles that. We still
      // count this record as a single hit, even though some miss
      // would have had to have happened to shootdown the old stale
      // sized value. This is to be fair to other policies like gLRU
      // that don't detect these size changes.

      queue.erase(list_it);
      map.erase(prior_request.kid);
      stat.bytes_cached -= prior_request.size();
    }
  } else {
    // If miss in hash table, then count a miss. This get will count
    // as a second access that hits below.
  }

  add(r);

  return PROC_MISS;
}

double lru::get_running_hit_rate() {
  return double(stat.hits) / stat.accesses;
}

double lru::get_running_utilization() {
  size_t in_use = 0;
  for (auto& request : queue)
    in_use += request.size();

  assert(in_use == stat.bytes_cached);

  return double(in_use) / stat.global_mem;
}

std::unordered_map<int32_t, size_t> lru::get_per_app_bytes_in_use() const
{
  std::unordered_map<int32_t, size_t> result{};
  for (auto& request : queue)
    result[request.appid] += request.size();

  return result;
}
