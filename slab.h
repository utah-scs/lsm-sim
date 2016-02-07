#ifndef SLAB_H
#define SLAB_H

#include "policy.h"
#include <vector>
#include <unordered_map>

#define MIN_CHUNK 48    // Default memcached minimum chunk size
#define DEF_GFACT 1.25  // Default memcached growth factor
#define PAGE_SIZE 1024  // Deafult memcached page allocation size

class slab : public policy {
  typedef std::list<request> lru_queue;
 
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

    // Chunk size growth factor.
    double growth;
	
    // The current number of bytes in eviction queue.
    uint32_t current_size; 

    // Represents a LRU eviction queue for a slab class.
    class sclru;

    // Container for all slab class eviction queues, indexed
    // by class size. 

    std::unordered_map<uint32_t, sclru*> slabs; 
};

#endif
