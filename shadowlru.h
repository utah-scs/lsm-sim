#ifndef SHADOWLRU_H
#define SHADOWLRU_H

#include <list>

#include "hit_rate_curve.h"
#include "policy.h"

// policy derived from Cliffhanger paper
class shadowlru : public policy {
  public:
    shadowlru();
    ~shadowlru();
    int64_t proc(const request *r, bool warmup);
    size_t get_bytes_cached();
    void log();

    const hit_rate_curve* get_position_curve() const {
      return &position_curve;
    }

    const hit_rate_curve* get_size_curve() const {
      return &size_curve;
    }

  private:
    hit_rate_curve position_curve;
    hit_rate_curve size_curve;
    std::list<request> queue;
};

#endif
