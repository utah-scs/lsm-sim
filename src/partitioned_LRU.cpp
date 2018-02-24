#include <iostream>
#include <fstream>
#include <cassert>

#include "partitioned_LRU.h"
#include "openssl/sha.h"

struct Partitioned_LRU::Partition
{
  // Constructs a new Partition.
  //
  // @param partition_size - unsigned value indicating the size in bytes of this
  //                         partition.
  // @param r_stat         - statistics structure for reporting overall
  //                         statistics with respect to the entire cache. 
  Partition(const size_t& partition_size, stats& r_stat)
    : m_r_stats(r_stat)
    , m_partition_size(partition_size)
    , m_partition_bytes_cached(0)
    , m_partition_map{}
    , m_partition_queue{}
  {
  }

  virtual ~Partition()
  {
  }
  
  // Performs lookup of request within this partition.
  // 
  // If request already exists in cache and the existing request size is the
  // same with respect to the new request size, the existing request is promoted
  // to the front of the LRU chain and placed and the cache remains otherwise
  // unchanged.
  //
  // If the size has changed, the existing request is removed and the new
  // request is added to the cache and placed at the front of the LRU chain.
  //
  // If the request does not already exist in the cache, it is added and placed
  // at the front of the LRU queue.
  //
  // @param request - reference to a request object to be added to the cache.
  // @param - boolean flag indicating whether warmup period is still active.
  size_t process_request(const Request* p_request, const bool& warmup)
  {
    Request request = *p_request;
    assert(request.size() > 0);
    size_t absolute_bytes_added = 0;

    auto map_iterator = m_partition_map.find(request.kid);
    if (map_iterator != m_partition_map.end())
    {
      auto queue_iterator = map_iterator->second;
      Request& existing_request = *queue_iterator;
      if (existing_request.size() == request.size() &&
          existing_request.frag_sz == request.frag_sz)
      {
        m_partition_queue.erase(queue_iterator);
        m_partition_queue.emplace_front(request);
        m_partition_map[request.kid] = m_partition_queue.begin();
        if (!warmup)
        {
          m_r_stats.hits++;
        }
      }
      else
      {
        m_partition_queue.erase(queue_iterator); 
        m_partition_map.erase(existing_request.kid);
        m_r_stats.bytes_cached    -= existing_request.size();
        m_partition_bytes_cached  -= existing_request.size();
        absolute_bytes_added      = add_request(request);
      }
    }
    else
    {
      absolute_bytes_added = add_request(request);
    }
    return absolute_bytes_added;
  }

  // Adds a request to the cache within this partition. 
  //
  // If adequate space with respect to this partition to add the request does
  // not exist, existing requests are evicted from the cache until adequate
  // space is obtained. If the cache becomes empty before adequate space is
  // obtained, the request fails to be added to the cache.
  //
  // @param request - a request object to be added to the queue.
  size_t add_request(const Request& request)
  {
    size_t bytes_evicted  = 0;
    size_t bytes_added    = 0;
    if (static_cast<size_t>(request.size()) <= m_partition_size)
    {
      while (m_partition_bytes_cached + request.size() > m_partition_size 
          && !m_partition_queue.empty())
      {
        Request* eviction_candidate = &m_partition_queue.back();
        m_partition_queue.pop_back();
        m_partition_map.erase(eviction_candidate->kid);
        m_partition_bytes_cached  -= eviction_candidate->size();
        m_r_stats.bytes_cached    -= eviction_candidate->size();
        m_r_stats.evicted_bytes   += eviction_candidate->size();
        m_r_stats.evicted_items++;
      }
      if (m_partition_bytes_cached + request.size() <= m_partition_size)
      {
        m_partition_queue.emplace_front(request);
        m_partition_map[request.kid] = m_partition_queue.begin();
        m_partition_bytes_cached  += request.size();
        m_r_stats.bytes_cached    += request.size();
      }
      assert(m_partition_bytes_cached <= m_partition_size);
    }
    return bytes_evicted - bytes_added;
  }

private:
  stats& m_r_stats;
  const size_t m_partition_size;
  size_t m_partition_bytes_cached;
  std::unordered_map<uint32_t, std::list<Request>::iterator> m_partition_map;
  std::list<Request> m_partition_queue;
};

Partitioned_LRU::Partitioned_LRU(stats stat, 
                                 const size_t& num_partitions,
                                 const size_t& max_overall_request_size)
  : Policy{stat}
  , m_num_partitions{num_partitions}
  , m_max_overall_request_size(max_overall_request_size)
  , m_p_partitions{}
{
  size_t partition_size = stat.global_mem / m_num_partitions;
  m_p_partitions.reserve(num_partitions);
  for (size_t i = 0; i < num_partitions; ++i)
  {
    m_p_partitions[i] = std::make_unique<Partition>(partition_size, this->stat);
  }
}

Partitioned_LRU::~Partitioned_LRU () 
{
}

size_t Partitioned_LRU::process_request(const Request* p_request, bool warmup) 
{
  Request request = *p_request;
  assert(request.size() > 0);
  if (stat.apps->empty())
  {
    stat.apps->insert(request.appid);
  }
  if (!warmup)
  {
    ++stat.accesses;
  }
  size_t request_hash = p_request->hash_key(m_num_partitions);
  return m_p_partitions[request_hash]->process_request(p_request, warmup);
}

size_t Partitioned_LRU::get_bytes_cached() const 
{ 
  return stat.bytes_cached; 
}
