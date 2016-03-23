#ifndef LRU_H
#define LRU_H

#include <unordered_map>
#include <list>
#include "policy.h"

class lru : public policy {
  typedef std::list<request> lru_queue; 

  typedef std::unordered_map<uint32_t, lru_queue::iterator> hash_map;

  public:
    lru();
    lru(const std::string& filename_suffix, size_t global_mem);
   ~lru();

    // Modifiers.
    size_t proc (const request *r, bool warmup);
    int64_t remove (const request *r);
    void expand(size_t bytes);
  
    // Accessors.
    size_t get_bytes_cached() const;
    size_t get_hits(); 
    size_t get_accs();
    void log();

    double get_running_hit_rate();
    double get_running_utilization();
    size_t get_evicted_bytes() { return stat.evicted_bytes; }
    size_t get_evicted_items() { return stat.evicted_items; }

  protected:
    // Number of access requests fed to the cache.
    // size_t accesses;

    // Subset of accesses which hit in the simulated cache.
    // size_t hits;

    // size_t evicted_bytes;
    //size_t evicted_items;

		// The number of bytes currently cached.
    // size_t bytes_cached; 

    hash_map map; 
    lru_queue queue; 

    // uint32_t appid;
};

#endif
