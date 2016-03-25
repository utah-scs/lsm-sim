#ifndef SHADOWLRU_H
#define SHADOWLRU_H

#include <list>

#include "hit_rate_curve.h"
#include "policy.h"

// policy derived from Cliffhanger paper
class shadowlru : public policy {
  public:
    shadowlru(const stats stat = stats{"","",0,0});
    ~shadowlru();

    size_t proc(const request *r, bool warmup);
    size_t remove(const request *r);
     
    size_t get_bytes_cached() const;
    std::vector<size_t> get_class_frags(size_t slab_size) const;
    void log();

    const hit_rate_curve* get_size_curve() const {
      return &size_curve;
    }

  private:    
    size_t class_size;
    hit_rate_curve size_curve;
    std::list<request> queue;
    bool part_of_slab_allocator;
};

#endif
