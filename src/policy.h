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
      std::string appids{};
      for (const auto& app : stat.apps)
        appids += std::to_string(app) + ",";
      appids = appids.substr(0, appids.length() - 1);
      std::string filename{stat.policy
                          + "-app" + appids
                          + "-global_mem" + std::to_string(stat.global_mem)};
                             
      // Specific filename additions. 
      filename += stat.segment_size > 0 ? 
        "-segment_size" + std::to_string(stat.segment_size) : ""; 
      filename += stat.block_size > 0 ?
        "-block_size" + std::to_string(stat.block_size) : "";
      filename += stat.cleaning_width > 0 ?
        "-cleaning_width" + std::to_string(stat.cleaning_width) : "";
      filename += stat.gfactor > 0 ? 
        "-growth_factor" + to_string_with_precision(stat.gfactor) : "";
      filename += stat.partitions > 0 ?
        "-partitions" + std::to_string(stat.partitions) : "";  
      filename += stat.dram_size > 0 ?
        "-dram_size" + std::to_string(stat.dram_size) : "";
      filename += stat.num_dsections > 0 ?
        "-num_dsections" + std::to_string(stat.num_dsections) : "";
      filename += stat.flash_size > 0 ?
        "-flash_size" + std::to_string(stat.flash_size) : "";
      filename += stat.num_sections > 0 ?
        "-num_sections" + std::to_string(stat.num_sections) : "";
      filename += stat.threshold > 0 ?
	"-threshold" + std::to_string(stat.threshold) : "";
     
      filename += ".data";

      std::ofstream out{filename};
      stat.dump(out);
    }
};


#endif
