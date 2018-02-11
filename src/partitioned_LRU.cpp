#include <iostream>
#include <fstream>
#include <cassert>

#include "partitioned_LRU.h"

struct Partitioned_LRU::Partition
{
  Partition(const size_t& partition_size)
    : m_partition_size(partition_size)
    , m_partition_map{}
    , m_partition_queue{}
  {
  }
  
  bool try_put(const request* r)
  {
    return false;
  }

  bool try_get(const request* r)
  {
    return false;
  }

  private:
  size_t m_partition_size;
  std::unordered_map<uint32_t, std::list<request>::iterator> m_partition_map;
  std::list<request> m_partition_queue;
};

Partitioned_LRU::Partitioned_LRU(stats stat, const size_t& num_partitions)
  : policy{stat}
  , partitions{}
  , map{}
  , queue{}
{
}

Partitioned_LRU::~Partitioned_LRU () 
{
}

// checks the map for membership, if the key is found returns a hit, otherwise
// the key is added to the map and to the LRU queue and returns a miss. Returns
// absolute number of bytes added to the cache.
size_t Partitioned_LRU::proc(const request *r, bool warmup) 
{
  assert(r->size() > 0);

  if (stat.apps->empty())
  {
    stat.apps->insert(r->appid);
  }
  if (!warmup)
  {
    ++stat.accesses;
  }

  auto it = map.find(r->kid);
  if (it != map.end()) 
  {
    auto list_it = it->second;
    request& prior_request = *list_it;

    if (prior_request.size() == r->size() &&
        prior_request.frag_sz == r->frag_sz) 
    {
      // Promote this item to the front.
      queue.erase(list_it);
      queue.emplace_front(*r);
      map[r->kid] = queue.begin();

      if (!warmup)
        stat.hits++;

      return 1;
    } 
    else 
    {
      // If the size changed, then we have to put it in head. Just falling
      // through to the code below handles that. We still count this record as a
      // single hit, even though some miss would have had to have happened to
      // shoot down the old stale sized value. This is to be fair to other
      // policies like gLRU that don't detect these size changes.
      queue.erase(list_it);
      map.erase(prior_request.kid);
      stat.bytes_cached -= prior_request.size();
    }
  } 
  add(r);

  return PROC_MISS;
}

void Partitioned_LRU::add(const request *r) 
{
  auto it = map.find(r->kid);
  assert(it == map.end());

  // Throw out enough junk to make room for new record.
  while (stat.bytes_cached + size_t(r->size()) > stat.global_mem) 
  {
    // If the queue is already empty, then we are in trouble. The cache just
    // isn't big enough to hold this object under any circumstances.  Though,
    // you probably shouldn't be setting the cache size smaller than the max
    // memcache object size.
    if (queue.empty())
    {
      return;
    }

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

// Removes a request with matching key from the chain and updates bytes_cached
// accordingly.  Returns the 'stack distance' which is calc's by summing the
// bytes for all requests in the chain until 'r' is reached.
int64_t Partitioned_LRU::remove (const request *r) 
{
  // If r is not in the map something weird has happened.
  auto it = map.find(r->kid); 
  if(it == map.end())
  {
    return -1; 
  }

  // Sum the sizes of all requests up until we reach 'r'.
  size_t stack_dist  = 0;
  for ( const auto &i : queue ) 
  {
    if (i.kid != r->kid)
      break;
    stack_dist += i.size();
  }

  // Adjust the local bytes_cached value, remove 'r' from the hash table, and
  // from the LRU chain.
  auto& list_it = it->second;
  stat.bytes_cached -= list_it->size();
  queue.erase(list_it);
  map.erase(it);

  return stack_dist;
}

// Simply returns the current number of bytes cached.
size_t Partitioned_LRU::get_bytes_cached() const { return stat.bytes_cached; }

// Public accessors for hits and accesses.
size_t Partitioned_LRU::get_num_hits() 
{ 
  return stat.hits; 
}

size_t Partitioned_LRU::get_num_accesses() 
{ 
  return stat.accesses; 
}
