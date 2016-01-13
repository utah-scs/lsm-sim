#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <set>
#include <stdint.h>
#include <GetOpt.h>
#include <sstream>
#include <math.h>
#include <boost/version.hpp>
#include <boost/intrusive/list.hpp>
#include <stdio.h>
#include <iomanip>
#include <ctime>


// defs
enum req_typ { GET = 1, SET = 2, DEL = 3, ADD = 4, INC = 5, STAT = 6, OTHR = 7 };

namespace bi = boost::intrusive;

typedef struct request {
  float time;
  uint32_t key_sz;
  uint32_t val_sz;
  uint32_t kid;
  uint16_t appid;
  req_typ type;
  uint8_t hit;
} req;

typedef struct req_pair : public bi::list_base_hook<> {
  uint16_t id;
  uint16_t size;
  req_pair (uint16_t i, uint16_t s) : id (i), size(s) {}
} req_pair;

bool operator == (const req_pair &lhs, const req_pair &rhs) {
  return lhs.id == rhs.id;
}


// disposer object function
struct delete_disposer
{
   void operator()(req_pair *delete_this)
   {  delete delete_this;  }
};


typedef std::vector<std::string> string_vec;


// globals
std::vector<bool>     global_hits;                    // vector of hits/misses (true for hit)
std::vector<uint32_t> slab_classes;                   // I think we can get rid of this.
std::set<uint16_t>    apps;                           // the set of all apps we want to consider
bi::list<req_pair>    *global_lru;                    // global lru


bool                  all_apps = true;                // run all apps by default (if no args sepc'd)
bool                  roundup  = false;               // don't round up by default
float                 lsm_util = 1.0;                 // default lsm utilization factor
std::string           trace    = "m.cap.out.head";    // default filepath
float                 run_time = 0.0;                 // keep track of execution time
std::string           app_str;                        // just for logging which apps we're proc'ing
std::clock_t          start;

const std::string     usage  = std::string("-p    specify path\n") 
                             + std::string("-a    specify apps to eval\n") 
                             + std::string("-r    enable rounding\n")
                             + std::string("-u    specify utilization\n");

// memcachier slab allocations at t=86400 (24 hours)
const int orig_alloc[15] = {  1664, 2304, 512, 17408, 266240, 16384, 73728, 188416, 442368, 
                              3932160, 11665408, 34340864, 262144, 0 , 0 };


float global_mem = 0;             
uint64_t global_queue_size = 0;
int req_num = 0;

// prototypes
int csv_tokenize(const std::string &s, string_vec *tokens);
int proc_line (const std::string &s, request *r);
void dump_request (const request *r); 
int proc_request (const request *r);
bool valid_id (const request *r); 
uint32_t get_slab_class (uint32_t size);

int main (int argc, char *argv[]) {

  // just checking boost
  std::cout << "Using Boost "     
            << BOOST_VERSION / 100000     << "."  // major version
            << BOOST_VERSION / 100 % 1000 << "."  // minor version
            << BOOST_VERSION % 100                // patch level
            << std::endl;

  // parse cmd args
  int c, e;
  std::string sets;
  while ((c = getopt(argc, argv, "p:a:ru:h")) != -1) {
    switch (c)
    {
      case 'p':
        trace = optarg;
        break;
      case 'a':
        {
          apps = std::set<uint16_t>();
          string_vec v;
          app_str = optarg;
          csv_tokenize(std::string(optarg), &v);
          all_apps = false;
          for( auto it = v.begin(); it != v.end(); ++it)
            apps.insert(stoi(*it));
          break;
        }
      case 'r':
        roundup = true;
        break;
      case 'u':
        lsm_util = atof(optarg);
        break;
      case 'h': 
        std::cout << usage << std::endl;
    }
  }



  // PRE PROCESSING

  // calculate global memory
  for (int i = 0; i < 15; i++)
    global_mem += orig_alloc[i];
  global_mem *= lsm_util;

  // NOTE: this may not actually be necessary if we just
  // calculate the slab class instead of keeping a list.
  // populate slab classes vector
  for (int i = 0; i < 15; i++) {
    slab_classes.push_back( 64 * pow(2,i) );
    // std::cout << (64 * pow(2,i) ) << std::endl;
  }


  // set up the eviction queue
  global_lru = new bi::list<req_pair>();


  // print out some possibly useful stuff
  std::cout << "performing trace analysis on app\\s: " << app_str << std::endl;
  std::cout << "using trace file: " << trace << std::endl;
  std::cout << "rounding: " << (roundup ? "on" : "off") << std::endl;
  std::cout << "utilization rate: " << lsm_util << std::endl; 



  // proc file line by line
  std::ifstream t_stream(trace);
  std::string line;
  start = std::clock();
  while (std::getline(t_stream, line)) {
    request r;
    proc_line (line, &r);

    // only process requests for specified apps, of type GET, 
    // and values of size > 0
    if (valid_id(&r) && r.type == GET && r.val_sz > 0)
      proc_request(&r);
  }




  // POST PROCESSING

  uint64_t sum_hits = 0;
  for (auto h : global_hits)
    sum_hits += h;

  double hit_rate = float(sum_hits)/global_hits.size();

  double duration = (std::clock() - start) / (double) CLOCKS_PER_SEC;

  std::cout << "final global queue size: " << global_queue_size << std::endl;
  std::cout << "final hit rate: " << std::setprecision(12) << hit_rate << std::endl; 
  std::cout << "total execution time: " << duration << std::endl;

  delete (global_lru);

}



// MAIN SIMULATION ALGORITHM 
int proc_request (const request *r) {

  // dump_request(r);
  uint64_t kv_size      = r->key_sz + r->val_sz;
  uint64_t request_size = roundup ? get_slab_class(kv_size) : kv_size;
  int global_pos = -1;
  uint64_t k = 0;
  global_queue_size = 0;

  // iterate over keys in the lru queue
  for (req_pair &a : *global_lru) {

    k++;
    global_queue_size += a.size;

    if (a.id == r->kid) {

      global_pos = k;
      global_queue_size += request_size;
      global_queue_size -= a.size;

      // remove the request from its current position in the queue
      global_lru->remove_and_dispose(a, delete_disposer());
      break;
    }
  }

  // append the request to the back of the LRU queue
  req_pair *req = new req_pair(r->kid, request_size);
  global_lru->push_back(*req);
  req_num++;


  // after 24 hours has passed, start counting hits/misses
  if(r->time > 0) {
    if (global_pos != -1) 
      global_hits.push_back(global_queue_size <= global_mem);
    else
      global_hits.push_back(false);
  }
    
  return 0;
}




// returns true if an ID is in the spec'd set
// returns true if set is empty
bool valid_id (const request *r) {
  if(all_apps)
    return true;
  else
    return apps.count(r->appid);
}


// Sean Anderson's nearest power of 2 alg.
uint32_t get_slab_class (uint32_t size) {
  --size;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
   return size + 1;
}


// parse a CSV line from file into request struct
int proc_line (const std::string &s, request *r) {

  std::string::size_type sz;
  string_vec tokens;
  csv_tokenize(s, &tokens);

  r->time   = std::stof (tokens.at(0), &sz);
  r->appid  = std::stoi (tokens.at(1), &sz);
  r->type   = req_typ(std::stoi(tokens.at(2), &sz));
  r->key_sz = std::stoi(tokens.at(3), &sz);
  r->val_sz = std::stoi(tokens.at(4), &sz);
  r->kid    = std::stoi(tokens.at(5), &sz);
  r->hit    = stoi(tokens.at(6), &sz) == 1 ? true : false;

  return 0;
}


// debug/check
void dump_request (const request *r) {

  std::cout << "*** request ***"  << std::endl;
  std::cout << "time: "           << r->time    << std::endl;
  std::cout << "app id: "         << r->appid   << std::endl;
  std::cout << "req type: "       << r->type    << std::endl;
  std::cout << "key size: "       << r->key_sz  << std::endl;
  std::cout << "val size: "       << r->val_sz  << std::endl;
  std::cout << "kid: "            << r->kid     << std::endl;
  std::cout << "hit:";            
  r->hit == 1 ? std::cout << "yes" << std::endl : 
                std::cout << "no"  << std::endl;
}



// breaks a CSV string into a vector of tokens
int csv_tokenize(const std::string &s, string_vec *tokens) {
  string_vec::iterator it = tokens->begin();
  std::istringstream ss(s);
  std::string token;
  while (std::getline(ss, token, ',')) {
    tokens->push_back(token);
  }
  return 0;
}






