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
#include "slab_multi.h"
#include "lru.h"
#include "slab.h"
#include "mc.h"
#include "flash_cache.h"

namespace ch = std::chrono;
typedef ch::high_resolution_clock hrc;

const char* policy_names[10] = { "shadowlru"
                              , "fifo"
                              , "lru"
			      , "slab"
                              , "shadowslab"
                              , "partslab"
                              , "lsm"
                              , "multi"
                              , "multislab"
			      , "flashcache"
                              };
enum pol_type { 
    SHADOWLRU = 0
  , FIFO
  , LRU
  , SLAB
  , SHADOWSLAB
  , PARTSLAB
  , LSM
  , MULTI
  , MULTISLAB
  , FLASHCACHE };

// globals
std::set<uint32_t>    apps{};                        // apps to consider
std::unordered_map<uint32_t, uint32_t> app_steal_sizes{}; // how much each app
                                                        // steals per shadow q
                                                        // hit
bool                  all_apps = true;                  // run all by default 
bool                  roundup  = false;                 // no rounding default
float                 lsm_util = 1.0;                   // default util factor
std::string           trace    = "data/m.cap.out";      // default filepath
std::string           app_str;                          // for logging apps
std::string           app_steal_sizes_str;
double                hit_start_time = 86400;           // default time 
size_t                global_mem = 0;
pol_type              policy_type;                      // policy type
lsc_multi::subpolicy  subpolicy;                        // subpolicy type
bool                  verbose = false;
double                gfactor = 1.25;                   // def slab growth fact
bool                  memcachier_classes = false;
size_t                partitions = 2;
size_t                segment_size = 1 * 1024 * 1024;
size_t                min_mem_pct = 25;
const size_t          default_steal_size = 65536;
bool                  use_tax = false;
double                tax_rate = 0.05;
double                priv_mem_percentage =0.25; 
bool                  use_percentage = false;           // specify priv mem %

// Only parse this many requests from the CSV file before breaking.
// Helpful for limiting runtime when playing around.
int request_limit = 0;

bool debug = false;

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
                               "-S    segment size in bytes for lsm\n"
                               "-E    eviction subpolicy (for multi)\n"
                               "-m    private mem percentage of target mem";

// memcachier slab allocations at t=86400 (24 hours)
const int orig_alloc[15] = {
  1664, 2304, 512, 17408, 266240, 16384, 73728, 188416, 442368,
  3932160, 11665408, 34340864, 262144, 0 , 0
};

std::unordered_map<size_t, size_t> memcachier_app_size = { {1, 701423104}
                                                         , {2, 118577408}
                                                         , {3, 19450368}
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
  std::vector<int32_t> ordered_apps{};
  while ((c = getopt(argc, argv, "p:s:l:f:a:ru:w:vhg:MP:S:E:N:W:T:m:")) != -1) {
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
        else if (std::string(optarg) == "multislab")
          policy_type = pol_type(8);
	else if (std::string(optarg) == "flashcache")
	  policy_type = pol_type(9);
        else {
          std::cerr << "Invalid policy specified" << std::endl;
          exit(EXIT_FAILURE);
        }            
        break;
      case 'E':
        if (std::string(optarg) == "normal")
          subpolicy = lsc_multi::subpolicy(0); 
        else if (std::string(optarg) == "greedy")
          subpolicy = lsc_multi::subpolicy(1); 
        else if (std::string(optarg) == "static")
          subpolicy = lsc_multi::subpolicy(2); 
        else {
          std::cerr << "Invalid subpolicy specified" << std::endl;
          exit(EXIT_FAILURE);
        }            
        break;
      case 'N':
        min_mem_pct = atol(optarg);
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
          for (const auto& e : v) {
            int i = stoi(e);
            apps.insert(i);
            ordered_apps.push_back(i);
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
      case 'W':
        {
          string_vec v{};
          csv_tokenize(optarg, &v);
          int i = 0;
          for (const auto& e : v) {
            app_steal_sizes.insert(std::make_pair(ordered_apps.at(i++), stoi(e)));
            app_steal_sizes_str += e;
            app_steal_sizes_str += ",";
          }
          break;
        }
      case 'T':
        use_tax = true;
        tax_rate = atol(optarg) / 100.;
        break;
      case 'm':
        priv_mem_percentage = atof(optarg);
        use_percentage = true;
        break;
    }
  }

  assert(apps.size() >= app_steal_sizes.size());

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
    case FLASHCACHE :
	policy.reset(new FlashCache(sts));
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
      sts.cleaning_width = 10;
      policy.reset(new lsm(sts));
      break;
    case MULTI:
      sts.segment_size = segment_size;
      sts.cleaning_width = 10;
      {
        lsc_multi* multi = new lsc_multi(sts, subpolicy);
        policy.reset(multi);

        for (size_t appid : apps) {
          assert(memcachier_app_size[appid] > 0);
          uint32_t app_steal_size = 65536;
          auto it = app_steal_sizes.find(appid);
          if (it != app_steal_sizes.end()) {
            app_steal_size = it->second;
          } 
          size_t private_mem = use_percentage ? 
            (size_t)(memcachier_app_size[appid] * priv_mem_percentage) :
            min_mem_pct;         
          multi->add_app(appid,
                         private_mem ,
                         memcachier_app_size[appid],
                         app_steal_size);
        }

        if (use_tax) {
          multi->set_tax(tax_rate);
        }
      }

      break;
    case MULTISLAB:
      if (memcachier_classes) {
        sts.gfactor = 2.0;
      } else {
        sts.gfactor = gfactor;
      }
      sts.memcachier_classes = memcachier_classes;
      slab_multi* slmulti = new slab_multi(sts);
      policy.reset(slmulti);

      for (size_t appid : apps) {
        assert(memcachier_app_size[appid] > 0);
        slmulti->add_app(appid, min_mem_pct, memcachier_app_size[appid]);
      }
      break;
  }

   if (!policy) {
    std::cerr << "No valid policy selected!" << std::endl;
    exit(-1);
  }

  // List input parameters
  std::cerr << "performing trace analysis on apps " << app_str << std::endl
            << "with steal weights of " << app_steal_sizes_str << std::endl
            << "policy " << policy_names[policy_type] << std::endl
            << "using trace file " << trace << std::endl
            << "rounding " << (roundup ? "on" : "off") << std::endl
            << "utilization rate " << lsm_util << std::endl
            << "start counting hits at t = " << hit_start_time << std::endl
            << "request limit " << request_limit << std::endl
            << "global mem " << global_mem << std::endl
            << "use tax " << use_tax << std::endl
            << "tax rate " << tax_rate << std::endl;
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
    if ((r.type != request::GET) ||
        std::find(std::begin(apps),
                  std::end(apps), r.appid) == std::end(apps) ||
        (r.val_sz <= 0))
    {
      continue;
    }

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

