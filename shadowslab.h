#ifndef SHADOWSLAB_H
#define SHADOWSLAB_H

#include <list>
#include <unordered_map>
#include "hit_rate_curve.h"
#include "policy.h"
#include "shadowlru.h"
#include "mc.h"

// policy derived from Cliffhanger paper
class shadowslab : public policy {
  public:
 
    static constexpr size_t slab_size = 1024 * 1024;

    shadowslab(double factor, bool memcachier_classes);
    ~shadowslab();

    size_t proc(const request *r, bool warmup);

    size_t get_bytes_cached();
    
    void log();

  private:
    std::pair<uint64_t, uint64_t> get_slab_class(uint32_t size);

    std::vector<shadowlru> slabs;

    // A vector of vectors of slabids. One vector for each slab with 0
    // or more slabids associated with that slab.
    std::vector<std::vector<uint64_t>> slabids;

    // Simple mapping of existing keys to their respective slab.
    std::unordered_map<uint32_t, uint32_t> slab_for_key;

    size_t next_slabid;

    hit_rate_curve size_curve;

    bool memcachier_classes;

    uint32_t slab_count;
};

#endif
