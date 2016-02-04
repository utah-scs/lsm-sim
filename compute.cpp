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

#include "common.h"
#include "request.h"
#include "fifo.h"
#include "cliff.h"

namespace ch = std::chrono;
typedef ch::high_resolution_clock hrc;

const char* policy_names[3] = { "cliff"
                              , "fifo"
                              , "lru"
                              };
enum pol_typ { CLIFF = 0, FIFO = 1, LRU = 2 };

// globals
std::vector<bool>     global_hits{};                    // hits/misses (true for hit)
std::set<uint16_t>    apps{};                           // apps to consider
bool                  all_apps = true;                  // run all by default 
bool                  roundup  = false;                 // no rounding default
float                 lsm_util = 1.0;                   // default utilization factor
std::string           trace    = "data/m.cap.out"; // default filepath
std::string           app_str;                          // for logging apps
double                hit_start_time = 86400;           // default time 
double                global_mem = 0;
pol_typ               p_type;                           // policy type

// Only parse this many requests from the CSV file before breaking.
// Helpful for limiting runtime when playing around.
int request_limit = 0;


  
#ifdef DEBUG
  bool debug = true;
#else
  bool debug = false;
#endif

const std::string     usage  = "-f    specify file path\n"
                               "-a    specify apps to eval\n"
                               "-r    enable rounding\n"
                               "-u    specify utilization\n"
                               "-w    specify warmup period\n";

// memcachier slab allocations at t=86400 (24 hours)
const int orig_alloc[15] = {
  1664, 2304, 512, 17408, 266240, 16384, 73728, 188416, 442368,
  3932160, 11665408, 34340864, 262144, 0 , 0
};


// returns true if an ID is in the spec'd set
// returns true if set is empty
bool valid_id(const request *r) {
  if (all_apps)
    return true;
  else
    return apps.count(r->appid);
}

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
  while ((c = getopt(argc, argv, "p:l:f:a:ru:w:h")) != -1) {
    switch (c)
    {
      case 'f':
        trace = optarg;
        break;
      case 'p':
        p_type = pol_typ(atoi(optarg)); 
        break;
      case 'l':
        request_limit = atoi(optarg); 
        break;
      case 'a':
        {
          string_vec v;
          app_str = optarg;
          csv_tokenize(std::string(optarg), &v);
          all_apps = false;
          for (const auto& e : v)
            apps.insert(stoi(e));
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
      case 'h': 
        std::cerr << usage << std::endl;
        break;
    }
  }

  // PRE PROCESSING


  // instantiate a policy
  std::unique_ptr<policy> policy{};
  switch(p_type) {

    case CLIFF :
      { policy.reset(new cliff(global_mem));
        break; }
    case FIFO : 
      { policy.reset(new fifo(global_mem));
        break; }  
    case LRU : 
      { std::cerr << "TODO : LRU not yet enabled" << std::endl;
        exit(0);
        break;
      }
  }

  if (!policy) {
    std::cerr << "No valid policy selected!" << std::endl;
    exit(-1);
  }

  // List input parameters
  std::cerr << "performing trace analysis on app\\s: " << app_str << std::endl
            << "policy: " << policy_names[p_type] << std::endl
            << "using trace file: " << trace << std::endl
            << "rounding: " << (roundup ? "on" : "off") << std::endl
            << "utilization rate: " << lsm_util << std::endl
            << "start counting hits at t = " << hit_start_time << std::endl
            << "request limit: " << request_limit << std::endl;


  // proc file line by line
  std::ifstream t_stream(trace);
  std::string line;

  int i = 0;
  auto start = hrc::now();
  auto last_progress = start;
  size_t bytes = 0;

  policy->log_header();
 
  while (std::getline(t_stream, line) &&
         (request_limit == 0 || i < request_limit))
  {
    request r{line};
    bytes += line.size();

    // MAIN SIMULATION ALGORITHM
    // only process requests for specified apps, of type GET, 
    // and values of size > 0, after time 'hit_start_time'
    if ((r.type == request::GET) & valid_id(&r) & (r.val_sz > 0))
      if(r.time >= hit_start_time)
        global_hits.push_back(policy->proc(&r));
    
    ++i;
    if ((i & ((1 << 18) - 1)) == 0) {
      auto now  = hrc::now();
      double seconds =
        ch::duration_cast<ch::nanoseconds>(now - last_progress).count() / 1e9;
      std::cerr << "Progress: " << r.time << " "
                << "Rate: " << bytes / (1 << 20) / seconds << " MB/s"
                << std::endl;
      last_progress = now;
      policy->log();
    }
  }
  auto stop = hrc::now();

  policy->log();

  // POST PROCESSING
  uint64_t sum_hits = 0;
  for (auto h : global_hits)
    sum_hits += h;

  double hit_rate = float(sum_hits)/global_hits.size();
  double seconds =
    ch::duration_cast<ch::milliseconds>(stop - start).count() / 1000.;
    std::cerr << "final global queue size: " << policy->get_size() << std::endl
             << "final hit rate: "
             << std::setprecision(12) << hit_rate << std::endl 
             << "total execution time: " << seconds << std::endl;
}

