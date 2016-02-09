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
    uint64_t global_mem;

  public:
    policy (const uint64_t g) : global_mem(g) {};
    virtual ~policy() {};
    virtual void proc(const request *r, bool warmup) = 0;

    virtual void log_header() = 0;
    virtual void log() = 0;
    // virtual get_dump () 
};


#endif
