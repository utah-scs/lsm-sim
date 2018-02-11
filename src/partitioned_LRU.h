#ifndef Partitioned_LRU_H
#define Partitioned_LRU_H

#include <unordered_map>
#include <list>
#include <memory>
#include "policy.h"

class Partitioned_LRU : public policy 
{
public:

  // Models a single cache partition with hash table and eviction queue.
  struct Partition;
  
  Partitioned_LRU();
  Partitioned_LRU(stats stat, const size_t& num_partitions);
  ~Partitioned_LRU();

  size_t proc(const request* r, bool warmup);
  void add(const request* r);
  int64_t remove(const request* r);

  size_t get_bytes_cached() const;
  size_t get_num_hits(); 
  size_t get_num_accesses();

protected:
  std::unordered_map<uint32_t, std::unique_ptr<Partition>> partitions;
  std::unordered_map<uint32_t, std::list<request>::iterator> map;
  std::list<request> queue;
};

#endif
