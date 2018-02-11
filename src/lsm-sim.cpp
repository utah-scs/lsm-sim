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
#include "victim_cache.h"
#include "ripq.h"
#include "ripq_shield.h"
#include "lruk.h"
#include "clock.h"
#include "segment_util.h"
#include "flash_cache_lruk.h"
#include "ram_shield.h"
#include "ram_shield_fifo.h"
#include "ram_shield_sel.h"
#include "flash_shield.h"
#include "flash_cache_lruk_clock.h"
#include "replay.h"
#include "flash_cache_lruk_clock_machinelearning.h"
#include "partitioned_LRU.h"

using namespace std::chrono;
typedef high_resolution_clock hrc;

const char* Policy_names[24] = { "shadowlru"
                               , "fifo"
                               , "lru"
                               , "slab"
                               , "shadowslab"
                               , "partslab"
                               , "lsm"
                               , "multi"
                               , "multislab"
                               , "flashcache"
                               , "victimcache"
                               , "lruk"
                               , "ripq"
                               , "ripq_shield"
                               , "clock"
                               , "flashcachelruk"
                               , "flashcachelrukclk"
                               , "segment_util"
                               , "ramshield"
                               , "ramshield_fifo"
                               , "ramshield_sel"
                               , "replay"
                               , "flashshield"
							                 , "flashcachelrukclkmachinelearning"
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
/// String of arguments.
const std::string usage =
"-f    specify file path\n"
"-a    specify app to eval\n"
"-r    enable rounding\n"
"-u    specify utilization\n"
"-w    specify warmup period\n"
"-l    number of Requests after warmup\n"
"-s    simulated cache size in bytes\n"
"-p    Policy 0, 1, 2; shadowlru, fifo, lru\n"
"-v    incremental output\n"
"-g    specify slab growth factor\n"
"-M    use memcachier slab classes\n"
"-P    number of partitions for partslab\n"
"-S    segment size in bytes for lsm\n"
"-B    block size in bytes for ripq and ram_shield\n"
"-E    eviction Subpolicy (for multi)\n"
"-m    private mem percentage of target mem\n"
"-W    per-application weights\n"
"-c    cleaning width\n"
"-D    dram size in flashcache, victimcache and ripq_shield policies\n"
"-F    flash size in flashcache, victimcache, ripq and ripq_shield policies\n"
"-K    number of queues in lruk Policy\n"
"-L    queue size in lruk Policy\n"
"-H    hit value in flashcache formula\n"
"-n    number of flash sections for ripq and ripq_shield\n" 
"-d    number of dram sections for ripq_shield\n"
"-C    CLOCK maximum value\n"
"-Z    number of cores to partition cache processing by\n";

/// Memcachier slab allocations at t=86400 (24 hours)
const int orig_alloc[15] = {
  1664, 2304, 512, 17408, 266240, 16384, 73728, 188416, 442368,
  3932160, 11665408, 34340864, 262144, 0 , 0
};

/// Possible Policy types.
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
  , FLASHCACHE
  , VICTIMCACHE 
  , LRUK
  , RIPQ 
  , RIPQ_SHIELD
  , CLOCK
  , FLASHCACHELRUK
  , FLASHCACHELRUKCLK
  , SEGMENT_UTIL
  , RAMSHIELD
  , RAMSHIELD_FIFO
  , RAMSHIELD_SEL
  , FLASHSHIELD
  , FLASHCACHELRUKCLKMACHINELEARNING
  , PARTITIONED_LRU
  , NONE
  };

struct Args {
  
  std::unordered_map<uint32_t, uint32_t> app_steal_sizes{};
  std::set<uint32_t> apps{};

  bool          all_apps             = true;             //< run all by default 
  bool          roundup              = false;            //< no rounding default
  float         lsm_util             = 1.0;              //< default util factor
  std::string   trace                = "data/m.cap.out"; //< default filepath
  double        hit_start_time       = 86400;            //< default warmup time
  size_t        global_mem           = 0;
  bool          verbose              = false;
  double        gfactor              = 1.25;             //< Slab growth factor.
  bool          memcachier_classes   = false;
  size_t        partslab_partitions  = 2;
  size_t        segment_size         = 1 * 1024 * 1024;
  size_t        block_size           = 1 * 1024 * 1024;
  size_t        num_dsections        = 4;
  size_t        min_mem_pct          = 75;
  const size_t  default_steal_size   = 65536;
  bool          use_tax              = false;
  double        tax_rate             = 0.05;
  double        priv_mem_percentage  = 0.25;
  bool          use_percentage       = false;            // specify priv mem %
  size_t        flash_size           = 0;
  size_t        num_sections         = 0;
  double        USER_SVM_TH          = 1;
  bool          debug                = false;

  // Only parse this many Requests from the CSV file before breaking.
  // Helpful for limiting runtime when playing around.
  int           Request_limit        = 0;

  /// Amount of dram memory allocated for ripq_shield active_blocks.
  size_t        dram_size            = 0;
  double        threshold            = 0.7;
  size_t        cleaning_width       = 100;

  std::string           app_str      = "";              //< for logging apps
  pol_type              Policy_type  = NONE;
  lsc_multi::Subpolicy  Subpolicy    = lsc_multi::Subpolicy(0);
  std::string           app_steal_sizes_str = "";

  // The number of partitions in which to globally partition the cache.
  uint16_t num_global_partitions = 1;
};

/// Forward declare some utility functions.
std::unique_ptr<Policy> create_Policy(Args& args);
void parse_stdin(Args& args, int argc, char** argv);
void calculate_global_memory(Args& args);
void init_partitions(Args& args, const size_t num_partitions);

// Partition (high-level partitioning) : A Partition is a logical grouping of
// cache-related mechanisms and statistical variables that emulates a single
// parition in high-level partitioning of cache processing.  
// TODO: Consider making Partition a thread object/functor with a data callback
//       get results.  Should look into Intel TBB information first and see what
//       facilities are offered before deciding.
// TODO: Think about better ways of extracting data than the previous "dump
//       to the console every x seconds" method.  This will likely become a mess
//       when partition numbers increase.
// TODO: Also think about what data will be useful on a per-partition basis and
//       a global basis.  What patterns will best tease out the comparisons
//       between globalism and partitioning.
/* struct Partition */
/* { */ 
/*   Partition(std::unique_ptr<Policy> p) */
/*     : _Policy(std::move(p)) */
/*   { */
/*   } */
    
/*   std::unique_ptr<Policy> _Policy; */
/*   /// Additional stuff can go here as needed. */
/*   /// i.e. additional stats tracking, concurrency mechanisms, etc. */
/*   /// ... */
/* }; */

/* std::vector<std::unique_ptr<Partition>> global_partitions; */ 

/// Creates a new Policy with stats and packs it into a partition struct
/// for num_partitions partitions and pushes them all into a vector.
/// 
/// @param Args struct containing Policy info.
///
/// @param size_t indicating the number of partitions to create.
/* void init_partitions(Args& args) */
/* { */
/*   for (size_t p = 0; p < args.num_global_partitions; ++p) */
/*   { */
/*     std::unique_ptr<Policy> pol = create_Policy(args); */
/*     std::unique_ptr<Partition> part */ 
/*       = std::make_unique<Partition>(std::move(pol)); */
/*     global_partitions.push_back(std::move(part)); */ 

/*     // TODO: This is likely a good place to fire off threads. */
/*     //       May want to consider making Partitions a functor */ 
/*     //       with some sort of callback to aggregate all of the incoming */
/*     //       partitions data async. */
/*   } */
/* } */

int main(int argc, char *argv[]) 
{
  Args args;
  calculate_global_memory(args);
  parse_stdin(args, argc, argv);
  /* init_partitions(args); */

  // TODO: Move this somewhere, this is Policy specific.
  assert(args.apps.size() >= args.app_steal_sizes.size());
  if (args.Policy_type == FLASHSHIELD ||
      args.Policy_type == FLASHCACHE ||
      args.Policy_type == FLASHCACHELRUK ||
      args.Policy_type == FLASHCACHELRUKCLK ||
      args.Policy_type == VICTIMCACHE ||
      args.Policy_type == RIPQ ||
	    args.Policy_type == FLASHCACHELRUKCLKMACHINELEARNING ||
      args.Policy_type == RIPQ_SHIELD)
  {
    args.global_mem = DRAM_SIZE + FLASH_SIZE;
  }

  // TODO: Create a dispatch meachanism that uniformly assigns Requests to a
  // Partition object for processing.  To ensure legacy operation of other
  // experiments, dispatch routine will simply dispatch to a single partition.

  std::unique_ptr<Policy> Policy = create_Policy(args); // <<-- replace with parts

  // List input parameters
  std::cerr << "performing trace analysis on apps " << args.app_str << std::endl
            << "with steal weights of " << args.app_steal_sizes_str << std::endl
            << "Policy " << Policy_names[args.Policy_type] << std::endl
            << "using trace file " << args.trace << std::endl
            << "rounding " << (args.roundup ? "on" : "off") << std::endl
            << "utilization rate " << args.lsm_util << std::endl
            << "start counting hits at t = " << args.hit_start_time << std::endl
            << "Request limit " << args.Request_limit << std::endl
            << "global mem " << args.global_mem << std::endl
            << "use tax " << args.use_tax << std::endl
            << "tax rate " << args.tax_rate << std::endl
            << "cleaning width " << args.cleaning_width << std::endl;

  if (args.Policy_type == SHADOWSLAB) 
  {
    if (args.memcachier_classes)
    {
      std::cerr << "slab growth factor: memcachier" << std::endl;
    }
    else
    {
      std::cerr << "slab growth factor: " << args.gfactor << std::endl;
    }
  }

  // proc file line by line
  std::ifstream t_stream(args.trace);
  assert(t_stream.is_open());
  std::string line;

  int i = 0;
  auto start = hrc::now();
  auto last_progress = start;
  size_t bytes = 0;
  
  size_t time_hour = 1; 
  while (std::getline(t_stream, line) &&
    (args.Request_limit == 0 || i < args.Request_limit))
  {
  
  // TODO: Create a dispatch function here to distribute lines evenly aross
  // partitions.

    Request r{line};
    bytes += line.size();

    if (args.verbose && ((i & ((1 << 18) - 1)) == 0)) {
      auto now  = hrc::now();
      double seconds =
        duration_cast<nanoseconds>(now - last_progress).count() / 1e9;

      // TODO: Report stats for each parition as well as the overall hit rate.
      if (seconds > 1.0) {
        stats* stats = Policy->get_stats();
        std::cerr << "Progress: " << std::setprecision(10) << r.time << " "
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

    // Only process Requests for specified app, of type GET,
    // and values of size > 0, after time 'hit_start_time'.
    if (r.type != Request::GET)
    {
      continue;
    }
    if (r.val_sz <= 0)
    {
      continue;
    }

    const bool in_apps =
      std::find(std::begin(args.apps), std::end(args.apps), r.appid) != std::end(args.apps);

    if (args.all_apps && !in_apps) 
    {
      args.apps.insert(r.appid);
    } 
    else if (!in_apps) 
    {
      continue;
    }

    Policy->process_request(&r, r.time < args.hit_start_time);
    if (args.verbose && ( args.Policy_type == FLASHSHIELD || args.Policy_type == VICTIMCACHE || 
          args.Policy_type == RIPQ ) && time_hour * 3600 < r.time) 
    {
      printf ("Dumping stats for FLASHSHIELD\n");
	    Policy->dump_stats();
	    time_hour++;
    }
    ++i;
  }

  auto stop = hrc::now();

  // Log curves for shadowlru, shadowslab, and partslab.
  if (args.Policy_type == 0 || args.Policy_type == 4 || args.Policy_type == 5)
  {
    Policy->log_curves();
  }
 
  // Dump stats for all policies. 
  Policy->dump_stats();
  double seconds = duration_cast<milliseconds>(stop - start).count() / 1000.;
  std::cerr << "total execution time: " << seconds << std::endl;

  return 0;
}

/// Calculates global memory from the Memcachier data points.
///
/// @param Args struct to populate with the global memory value.
void calculate_global_memory(Args& args)
{
  for (int i = 0; i < 15; i++)
  {
    args.global_mem += orig_alloc[i];
  }
  args.global_mem *= args.lsm_util;
}

/// Parse arguments from stdin on startup and populate the args struct.
///
/// @param args reference to an Args struct.
/// 
/// @param argc from main.
/// @param argv from main.
void parse_stdin(Args& args, int argc, char** argv)
{
  // parse cmd args
  int c;
  std::vector<int32_t> ordered_apps{};
  while ((c = getopt(argc, argv, "p:s:l:f:a:ru:w:vhg:MP:S:B:E:N:W:T:t:m:d:F:n:"
                                 "D:L:K:k:C:c:A:C:Y:Z:")) != -1)
  {
    switch (c)
    {
      case 'f':
        args.trace = optarg;
        break;
      case 'p':
        if (std::string(optarg) == "shadowlru")
          args.Policy_type = pol_type(0); 
        else if (std::string(optarg) == "fifo")
          args.Policy_type = pol_type(1); 
        else if (std::string(optarg) == "lru")
          args.Policy_type = pol_type(2); 
        else if (std::string(optarg) == "slab")
          args.Policy_type = pol_type(3);
        else if (std::string(optarg) == "shadowslab")
          args.Policy_type = pol_type(4);
        else if (std::string(optarg) == "partslab")
          args.Policy_type = pol_type(5);
        else if (std::string(optarg) == "lsm")
          args.Policy_type = pol_type(6);
        else if (std::string(optarg) == "multi")
          args.Policy_type = pol_type(7);
        else if (std::string(optarg) == "multislab")
          args.Policy_type = pol_type(8);
        else if (std::string(optarg) == "flashcache")
          args.Policy_type = pol_type(9);
        else if (std::string(optarg) == "victimcache")
          args.Policy_type = pol_type(10);
        else if (std::string(optarg) == "lruk")
          args.Policy_type = pol_type(11);
        else if (std::string(optarg) == "ripq")
          args.Policy_type = pol_type(12);
        else if (std::string(optarg) == "ripq_shield")
          args.Policy_type = pol_type(13);
        else if (std::string(optarg) == "clock")
          args.Policy_type = pol_type(14);
        else if (std::string(optarg) == "flashcachelruk")
          args.Policy_type = pol_type(15);
        else if (std::string(optarg) == "flashcachelrukclk")
          args.Policy_type = pol_type(16);
        else if (std::string(optarg) == "segment_util")
          args.Policy_type = pol_type(17);
        else if (std::string(optarg) == "ramshield")
          args.Policy_type = pol_type(18);  
        else if (std::string(optarg) == "ramshield_fifo")
          args.Policy_type = pol_type(19);
        else if (std::string(optarg) == "ramshield_sel")
          args.Policy_type = pol_type(20);
        else if (std::string(optarg) == "replay")
          args.Policy_type = pol_type(21);
        else if (std::string(optarg) == "flashshield")
          args.Policy_type = pol_type(22);
		    else if (std::string(optarg) == "flashcachelrukclkmachinelearning")
          args.Policy_type = pol_type(23);
        else {
          std::cerr << "Invalid Policy specified" << std::endl;
          exit(EXIT_FAILURE);
        }
        break;
      case 'E':
        if (std::string(optarg) == "normal")
          args.Subpolicy = lsc_multi::Subpolicy(0); 
        else if (std::string(optarg) == "greedy")
          args.Subpolicy = lsc_multi::Subpolicy(1); 
        else if (std::string(optarg) == "static")
          args.Subpolicy = lsc_multi::Subpolicy(2); 
        else {
          std::cerr << "Invalid Subpolicy specified" << std::endl;
          exit(EXIT_FAILURE);
        }            
        break;
      case 'N':
        args.min_mem_pct = atol(optarg);
        break;
      case 's':
        args.global_mem = atol(optarg);
        break;
      case 'S':
        args.segment_size = atol(optarg);
        break;
      case 'B':
        args.block_size = atol(optarg);
        break;
      case 'l':
        args.Request_limit = atoi(optarg); 
        break;
      case 'a':
        {
          string_vec v;
          csv_tokenize(std::string(optarg), &v);
          for (const auto& e : v) {
            args.all_apps = false;
            int i = stoi(e);
            args.apps.insert(i);
            ordered_apps.push_back(i);
            args.app_str += e;
            args.app_str += ",";
          }
          break;
        }
      case 'r':
        args.roundup = true;
        break;
      case 'u':
        args.lsm_util = atof(optarg);
        break;   
      case 'w':
        args.hit_start_time = atof(optarg);
        break;
      case 'v':
        args.verbose = true;
        break;
      case 'h': 
        std::cerr << usage << std::endl;
        break;
      case 'g':
        args.gfactor = atof(optarg);  
        break;
      case 'M':
        args.memcachier_classes = true;
        break;
      case 'P':
        args.partslab_partitions = atoi(optarg); 
        break;
      case 'W':
        {
          string_vec v{};
          csv_tokenize(optarg, &v);
          int i = 0;
          for (const auto& e : v) {
            args.app_steal_sizes.insert(std::make_pair(ordered_apps.at(i++), stoi(e)));
            args.app_steal_sizes_str += e;
            args.app_steal_sizes_str += ",";
          }
          break;
        }
      case 'c':
        args.cleaning_width = atol(optarg);
        break;
      case 'T':
        args.use_tax = true;
        args.tax_rate = atol(optarg) / 100.;
        break;
      case 'm':
        args.priv_mem_percentage = atof(optarg);
        args.use_percentage = true;
        break;
      case 'F':
        FLASH_SIZE = args.flash_size = atol(optarg);
        FLASH_SIZE_FC_KLRU = args.flash_size = atol(optarg);
        FLASH_SIZE_FC_KLRU_CLK = args.flash_size = atol(optarg);
        FLASH_SHILD_FLASH_SIZE = args.flash_size = atol(optarg);
		    FLASH_SIZE_FC_KLRU_CLK_ML = args.flash_size = atol(optarg);
        break;
      case 'D':
        DRAM_SIZE = args.dram_size = atol(optarg);
        DRAM_SIZE_FC_KLRU = args.dram_size = atol(optarg);
        DRAM_SIZE_FC_KLRU_CLK = args.dram_size = atol(optarg);
        FLASH_SHILD_DRAM_SIZE = args.dram_size = atol(optarg);
		    DRAM_SIZE_FC_KLRU_CLK_ML = args.dram_size = atol(optarg);
	      break;
      case 'K':
        K_LRU = atol(optarg);
        break;
      case 'L':
        KLRU_QUEUE_SIZE = atol(optarg);
        break;  
      case 'n':
        args.num_sections =  atol(optarg);
        break;
      case 'd':
        args.num_dsections = atol(optarg);
        break;
      case 'k':
        K = atof(optarg);
        break;
      case 'H':
        L_FC = atol(optarg);
        break;
      case 't':
        args.threshold = atof(optarg);
        break;
      case 'C':
        CLOCK_MAX_VALUE = atol(optarg);
        CLOCK_MAX_VALUE_KLRU = atol(optarg);
        break;
      case 'A':
        args.USER_SVM_TH = atol(optarg);
        break;
      case 'Y':
        args.num_global_partitions = atoi(optarg);
    }
  }
}

/// Factory to create a Policy of specified type with stats object included.
///
/// @param Policy_type the type of Policy to create
///
/// @return std::unique_ptr<Policy> 
std::unique_ptr<Policy> create_Policy(Args& args)
{
  stats sts{Policy_names[args.Policy_type], &args.apps, args.global_mem};
  std::unique_ptr<Policy> Policy{};
   switch(args.Policy_type) {
    case SHADOWLRU : Policy.reset(new shadowlru(sts)); break;
    case FIFO : Policy.reset(new fifo(sts)); break;
    case LRU : 
      Policy.reset(new lru(sts)); 
      break;
    case FLASHCACHE : Policy.reset(new FlashCache(sts)); break;
    case FLASHCACHELRUK : Policy.reset(new FlashCacheLruk(sts)); break;
    case FLASHCACHELRUKCLK : Policy.reset(new FlashCacheLrukClk(sts)); break;
    case FLASHCACHELRUKCLKMACHINELEARNING :
        APP_NUMBER = *std::begin(args.apps);
        ML_SVM_TH = args.USER_SVM_TH;
        Policy.reset(new FlashCacheLrukClkMachineLearning(sts));
        break;
    case VICTIMCACHE : Policy.reset(new VictimCache(sts)); break;
    case LRUK : Policy.reset(new Lruk(sts)); break;
    case CLOCK : Policy.reset(new Clock(sts)); break;
    case SEGMENT_UTIL : Policy.reset(new SegmentUtil(sts)); break;
    case SLAB :
      if (args.memcachier_classes) { sts.gfactor = 2.0; } 
      else { sts.gfactor = args.gfactor; }
      sts.memcachier_classes = args.memcachier_classes;
      Policy.reset(new slab(sts));
      break;
    case SHADOWSLAB:
      if (args.memcachier_classes) { sts.gfactor = 2.0;} 
      else { sts.gfactor = args.gfactor; }
      sts.memcachier_classes = args.memcachier_classes;
      Policy.reset(new shadowslab(sts));
      break;
    case PARTSLAB:
      sts.partitions = args.partslab_partitions;
      Policy.reset(new partslab(sts));
      break;
    case LSM:
      sts.segment_size = args.segment_size;
      sts.cleaning_width = args.cleaning_width;
      Policy.reset(new lsm(sts));
      break;
    case MULTI:
      sts.segment_size = args.segment_size;
      sts.cleaning_width = args.cleaning_width;
      {
        lsc_multi* multi = new lsc_multi(sts, args.Subpolicy);
        Policy.reset(multi);
        for (size_t appid : args.apps) {
          assert(memcachier_app_size[appid] > 0);
          uint32_t app_steal_size = 65536;
          auto it = args.app_steal_sizes.find(appid);
          if (it != args.app_steal_sizes.end()) {
            app_steal_size = it->second;
          } 
          size_t private_mem = args.use_percentage ? 
            (size_t)(memcachier_app_size[appid] * args.priv_mem_percentage) :
            args.min_mem_pct;         
          multi->add_app(appid,
                         private_mem ,
                         memcachier_app_size[appid],
                         app_steal_size);
        }
        if (args.use_tax) {
          multi->set_tax(args.tax_rate);
        }
      }
      break;
    case MULTISLAB:
    {
      if (args.memcachier_classes) {
        sts.gfactor = 2.0;
      } else {
        sts.gfactor = args.gfactor;
      }
      sts.memcachier_classes = args.memcachier_classes;
      slab_multi* slmulti = new slab_multi(sts);
      Policy.reset(slmulti);
      for (size_t appid : args.apps) {
        assert(memcachier_app_size[appid] > 0);
        slmulti->add_app(appid, args.min_mem_pct, memcachier_app_size[appid]);
      }
      break;
    }
    case RIPQ:
      sts.block_size = args.block_size;
      sts.flash_size = args.flash_size;
      sts.num_sections = args.num_sections;
      Policy.reset(new ripq(sts, args.block_size, args.num_sections, args.flash_size));
      break;
    case RIPQ_SHIELD:
      sts.block_size = args.block_size;
      sts.flash_size = args.flash_size;
      sts.dram_size = args.dram_size;
      sts.num_sections = args.num_sections;
      sts.num_dsections = args.num_dsections;
      Policy.reset(new ripq_shield(sts, args.block_size, args.num_sections,
                                   args.dram_size, args.num_dsections, args.flash_size));
      break;
    case RAMSHIELD:
      sts.threshold = args.threshold;
      sts.flash_size = args.flash_size;
      sts.dram_size = args.dram_size;
      Policy.reset(new RamShield(sts, args.block_size));
      break;
    case RAMSHIELD_FIFO:
      sts.threshold = args.threshold;
      sts.block_size = args.block_size;
      sts.flash_size = args.flash_size;
      sts.dram_size = args.dram_size;
      Policy.reset(new RamShield_fifo(sts, args.block_size));
      break;
    case RAMSHIELD_SEL:
      sts.threshold = args.threshold;
      sts.block_size = args.block_size;
      sts.flash_size = args.flash_size;
      sts.dram_size = args.dram_size;
      Policy.reset(new RamShield_sel(sts, args.block_size));
      break;
    case FLASHSHIELD:
        FLASH_SHILD_APP_NUMBER = *std::begin(args.apps);
        FLASH_SHILD_TH = args.USER_SVM_TH;
        sts.threshold = args.threshold;
        sts.flash_size = args.flash_size;
        sts.dram_size = args.dram_size;
        Policy.reset(new flashshield(sts));
        break;
    case NONE:
        break;
    case PARTITIONED_LRU:
        Policy.reset(new Partitioned_LRU(sts, 1));
        break;
  }
  if (!Policy) {
    std::cerr << "No valid Policy selected!" << std::endl;
    exit(-1);
  }
  else
  {
    std::cout << "Policy created."  << std::endl;
  }
  return Policy; 
}

