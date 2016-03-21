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

class slab : public policy {
  public:
    slab(const std::string& filename_suffix,
         uint64_t size,
         double factor,
         bool memcachier_classes);
   ~slab();
    size_t proc(const request *r, bool warmup);
    void log();
    size_t get_bytes_cached();

  private:
  std::pair<uint64_t, uint64_t> get_slab_class(uint32_t size);

    static constexpr size_t SLABSIZE = 1024 * 1024;

    // Number of access requests fed to the cache.
    size_t accesses;

    // Subset of accesses which hit in the simulated cache.
    size_t hits;

    std::vector<lru> slabs; 

    // Simple mapping of existing keys to their respective slab.
    std::unordered_map<uint32_t, uint32_t> slab_for_key;

    // If true use memcachier size classes instead of memcacheds.
    bool memcachier_classes;

    uint32_t slab_count;

    uint64_t mem_in_use;

    uint32_t appid;
};

#endif
