#ifndef POLICY_H
#define POLICY_H

#include <cassert>
#include <string>

#include "common.h"
#include "request.h"
#include "stats.h"

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

    stats* get_stats() { return &stat; }

    virtual void dump_stats(void) {
      assert(stat.apps.size() == 1);
      uint32_t appid = 0;
      for (const auto& app : stat.apps)
        appid = app;
      std::string filename{stat.policy
                          + "-app" + std::to_string(appid)
                          + "-global_mem" + std::to_string(stat.global_mem)};
                             
      // Specific filename additions. 
      filename += stat.segment_size > 0 ? 
        "-segment_size" + std::to_string(stat.segment_size) : ""; 
      filename += stat.cleaning_width > 0 ?
        "-cleaning_width" + std::to_string(stat.cleaning_width) : "";
      filename += stat.gfactor > 0 ? 
        "-growth_factor" + to_string_with_precision(stat.gfactor) : "";
      filename += stat.partitions > 0 ?
        "-partitions" + std::to_string(stat.partitions) : "";  

      filename += ".data";

      std::ofstream out{filename};
      stat.dump(out);
    }
};


#endif
