#ifndef Partitioned_LRU_H
#define Partitioned_LRU_H

#include <unordered_map>
#include <list>
#include <memory>
#include <vector>

#include "policy.h"

class Partitioned_LRU : public Policy 
{
  /// Models a single cache partition with hash table and eviction queue.
  struct Partition;
 
public:
  Partitioned_LRU();
  Partitioned_LRU(stats stat, 
                  const size_t& num_partitions, 
                  const size_t& max_overall_request_size);
  ~Partitioned_LRU();
  
  // Performs request key lookup by hashing into the corresponding partition
  // and performing key lookup within that partition. If an existing key is
  // found and the request size is equal with respect to the existing request,
  // the request is promoted to the front of the LRU chain for the partitions.
  // If an existing key is found and the size has changed the existing request
  // is removed and the new request is added to the front of the LRU chain and
  // treated as if it had not existed.
  // (conforms to Policy interface)
  //
  // @param request - a request object to be added to the cache.
  // @param warmup - a flag to indicate if the warmup period is still active.
  //
  // @return - an unsigned value representing the absolute value of change in
  // bytes due to processing this request. i.e. bytes_evicted - bytes_cached.
  size_t process_request(const Request* request, bool warmup);

  // Reports the current number of bytes utilized by cached requests.
  // (conforms to Policy interface)
  //
  // @return - unsigned value representing the number of bytes utilized by
  // cached requests.
  size_t get_bytes_cached() const;

private:
  size_t m_num_partitions;
  size_t m_max_overall_request_size;
  std::vector<std::unique_ptr<Partition>> m_p_partitions;
};

#endif
