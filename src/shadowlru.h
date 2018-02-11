#ifndef SHADOWLRU_H
#define SHADOWLRU_H

#include <list>

#include "hit_rate_curve.h"
#include "policy.h"

// Policy derived from Cliffhanger paper
class shadowlru : public Policy {
  public:
    shadowlru();
    shadowlru(const stats& stat);
    ~shadowlru();

    size_t process_request(const Request *r, bool warmup);
    size_t remove(const Request *r);
     
    size_t get_bytes_cached() const;
    std::vector<size_t> get_class_frags(size_t slab_size) const;
    
    const hit_rate_curve* get_size_curve() const {
      return &size_curve;
    }

    void log_curves();

  private:    
    size_t class_size;
    hit_rate_curve size_curve;
    std::list<Request> queue;
    bool part_of_slab_allocator;
};

#endif
