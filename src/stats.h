#include <string>
#include <set>

#ifndef STATS_H
#define STATS_H

/** Tracks detailed statistics for a particular run of a particular policy.
 * Policies accumulate statistcs in an instance of this class; they dump these
 * stats at the end of a run. The stats format is designed to be easily
 * parsable.
 *
 * Individual policies only use subsets of these fields.
 */
struct stats {
  /// Policy used for this run.
  std::string policy;

  /**
   * The set of appids that this policy was fed. Most policies just treat
   * all of the requests as coming from one application. Others like
   * #lsc_multi track application request separately and tune eviction
   * for each separation application.
   */
  std::set<uint32_t> apps;

  /**
   * The total amount of data that can be stored in the cache. Policies count
   * both the size of a request's key and its data against this, but not
   * metadata (e.g. LRU pointers).
   */
  size_t global_mem; 

  /// Ratio of total size of all keys and data to #global_mem.
  double utilization;

  /// Number of get requests this cache has processed.
  size_t accesses;

  /// Number of #accesses that resulted in a cache hit.
  size_t hits;

  /// Total size of all keys and values stored in the cache.
  size_t bytes_cached;

  /// Number size of all keys and values evicted by the cache.
  size_t evicted_bytes;


  /// Number of key/value pairs evicted from the cache.
  size_t evicted_items;

  /**
   * For lsm and lsc_multi, size of cleaning units. This has several
   * complicated effects including internal fragmentation, accuracy
   * of the cleaning policy, and the resource consumption of cleaning.
   */
  size_t segment_size;

  // For ripq and ripq_shield, size of blocks (items in flash are accomodated in blocks)
  size_t block_size;

  // For ripq and ripq_shield, number of sections in flash
  int num_sections;

  // For ripq and ripq_shield, number of sections (i.e. queues) in dram
  int num_dsections;

  /**
   * For lsm and lsc_multi, number of segments to clean in a single pass.
   * Each pass cleans #cleaning_width segments down into #cleaning_width - 1
   * segments.  This has several complicated effects including internal
   * fragmentation, accuracy of the cleaning policy, and the resource
   * consumption of cleaning.
   */
  size_t cleaning_width;

  /// For lsm and lsc_multi, number of survivor segments created by cleaner.
  size_t cleaned_generated_segs;

  /**
   * For lsm and lsc_multi, total amount of unused space left at the end of
   * survivor segments.
   */
  size_t cleaned_ext_frag_bytes;

  /**
   * For slab-style caches; use the exact same slab classes as memcachier did.
   * Starts classes at 64B increasing by factors of 2 up to 1MB.
   */
  bool   memcachier_classes;


  /**
   * For slab-style caches; use the exact same slab class as memcached does
   * for this growth factor.  Starts classes at 48B increasing by factors of
   * #gfactor.
   */
  double gfactor;

  /// Only for partslab; number of hash partitions to split the slabs across.
  size_t partitions;

  /// For flash_cache, victim_cache, ripq, ripq_shield; number of #hits that hit from an item in DRAM.
  size_t hits_dram;

  /// For flash_cache, victim_cache, ripq, ripq_shield; number of #hits that hit from an item in Flash (including active blocks).
  size_t hits_flash;

  /// For flash_cache + victim_cache; total number of items ever written to Flash.
  size_t writes_flash;

  /// For flash_cache; used to pace evictions to flash.
  size_t credit_limit;

  /// For flash_cache + victim_cache; total size of all bytes ever written to Flash. 
  size_t flash_bytes_written;

  /// For ripq_shield, size of DRAM cache (excluding active blocks). 
  size_t dram_size;

  // For ripq and ripq_shield, size of Flash cache (excluding active blocks).
  size_t flash_size;

  stats(const std::string& policy, 
        const std::set<uint32_t>& apps, 
        size_t global_mem)
    : policy{policy}
    , apps{apps}
    , global_mem{global_mem} 
    , utilization{}
    , accesses{}
    , hits{}
    , bytes_cached{}
    , evicted_bytes{}
    , evicted_items{}
    , segment_size{}
    , block_size{}
    , num_sections{}
    , num_dsections{}
    , cleaning_width{}
    , cleaned_generated_segs{}
    , cleaned_ext_frag_bytes{}
    , memcachier_classes{}
    , gfactor{}
    , partitions{}
    , hits_dram{}
    , hits_flash{}
    , writes_flash{}
    , credit_limit{}
    , flash_bytes_written{}
    , dram_size{}
    , flash_size{}
  {}

  /// Return the ratio of access that resulted in a cache hit.
  double get_hit_rate() { return double(hits) / accesses; }

  /// Return the ratio of total cache memory that contains cached data.
  double get_utilization() { return double(bytes_cached) / global_mem; }

  void dump(std::ofstream& out) const {
      assert(apps.size() == 1);
      uint32_t appid = 0;
      for (const auto& app : apps)
        appid = app;
      out << "app "
             "policy "
             "global_mem "
             "segment_size "
             "block_size "
             "num_sections "
             "num_dsections "
             "cleaning_width "
             "growth_factor "
             "hits accesses "
             "hit_rate "
             "bytes_cached "
             "evicted_bytes "
             "evicted_items "
             "cleaned_generated_segs "
             "cleaned_ext_frag_bytes "
             "hits_dram "
             "hits_flash "
             "writes_flash "
             "credit_limit "
             "flash_bytes_written "
             "dram_size "
             "flash_size "
          << std::endl;
      out << appid << " "
          << policy << " "
          << global_mem << " "
          << segment_size << " "
          << block_size << " "
          << num_sections << " "
          << num_dsections << " "
          << cleaning_width << " "
          << gfactor << " "
          << hits << " "
          << accesses << " "
          << double(hits) / accesses << " "
          << bytes_cached << " "
          << evicted_bytes << " "
          << evicted_items << " "
          << cleaned_generated_segs << " "
          << cleaned_ext_frag_bytes << " "
          << hits_dram << " "
          << hits_flash << " "
          << writes_flash << " "
          << credit_limit << " "
          << flash_bytes_written << " "
          << dram_size << " "
          << flash_size << " "
          << std::endl;
  }
  
};

#endif
