#ifndef SLAB_H
#define SLAB_H

#include "policy.h"
#include <vector>
#include <memory>
#include <unordered_map>

static const uint8_t  MIN_CHUNK=48;       // Memcached min chunk bytes
static const double   DEF_GFACT=1.25;     // Memcached growth factor
static const uint32_t PAGE_SIZE=1000000;  // Memcached page bytes
static const uint16_t MAX_CLASSES=256;    // Memcached max no of slabs   
static const size_t   MAX_SIZE=5000000;   // Largest KV pair allowed 

class slab : public policy {
   
  public:
    slab(uint64_t size);
   ~slab();
    size_t proc(const request *r, bool warmup);
    void log();
    size_t get_bytes_cached();

  private:

    // Number of access requests fed to the cache.
    size_t accesses();

    // Subset of accesses which hit in the simulated cache.
    size_t hits(); 

    // Size in bytes of the entire allocated cache.
    size_t global_cache_size();

    // Chunk size growth factor.
    double growth;
	
    // The current number of bytes in eviction queue.
    size_t current_size; 

    // Represents a LRU eviction queue for a slab class.
    class sclru;

    typedef std::pair<uint32_t, uint32_t> ks_pr;
    typedef std::unique_ptr<sclru> sc_ptr;

    // Simple mapping of existing keys to their respective slab.
    std::unordered_map<uint32_t, uint32_t> slab_for_key;
 
    // Container for all slab class eviction queues.
    std::unordered_map<uint32_t, sc_ptr> slabs;

    // Given initial chunk size and growth factor initializes an sclru for each 
    // class up to max size or max number of classes, whichever comes first. 
    // Returns the index of the last slab class created as a bounds check for 
    // future accesses.
    uint16_t init_slabs (void); 
};

#endif
