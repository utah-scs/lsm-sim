#ifndef LRU_H
#define LRU_H

#include <unordered_map>
#include <list>
#include "policy.h"

class lru : public policy {
  typedef std::list<request> lru_queue; 

  typedef std::unordered_map<uint32_t, lru_queue::iterator> hash_map;

  public:
    lru(uint64_t size);
   ~lru();
    void proc(const request *r, bool warmup);
    uint32_t get_size(); 

    void log_header();
    void log();

  protected:
    // Number of access requests fed to the cache.
    size_t accesses;

    // Subset of accesses which hit in the simulated cache.
    size_t hits;

		// Accessors allowing derived classes to override
		// how hits/misses are recorded, possibly to a owner class.
		virtual void inc_hits(void)	{ ++hits; }
		virtual void inc_acss(void) { ++accesses; }
		
    uint32_t current_size;  // the current number of bytes in eviction queue

    hash_map hash; 
    lru_queue queue; 
};

#endif
