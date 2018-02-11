#include <cassert>
#include <tuple>

#include "common.h"
#include "shadowlru.h"
#include "partslab.h"
#include "mc.h"

partslab::partslab(stats stat)
  : Policy{stat}
  , slabs{}
  , size_curve{}
{
  slabs.resize(stat.partitions);
  std::cerr << "Initialized with " << stat.partitions << 
  " partitions" << std::endl;
}

partslab::~partslab() {
} 

size_t partslab::process_request(const Request *r, bool warmup) {
  assert(r->size() > 0);

  size_t klass = std::hash<decltype(r->kid)>{}(r->kid) % stat.partitions;
  shadowlru& slab_class = slabs.at(klass);

  size_t size_distance = slab_class.process_request(r, warmup);
  if (size_distance == PROC_MISS) {
    // Don't count compulsory misses.
    //if (!warmup)
      //size_curve.miss();
    return PROC_MISS;
  }

  const size_t approx_global_size_distance = size_distance * 
    stat.partitions + klass;
  if (!warmup)
    size_curve.hit(approx_global_size_distance);

  return 0;
}

size_t partslab::get_bytes_cached() const {
  return 0;
}

void partslab::log_curves() {

  std::string app_ids = "";

  for(auto &a : *stat.apps)
    app_ids += std::to_string(a);

  std::string filename_suffix{"-app" + app_ids
                             + (stat.memcachier_classes ?
                                 "-memcachier" : "-memcached")};
  size_curve.dump_cdf("partslab-size-curve" + filename_suffix + ".data");
  dump_util("partslab-util" + filename_suffix + ".data");
}

void partslab::dump_util(const std::string& filename) {}
