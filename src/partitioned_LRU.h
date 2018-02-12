#ifndef Partitioned_LRU_H
#define Partitioned_LRU_H

#include <unordered_map>
#include <list>
#include <memory>
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

protected:
  /// Maps a request to a partition.
  std::unordered_map<uint32_t, std::unique_ptr<Partition>> partitions;

  std::unordered_map<uint32_t, std::list<Request>::iterator> map;
  std::list<Request> queue;
};

#endif
