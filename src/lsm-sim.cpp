#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <set>
#include <cstdint>
#include <getopt.h>
#include <cmath>
#include <boost/version.hpp>
#include <cstdio>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <memory>
#include <cassert>

#include "common.h"
#include "request.h"
#include "fifo.h"
#include "shadowlru.h"
#include "shadowslab.h"
#include "partslab.h"
#include "lsm.h"
#include "lsc_multi.h"
#include "lru.h"
#include "slab.h"
#include "mc.h"

namespace ch = std::chrono;
typedef ch::high_resolution_clock hrc;

const char* policy_names[8] = { "shadowlru"
                              , "fifo"
                              , "lru"
															, "slab"
                              , "shadowslab"
                              , "partslab"
                              , "lsm"
                              , "multi"
                              };
enum pol_type { 
    SHADOWLRU = 0
  , FIFO
  , LRU
  , SLAB
  , SHADOWSLAB
  , PARTSLAB
  , LSM
  , MULTI };

// globals
std::set<uint32_t>    apps{};                           // apps to consider
bool                  all_apps = true;                  // run all by default 
bool                  roundup  = false;                 // no rounding default
float                 lsm_util = 1.0;                   // default util factor
std::string           trace    = "data/m.cap.out";      // default filepath
std::string           app_str;                          // for logging apps
double                hit_start_time = 86400;           // default time 
size_t                global_mem = 0;
pol_type              policy_type;                      // policy type
bool                  verbose = false;
double                gfactor = 1.25;                   // def slab growth fact
bool                  memcachier_classes = false;
size_t                partitions = 2;
size_t                segment_size = 8 * 1024 * 1024;

// Only parse this many requests from the CSV file before breaking.
// Helpful for limiting runtime when playing around.
int request_limit = 0;


  
#ifdef DEBUG
  bool debug = true;
#else
  bool debug = false;
#endif

const std::string     usage  = "-f    specify file path\n"
                               "-a    specify app to eval\n"
                               "-r    enable rounding\n"
                               "-u    specify utilization\n"
                               "-w    specify warmup period\n"
                               "-l    number of requests after warmup\n"
                               "-s    simulated cache size in bytes\n"
                               "-p    policy 0, 1, 2; shadowlru, fifo, lru\n"
                               "-v    incremental output\n"
                               "-g    specify slab growth factor\n"
                               "-M    use memcachier slab classes\n"
                               "-P    number of partitions for partslab\n"
                               "-S    segment size in bytes for lsm\n";

// memcachier slab allocations at t=86400 (24 hours)
const int orig_alloc[15] = {
  1664, 2304, 512, 17408, 266240, 16384, 73728, 188416, 442368,
  3932160, 11665408, 34340864, 262144, 0 , 0
};

std::unordered_map<size_t, size_t> memcachier_app_size = { {2, 118577408}
                                                         , {5, 35743872}
                                                         , {6, 7108608}
                                                         , {7, 77842880}
                                                         , {8, 10485760}
                                                         , {10, 684898304}
                                                         , {11, 7829952}
                                                         , {13, 36647040}
                                                         , {19, 51209600}
                                                         , {18, 6313216}
                                                         , {20, 70953344}
                                                         , {23, 4542897472}
                                                         , {29, 187378624}
                                                         , {31, 1409535488}
                                                         , {53, 11044096}
                                                         , {59, 1713664}
                                                         , {94, 23238784}
                                                         , {227, 20237184}
                                                         };


// returns true if an ID is in the spec'd set
// returns true if set is empty
// bool valid_id(const request *r) {
//  if (all_apps)
//    return true;
//  else
//    return apps.count(r->appid);
//}

int main(int argc, char *argv[]) {

  // just checking boost
  std::cerr << "Using Boost "     
            << BOOST_VERSION / 100000     << "."  // major version
            << BOOST_VERSION / 100 % 1000 << "."  // minor version
            << BOOST_VERSION % 100                // patch level
            << std::endl;

  // calculate global memory
  for (int i = 0; i < 15; i++)
    global_mem += orig_alloc[i];
  global_mem *= lsm_util;

  // parse cmd args
  int c;
  std::string sets;
  while ((c = getopt(argc, argv, "p:s:l:f:a:ru:w:vhg:MP:S:")) != -1) {
    switch (c)
    {
      case 'f':
        trace = optarg;
        break;
      case 'p':
        if (std::string(optarg) == "shadowlru")
          policy_type = pol_type(0); 
        else if (std::string(optarg) == "fifo")
          policy_type = pol_type(1); 
        else if (std::string(optarg) == "lru")
          policy_type = pol_type(2); 
        else if (std::string(optarg) == "slab")
          policy_type = pol_type(3);
        else if (std::string(optarg) == "shadowslab")
          policy_type = pol_type(4);
        else if (std::string(optarg) == "partslab")
          policy_type = pol_type(5);
        else if (std::string(optarg) == "lsm")
          policy_type = pol_type(6);
        else if (std::string(optarg) == "multi")
          policy_type = pol_type(7);
        else {
          std::cerr << "Invalid policy specified" << std::endl;
          exit(EXIT_FAILURE);
        }            
        break;
      case 's':
        global_mem = atol(optarg);
        break;
      case 'S':
        segment_size = atol(optarg);
        break;
      case 'l':
        request_limit = atoi(optarg); 
        break;
      case 'a':
        {
          string_vec v;
          csv_tokenize(std::string(optarg), &v);
          for ( const auto& e : v) {
            apps.insert(stoi(e));
            app_str += e;
            app_str += ",";
          }
          break;
        }
      case 'r':
        roundup = true;
        break;
      case 'u':
        lsm_util = atof(optarg);
        break;   
      case 'w':
        hit_start_time = atof(optarg);
        break;
      case 'v':
        verbose = true;
        break;
      case 'h': 
        std::cerr << usage << std::endl;
        break;
      case 'g':
        gfactor = atof(optarg);  
        break;
      case 'M':
        memcachier_classes = true;
        break;
      case 'P':
        partitions = atoi(optarg); 
        break;
    }
  }

  //assert(apps.size() == 1);

  // build a stats struct with basic info relevant to every policy.
  stats sts{policy_names[policy_type], apps, global_mem};

  //printf("APPID: %lu\n", appid);
 
  // instantiate a policy
  std::unique_ptr<policy> policy{};
  switch(policy_type) {
    case SHADOWLRU:
      policy.reset(new shadowlru(sts));
      break;
    case FIFO : 
      policy.reset(new fifo(sts));
      break;
    case LRU : 
      policy.reset(new lru(sts));
      break;
    case SLAB :
      if (memcachier_classes) {
        sts.gfactor = 2.0;
      } else {
        sts.gfactor = gfactor;
      }
      sts.memcachier_classes = memcachier_classes;
      policy.reset(new slab(sts));
      break;
    case SHADOWSLAB:
      if (memcachier_classes) {
        sts.gfactor = 2.0;
      } else {
        sts.gfactor = gfactor;
      }
      sts.memcachier_classes = memcachier_classes;
      policy.reset(new shadowslab(sts));
    case PARTSLAB:
      sts.partitions = partitions;
      policy.reset(new partslab(sts));
      break;
    case LSM:
      sts.segment_size = segment_size;
      sts.cleaning_width = 4;
      policy.reset(new lsm(sts));
      break;
    case MULTI:
      sts.segment_size = segment_size;
      sts.cleaning_width = 4;
      lsc_multi* multi = new lsc_multi(sts);
      policy.reset(multi);

      for (size_t appid : apps) {
        multi->add_app(
            appid,
            memcachier_app_size[appid] / 2,
            memcachier_app_size[appid]);
      }

      break;
  }

   if (!policy) {
    std::cerr << "No valid policy selected!" << std::endl;
    exit(-1);
  }

  // List input parameters
  std::cerr << "performing trace analysis on apps: " << app_str << std::endl
            << "policy: " << policy_names[policy_type] << std::endl
            << "using trace file: " << trace << std::endl
            << "rounding: " << (roundup ? "on" : "off") << std::endl
            << "utilization rate: " << lsm_util << std::endl
            << "start counting hits at t = " << hit_start_time << std::endl
            << "request limit: " << request_limit << std::endl
            << "global mem: " << global_mem << std::endl;
  if (policy_type == SHADOWSLAB) {
    if (memcachier_classes)
      std::cerr << "slab growth factor: memcachier" << std::endl;
    else
      std::cerr << "slab growth factor: " << gfactor << std::endl;
  }

  // proc file line by line
  std::ifstream t_stream(trace);
  assert(t_stream.is_open());
  std::string line;

  int i = 0;
  auto start = hrc::now();
  auto last_progress = start;
  size_t bytes = 0;
  
 
  while (std::getline(t_stream, line) &&
         (request_limit == 0 || i < request_limit))
  {
    request r{line};
    bytes += line.size();

    if (verbose && ((i & ((1 << 18) - 1)) == 0)) {
      auto now  = hrc::now();
      double seconds =
        ch::duration_cast<ch::nanoseconds>(now - last_progress).count() / 1e9;
      if (seconds > 1.0) {
        stats* stats = policy->get_stats();
        std::cerr << "Progress: " << r.time << " "
                  << "Rate: " << bytes / (1 << 20) / seconds << " MB/s "
                  << "Hit Rate: " << stats->get_hit_rate() * 100 << "% "
                  << "Evicted Items: " << stats->evicted_items << " "
                  << "Evicted Bytes: " << stats->evicted_bytes << " "
                  << "Utilization: " << stats->get_utilization()
                  << std::endl;
        bytes = 0;
        last_progress = now;
      }
    }

    // Only process requests for specified app, of type GET,
    // and values of size > 0, after time 'hit_start_time'.
    if ((r.type != request::GET) || apps.count(r.appid) == 0 || (r.val_sz <= 0))
      continue;

    policy->proc(&r, r.time < hit_start_time);

    ++i;
  }
  auto stop = hrc::now();

  // Log curves for shadowlru, shadowslab, and partslab.
  if(policy_type == 0 || policy_type == 4 || policy_type == 5)
    policy->log_curves();
 
  // Dump stats for all policies. 
  policy->dump_stats();

  double seconds =
    ch::duration_cast<ch::milliseconds>(stop - start).count() / 1000.;
  std::cerr << "total execution time: " << seconds << std::endl;

  return 0;
}

