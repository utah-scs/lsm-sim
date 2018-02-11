#ifndef SLAB_H
#define SLAB_H

#include "policy.h"
#include "lru.h"

#include <vector>
#include <memory>
#include <unordered_map>

static const uint8_t  MIN_CHUNK=48;       // Memcached min chunk bytes
static const double   DEF_GFACT=1.25;     // Memcached growth factor
static const uint32_t PAGE_SIZE=1000000;  // Memcached page bytes
static const uint16_t MAX_CLASSES=256;    // Memcached max no of slabs   
static const size_t   MAX_SIZE=5000000;   // Largest KV pair allowed 

class slab : public Policy {
  public:
    slab(stats stat);
   ~slab();
    size_t process_request(const Request *r, bool warmup); 
    size_t get_bytes_cached() const;


  private:
    std::pair<uint64_t, uint64_t> get_slab_class(uint32_t size);

    static constexpr size_t SLABSIZE = 1024 * 1024;

    std::vector<lru> slabs; 

    // Simple mapping of existing keys to their respective slab.
    std::unordered_map<uint32_t, uint32_t> slab_for_key;

    uint32_t slab_count;

    uint64_t mem_in_use;

};

#endif
