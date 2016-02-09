#ifndef SHADOWLRU_H
#define SHADOWLRU_H

#include <list>

#include "hit_rate_curve.h"
#include "policy.h"

// policy derived from Cliffhanger paper
class shadowlru : public policy {
  public:
    shadowlru(uint64_t size);
    ~shadowlru();
    void proc(const request *r, bool warmup);

    void log_header();
    void log();

  private:
    uint32_t get_slab_class(uint32_t size);

    hit_rate_curve position_curve;
    hit_rate_curve size_curve;
    std::list<request> queue;
};

#endif
