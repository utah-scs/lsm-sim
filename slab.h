#ifndef SLAB_H
#define SLAB_H

#include "policy.h"
#include <unordered_map>

class slab : public policy {
  
  public:
    slab(uint64_t size);
   ~slab();
    void proc(const request *r);
    uint32_t get_size(); 

    void log_header();
    void log();

  private:
    // Number of access requests fed to the cache.
    size_t accesses;

    // Subset of accesses which hit in the simulated cache.
    size_t hits;

    // The number of slab classes.
    size_t slabs;
			
		// the current number of bytes in eviction queue
    uint32_t current_size; 
     
};

#endif
