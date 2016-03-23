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
    std::string filename_suffix;
   // uint64_t global_mem; 
    stats stat; 
    
  public:
    policy (const std::string& filename_suffix, const size_t global_mem, 
      stats stat)
      : filename_suffix{filename_suffix} 
      , stat{stat}
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

    void dump_stats(void) {
      std::ofstream out {"lsm" + filename_suffix + ".data"};
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
