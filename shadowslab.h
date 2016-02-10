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

    shadowslab();
    ~shadowslab();
    int64_t proc(const request *r, bool warmup);

    void log();

  private:
    std::pair<uint32_t, uint32_t> get_slab_class(uint32_t size);

    std::vector<shadowlru> slabs;
};

#endif
