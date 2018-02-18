#include <iostream>
#include <fstream>
#include <cassert>

#include "partitioned_LRU.h"
#include "openssl/sha.h"

struct Partitioned_LRU::Partition
{
  Partition(const size_t& partition_size, stats& r_stat)
    : m_r_stats(r_stat)
    , m_partition_size(partition_size)
    , m_partition_bytes_cached(0)
    , m_partition_map{}
    , m_partition_queue{}
  {
  }

  size_t promote_request_payload(const Request* request)
  {
    return 0;

  }

  size_t remove_request_payload(const Request* request)
  {
    return 0;
  }

  bool lookup_request(const Request* request)
  {
    return 0;
  }
  
  // Places a new item into the cache partition. Performs object eviction until
  // adequate space exists to accomodate the new request. This function should
  // always succeed (cache semantics).
  size_t add_request_payload(const Request* request)
  {
    //auto it = m_partition_map.find(request->kid);
    //assert(it == m_partition_map.end());

    size_t bytes_evicted  = 0;
    size_t bytes_added    = 0;

    while (m_partition_bytes_cached + request->size() > m_partition_size)
    {
      Request* eviction_candidate = &m_partition_queue.back();
      m_partition_map.erase(eviction_candidate->kid);
      m_partition_queue.pop_back();

      m_partition_bytes_cached -= eviction_candidate->size();
      m_r_stats.bytes_cached -= eviction_candidate->size();
      m_r_stats.evicted_bytes += eviction_candidate->size();
      m_r_stats.evicted_items++;
    }

    m_partition_queue.emplace_front(*request);
    m_partition_map[request->kid] = m_partition_queue.begin();
    m_partition_bytes_cached += request->size();
    m_r_stats.bytes_cached += request->size();

    return bytes_evicted - bytes_added;
  }

  private:
  stats& m_r_stats;
  const size_t m_partition_size;
  size_t m_partition_bytes_cached;
  std::unordered_map<uint32_t, std::list<Request>::iterator> m_partition_map;
  std::list<Request> m_partition_queue;
};

Partitioned_LRU::Partitioned_LRU(stats stat, const size_t& num_partitions)
  : Policy{stat}
  , m_num_partitions{num_partitions}
  , m_p_partitions{}
{
  size_t partition_size = stat.global_mem / m_num_partitions;
  m_p_partitions.reserve(num_partitions);
  for (size_t i = 0; i < num_partitions; ++i)
  {
    m_p_partitions[i] = std::make_unique<Partition>(partition_size, stat);
  }
}

Partitioned_LRU::~Partitioned_LRU () 
{
}

// Performs request key lookup in partitions map. If found, retrieves the
// partition that potentially contains the matching request and performs lookup
// in the internal partition map. If the request is found it is moved to the
// front of the queue within its paritition.
size_t Partitioned_LRU::process_request(const Request* request, bool warmup) 
{
  size_t absolute_bytes_added = 0;

  if (stat.apps->count(request->appid) == 0)
  {
    stat.apps->insert(request->appid);
  }

  if (!warmup)
  {
    ++stat.accesses;
  }
  
  auto& partition = m_p_partitions[request->hash_key(m_num_partitions)];
  size_t existing_size = partition->lookup_request(request);
  if (existing_size > 0)
  {
    if ((size_t)request->size() > existing_size)
    {
      partition->remove_request_payload(request);
      partition->add_request_payload(request);
    }
    else
    {
      partition->promote_request_payload(request);
    }
  }
  else
  {
    partition->add_request_payload(request);
  }

  return absolute_bytes_added;
}

// Simply returns the current number of bytes cached.
size_t Partitioned_LRU::get_bytes_cached() const 
{ 
  return stat.bytes_cached; 
}
