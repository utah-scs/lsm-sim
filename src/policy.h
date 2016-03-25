#ifndef POLICY_H
#define POLICY_H

#include <string>
#include "request.h"
#include "common.h"

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
    stats stat; 
    
  public:
    policy (stats stat)
      :stat{stat}
    {}

    virtual ~policy() {}

    enum { PROC_MISS = ~0lu };
    virtual size_t proc(const request *r, bool warmup) = 0;

    virtual size_t get_bytes_cached() const = 0; 

    void log_curves() { std::cout << "Not enabled for this policy" 
      << std::endl; }

    stats get_stats() { return stat; }

    void dump_stats(void) {
      std::ofstream out { stat.policy + stat.filename_suffix + ".data"};
      out << "app policy global_mem segment_size cleaning_width hits accesses "
             "hit_rate, bytes_cached, evicted_bytes, evicted_items,"
             "cleaned_bytes, cleaned_items" 
          << std::endl;
      out << stat.appid << " "
          << stat.policy << " "
          << stat.global_mem << " "
          << stat.segment_size << " "
          << stat.cleaning_width << " "
          << stat.hits << " "
          << stat.accesses << " "
          << double(stat.hits) / stat.accesses << " "
          << stat.bytes_cached << " "
          << stat.evicted_bytes << " "
          << stat.evicted_items << " "
          << stat.cleaned_bytes << " "
          << stat.cleaned_items << " "
          << std::endl;
    }
};


#endif
