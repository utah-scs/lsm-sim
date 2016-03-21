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
 
    static constexpr size_t SLABSIZE = 1024 * 1024;

    shadowslab(
        const std::string& filename_suffix,
        double factor,
        bool memcachier_classes);
    ~shadowslab();

    size_t proc(const request *r, bool warmup);
    size_t get_bytes_cached() const;
    void log();
    void dump_util(const std::string& filename);

  private:
    std::pair<uint64_t, uint64_t> get_slab_class(uint32_t size);
    std::pair<int32_t, int64_t> get_next_slab(uint32_t c);

    std::vector<shadowlru> slabs;

    // A vector of vectors of slabids. One vector for each slab with 0
    // or more slabids associated with that slab.
    std::vector<std::vector<uint64_t>> slabids;

    // Simple mapping of existing keys to their respective slab.
    std::unordered_map<uint32_t, uint32_t> slab_for_key;

    size_t next_slabid;

    hit_rate_curve size_curve;

    // If true use memcachier size classes instead of memcacheds.
    bool memcachier_classes;

    uint32_t slab_count;
};

#endif
