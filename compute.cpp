#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <set>
#include <cstdint>
#include <getopt.h>
#include <sstream>
#include <cmath>
#include <boost/version.hpp>
#include <cstdio>
#include <iomanip>
#include <ctime>
#include <chrono>
#include "fifo.h"
#include "cliff.h"


namespace ch = std::chrono;
typedef ch::high_resolution_clock hrc;
typedef std::vector<std::string> string_vec;
enum pol_typ { CLIFF = 1, FIFO = 2, LRU = 3 };


// globals
std::vector<bool>     global_hits;                      // hits/misses (true for hit)
std::set<uint16_t>    apps;                             // apps to consider
bool                  all_apps = true;                  // run all by default 
bool                  roundup  = false;                 // no rounding default
float                 lsm_util = 1.0;                   // default utilization factor
std::string           trace    = "data/m.cap.out.head"; // default filepath
float                 run_time = 0.0;                   // execution time
std::string           app_str;                          // for logging apps
double                hit_start_time = 86400;           // default time 
uint32_t              req_count = 0;                    // num of reqs proc'd 
double                global_mem = 0;
int                   req_num = 0;
Policy                *pol;                             // policy to use
pol_typ               p_type;                           // policy type


  
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


// prototypes
int csv_tokenize(const std::string &s, string_vec *tokens);
void proc_line(const std::string &s, request *r);
void dump_request(const request *r);
int proc_request(const request *r);
bool valid_id(const request *r);
uint32_t get_slab_class(uint32_t size);


int main(int argc, char *argv[]) {

  // just checking boost
  std::cout << "Using Boost "     
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
  while ((c = getopt(argc, argv, "p:f:a:ru:w:h")) != -1) {
    switch (c)
    {
      case 'f':
        trace = optarg;
        break;
      case 'p':
        p_type = pol_typ(atoi(optarg)); 
        break;
      case 'a':
        {
          apps = std::set<uint16_t>();
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
        std::cout << usage << std::endl;
        break;
    }
  }

  // PRE PROCESSING


  // instantiate a policy
  switch(p_type) {

    case CLIFF :
      { pol = new Cliff(global_mem);
        break; }
    case FIFO : 
      { pol = new Fifo(global_mem); 
        break; }  
    case LRU : 
      { std::cout << "TODO : LRU not yet enabled" << std::endl;
        exit(0);
        break;
      }
  }


  // List input parameters
  std::cout << "performing trace analysis on app\\s: " << app_str << std::endl
            << "policy: " << p_type << std::endl
            << "using trace file: " << trace << std::endl
            << "rounding: " << (roundup ? "on" : "off") << std::endl
            << "utilization rate: " << lsm_util << std::endl
            << "start counting hits at t = " << hit_start_time << std::endl;


  // proc file line by line
  std::ifstream t_stream(trace);
  std::string line;

  int i = 0;
  auto start = hrc::now();
  auto last_progress = start;
  size_t bytes = 0;
 
  while (std::getline(t_stream, line)) {

    request r;
    proc_line (line, &r);
    bytes += line.size();


    // MAIN SIMULATION ALGORITHM
    // only process requests for specified apps, of type GET, 
    // and values of size > 0, after time 'hit_start_time'
    if ((r.type == GET) & valid_id(&r) & (r.val_sz > 0))
      if(r.time >= hit_start_time)
        global_hits.push_back(pol->proc(&r));
    
    ++i;
    if ((i & ((1 << 18) - 1)) == 0) {
      auto now  = hrc::now();
      double seconds =
        ch::duration_cast<ch::nanoseconds>(now - last_progress).count() / 1e9;
      std::cout << "Progress: " << r.time << " "
                << "Rate: " << bytes / (1 << 20) / seconds << " MB/s"
                << std::endl;
      last_progress = now;
    }
  }
  auto stop = hrc::now();



  // POST PROCESSING
  uint64_t sum_hits = 0;
  for (auto h : global_hits)
    sum_hits += h;

  double hit_rate = float(sum_hits)/global_hits.size();
  double seconds =
    ch::duration_cast<ch::milliseconds>(stop - start).count() / 1000.;
    std::cout << "final global queue size: " << pol->get_size() << std::endl
             << "final hit rate: "
             << std::setprecision(12) << hit_rate << std::endl 
             << "total execution time: " << seconds << std::endl;


  // CLEANUP
  
  delete pol;


}








// returns true if an ID is in the spec'd set
// returns true if set is empty
bool valid_id(const request *r) {
  if (all_apps)
    return true;
  else
    return apps.count(r->appid);
}



/**
 * Populate a request \a r from a single CSV string \a s.
 * Logs errors on lines that cannot be parsed.
 */
void proc_line(const std::string &s, request *r) {
  std::string::size_type sz;
  string_vec tokens;
  csv_tokenize(s, &tokens);

  try {
    r->time   = std::stod (tokens.at(0), &sz);
    r->appid  = std::stoi (tokens.at(1), &sz);
    r->type   = req_typ(std::stoi(tokens.at(2), &sz));
    r->key_sz = std::stoi(tokens.at(3), &sz);
    r->val_sz = std::stoi(tokens.at(4), &sz);
    r->kid    = std::stoi(tokens.at(5), &sz);
    // r->hit    = stoi(tokens.at(6), &sz) == 1 ? true : false;
  } catch (const std::out_of_range& e) {
    std::cerr << "Malformed line couldn't be parsed: " << e.what() << std::endl
              << "\t" << s << std::endl;
  }
}



// debug/check
void dump_request(const request *r) {
  std::cout << "*** request ***"  << std::endl
            << "time: "           << r->time    << std::endl
            << "app id: "         << r->appid   << std::endl
            << "req type: "       << r->type    << std::endl
            << "key size: "       << r->key_sz  << std::endl
            << "val size: "       << r->val_sz  << std::endl
            << "kid: "            << r->kid     << std::endl
            << "hit:" << (r->hit == 1 ? "yes" : "no") << std::endl;
}


// breaks a CSV string into a vector of tokens
int csv_tokenize(const std::string &s, string_vec *tokens) {
  std::istringstream ss(s);
  std::string token;
  while (std::getline(ss, token, ',')) {
    tokens->push_back(token);
  }
  return 0;
}






