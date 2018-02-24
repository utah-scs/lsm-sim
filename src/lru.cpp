#include <iostream>
#include <fstream>
#include <cassert>

#include "lru.h"

LRU::LRU()
  : Policy{stats{"",{},0}}
  , map{}
  , queue{}
{
  stat.apps = {};
}  

LRU::LRU(stats stat)
  : Policy{stat}
  , map{}
  , queue{}
{
}

LRU::~LRU () 
{
}

size_t LRU::process_request(const Request *r, bool warmup) 
{
  Request request = *r;
  assert(request.size() > 0);
  size_t absolute_bytes_added = 0;

  if (stat.apps->empty())
  {
    stat.apps->insert(request.appid);
  }
  if (!warmup)
  {
    ++stat.accesses;
  }
  auto map_iterator = map.find(request.kid);
  if (map_iterator != map.end()) 
  {
    auto queue_iterator = map_iterator->second;
    Request& existing_request = *queue_iterator;
    if (existing_request.size() == request.size() && 
        existing_request.frag_sz == request.frag_sz) 
    {
      queue.erase(queue_iterator);
      queue.emplace_front(request);
      map[request.kid] = queue.begin();
      if (!warmup)
      {
        stat.hits++;
      }
    } 
    else 
    {
      queue.erase(queue_iterator);
      map.erase(existing_request.kid);
      stat.bytes_cached -= existing_request.size();
      absolute_bytes_added = add_request(request);
    }
  } 
  else
  {
    absolute_bytes_added = add_request(request);
  }

  return absolute_bytes_added;
}

size_t LRU::get_bytes_cached() const 
{ 
  return stat.bytes_cached; 
}

size_t LRU::add_request(const Request& request) 
{
  auto it = map.find(request.kid);
  assert(it == map.end());
  size_t bytes_evicted        = 0;
  size_t bytes_added          = 0;
  while (stat.bytes_cached + size_t(request.size()) > 
         stat.global_mem && !queue.empty()) 
  {
    Request* victim    = &queue.back();
    stat.bytes_cached  -= victim->size();
    stat.evicted_bytes += victim->size();
    bytes_evicted      += victim->size();
    ++stat.evicted_items;
    map.erase(victim->kid);
    queue.pop_back();
  }
  if (stat.bytes_cached + size_t(request.size()) <= stat.global_mem)
  {
    queue.emplace_front(request);
    map[request.kid]  = queue.begin();
    stat.bytes_cached += request.size();
    bytes_added       += request.size();
  }
  return bytes_evicted - bytes_added;
}

bool LRU::would_cause_eviction(const Request& request) const
{
  return (map.find(request.kid) == map.end()) &&
         (stat.bytes_cached + size_t(request.size()) > stat.global_mem);
}

bool LRU::would_hit(const Request& request) const
{
  auto it = map.find(request.kid);
  return it != map.end();
}

void LRU::expand(const size_t bytes) 
{
  stat.global_mem += bytes;
}

std::unordered_map<int32_t, size_t> LRU::get_per_app_bytes_in_use() const
{
  std::unordered_map<int32_t, size_t> result{};
  for (auto& Request : queue)
  {
    result[Request.appid] += Request.size();
  }
  return result;
}
 
bool LRU::try_add_tail(const Request *r) 
{
  auto it = map.find(r->kid);
  assert(it == map.end());
  bool succeeded = false;
  if (stat.bytes_cached + size_t(r->size()) <= stat.global_mem)
  {
    queue.emplace_back(*r);
    auto back_it = queue.end();
    --back_it;
    map[r->kid] = back_it;
    stat.bytes_cached += r->size();
    succeeded = true;
  }
  return succeeded;
}

// Removes a Request with matching key from the chain and updates bytes_cached
// accordingly.  Returns the 'stack distance' which is calc's by summing the
// bytes for all Requests in the chain until 'r' is reached.
int64_t LRU::remove (const Request *r) 
{
  // If r is not in the map something weird has happened.
  auto it = map.find(r->kid); 
  if(it == map.end())
  {
    return -1; 
  }

  // Sum the sizes of all Requests up until we reach 'r'.
  size_t stack_dist  = 0;
  for ( const auto &i : queue ) 
  {
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
