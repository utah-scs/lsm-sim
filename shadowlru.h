#ifndef SHADOWLRU_H
#define SHADOWLRU_H

#include <list>

#include "hit_rate_curve.h"
#include "policy.h"

// policy derived from Cliffhanger paper
class shadowlru : public policy {
  public:
    shadowlru(const std::string& filename_suffix = "");
    ~shadowlru();

    size_t proc(const request *r, bool warmup);
    int64_t remove(const request *r);
     
    size_t get_bytes_cached();
    std::vector<size_t> get_class_frags(size_t slab_size) const;
    void log();

    const hit_rate_curve* get_size_curve() const {
      return &size_curve;
    }

  private:
    size_t bytes_cached;
    size_t class_size;
    hit_rate_curve size_curve;
    std::list<request> queue;
    bool part_of_slab_allocator;
};

#endif
