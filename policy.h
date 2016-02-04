#ifndef POLICY_H
#define POLICY_H

#include <boost/intrusive/list.hpp>

#include "request.h"

namespace bi = boost::intrusive;

struct req_pair : public bi::list_base_hook<> {
  uint32_t id;
  uint32_t size;
  req_pair (uint16_t i, uint32_t s) : id (i), size(s) {} 
};

inline bool operator == (const req_pair &lhs, const req_pair &rhs) {
  return lhs.id == rhs.id;
}

typedef bi::list<req_pair, bi::constant_time_size<false>> queue;

struct delete_disposer {
  void operator()(req_pair *delete_this)
  { delete delete_this; }
};

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
    uint64_t global_queue_size;
    uint64_t global_mem;

  public:
    policy (const uint64_t g) : global_queue_size{}, global_mem(g) {};
    virtual ~policy() {};
    virtual bool proc (const request *r) = 0;
    virtual uint32_t get_size() = 0; 

    virtual void log_header() = 0;
    virtual void log() = 0;
    // virtual get_dump () 
};


#endif
