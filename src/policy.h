#ifndef POLICY_H
#define POLICY_H

#include "request.h"

// abstract base class for plug-and-play policies
class policy {
  struct dump {
    // let k = size of key in bytes
    // let v = size of value in bytes
    // let m = size of metadata in bytes
    // let g = global (total) memory

    double util_oh;     // k+v / g
    double util;        // k+v+m / g
    double ov_head;     // m / g
    double hit_rate;    // sum of hits in hits vect over size of hits vector
  };

  protected: 
    std::string filename_suffix;
    uint64_t global_mem;

  public:
    policy (const std::string& filename_suffix, const uint64_t global_mem)
      : filename_suffix{filename_suffix}
      , global_mem{global_mem}
    {}

    virtual ~policy() {}

    enum { PROC_MISS = ~0lu };
    virtual size_t proc(const request *r, bool warmup) = 0;

    virtual size_t get_bytes_cached() const = 0; 

    virtual void log() = 0;

    virtual double get_running_hit_rate() { return 0.; }
    virtual double get_running_utilization() { return 0.; }
    virtual size_t get_evicted_bytes() { return 0; }
    virtual size_t get_evicted_items() { return 0; }
};


#endif
