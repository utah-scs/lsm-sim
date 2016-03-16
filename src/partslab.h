#ifndef PARTSLAB_H
#define PARTSLAB_H

#include <list>
#include <unordered_map>
#include "hit_rate_curve.h"
#include "policy.h"
#include "shadowlru.h"
#include "mc.h"

// policy derived from Cliffhanger paper
class partslab : public policy {
  public:
 
    partslab(
        const std::string& filename_suffix,
        size_t partitions);
    ~partslab();

    size_t proc(const request *r, bool warmup);
    size_t get_bytes_cached();
    void log();

    void dump_util(const std::string& filename);

  private:
    size_t partitions;
    std::vector<shadowlru> slabs;
    hit_rate_curve size_curve;
};

#endif
