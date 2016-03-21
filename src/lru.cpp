#include <iostream>
#include <fstream>
#include <cassert>

#include "lru.h"

lru::lru()
  : policy{"", 0}
  , accesses{}
  , hits{}
  , bytes_cached{}
  , hash{}
  , queue{}
  , appid{~0u}
{
}

lru::lru(const std::string& filename_suffix, uint64_t size)
  : policy{filename_suffix, size}
  , accesses{}
  , hits{}
  , bytes_cached{}
  , hash{}
  , queue{}
  , appid{~0u}
{
}

lru::~lru () {
}

// Simply returns the current number of bytes cached.
size_t lru::get_bytes_cached() const { return bytes_cached; }

// Public accessors for hits and accesses.
size_t lru::get_hits() { return hits; }
size_t lru::get_accs() { return accesses; }

// Removes a request with matching key from the chain and
// updates bytes_cached accordingly.
// Returns the 'stack distance' which is calc's by summing
// the bytes for all requests in the chain until 'r' is reached.
int64_t lru::remove (const request *r) {

  // If r is not in the hash something weird has happened.
  auto it = hash.find(r->kid); 
  if(it == hash.end())
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
  bytes_cached -= list_it->size();
  queue.erase(list_it);
  hash.erase(it);

  return stack_dist;
}


void lru::expand(size_t bytes) {
  global_mem += bytes;
}

// checks the hashmap for membership, if the key is found
// returns a hit, otherwise the key is added to the hash
// and to the LRU queue and returns a miss. Returns absolute
// number of bytes added to the cache.
size_t lru::proc(const request *r, bool warmup) {
  assert(r->size() > 0);

  if (appid == ~0u)
    appid = r->appid;
  assert(r->appid == appid);

  if (!warmup)
    ++accesses;

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

      return 1;
    } else {
      // If the size changed, then we have to put it in head. Just
      // falling through to the code below handles that. We still
      // count this record as a single hit, even though some miss
      // would have had to have happened to shootdown the old stale
      // sized value. This is to be fair to other policies like gLRU
      // that don't detect these size changes.
    }
  } else {
    // If miss in hash table, then count a miss. This get will count
    // as a second access that hits below.
  }

  // Throw out enough junk to make room for new record.
  while (global_mem - bytes_cached < uint32_t(r->size())) {
    // If the queue is already empty, then we are in trouble. The cache
    // just isn't big enough to hold this object under any circumstances.
    // Though, you probably shouldn't be setting the cache size smaller
    // than the max memcache object size.
    if (queue.empty())
      return PROC_MISS;

    request* victim = &queue.back();
    bytes_cached -= victim->size();
    hash.erase(victim->kid);
    queue.pop_back();
  }

  // Add the new request.
  queue.emplace_front(*r);
  hash[r->kid] = queue.begin();
  bytes_cached += r->size();
 
  return PROC_MISS;
}

void lru::log() {
  std::ofstream out{"lru" + filename_suffix + ".data"};
  out << "app policy global_mem segment_size cleaning_width hits accesses hit_rate"
      << std::endl;
  out << appid << " "
      << "lru" << " "
      << global_mem << " "
      << 0 << " "
      << 0 << " "
      << hits << " "
      << accesses << " "
      << double(hits) / accesses
      << std::endl;
}
