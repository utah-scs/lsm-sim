#ifndef PARTSLAB_H
#define PARTSLAB_H

#include <list>
#include <unordered_map>
#include "hit_rate_curve.h"
#include "policy.h"
#include "shadowlru.h"
#include "mc.h"

// policy derived from Cliffhanger paper
class partslab : public Policy {
  public:
 
    partslab(stats stat);
    ~partslab();

    size_t process_request(const Request *r, bool warmup);
    size_t get_bytes_cached() const;
    void log_curves();

    void dump_util(const std::string& filename);

  private:
    std::vector<shadowlru> slabs;
    hit_rate_curve size_curve;
};

#endif
