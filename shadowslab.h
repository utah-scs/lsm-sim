#ifndef SHADOWSLAB_H
#define SHADOWSLAB_H

#include <list>

#include "hit_rate_curve.h"
#include "policy.h"
#include "shadowlru.h"

// policy derived from Cliffhanger paper
class shadowslab : public policy {
  public:
    static constexpr size_t slab_count = 15;
    static constexpr size_t slab_size = 1024 * 1024;

    shadowslab();
    ~shadowslab();

    size_t proc(const request *r, bool warmup);

    size_t get_bytes_cached();
    
    void log();

  private:
    std::pair<uint32_t, uint32_t> get_slab_class(uint32_t size);

    std::vector<shadowlru> slabs;

    // A vector of vectors of slabids. One vector for each slab with 0
    // or more slabids associated with that slab.
    std::vector<std::vector<uint64_t>> slabids;

    size_t next_slabid;

    hit_rate_curve size_curve;
};

#endif
