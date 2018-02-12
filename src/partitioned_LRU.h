#ifndef Partitioned_LRU_H
#define Partitioned_LRU_H

#include <unordered_map>
#include <list>
#include <memory>
#include <vector>

#include "policy.h"

class Partitioned_LRU : public Policy 
{
  // Models a single cache partition with hash table and eviction queue.
  struct Partition;
 
public:
  Partitioned_LRU();
  Partitioned_LRU(stats stat, const size_t& num_partitions);
  ~Partitioned_LRU();
  size_t process_request(const Request* process_request, bool warmup);
  size_t get_bytes_cached() const;

private:
  size_t m_num_partitions;
  /// Maps a request to a partition.
  std::vector<std::unique_ptr<Partition>> m_p_partitions;
};

#endif
