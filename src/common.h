#include <experimental/optional>
#include <cinttypes>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <set>
#include <cassert>

#ifndef COMMON_H
#define COMMON_H

template <class T>
using optional = std::experimental::optional<T>;

constexpr auto nullopt = std::experimental::nullopt;

typedef std::vector<std::string> string_vec;

// breaks a CSV string into a vector of tokens
int csv_tokenize(const std::string &s, string_vec *tokens);

struct stats {
  std::string policy;
  std::set<uint32_t> apps;
  size_t global_mem; 
  double utilization;
  size_t accesses;
  size_t hits;
  size_t bytes_cached;
  size_t evicted_bytes;
  size_t evicted_items;
  size_t cleaned_bytes;
  size_t cleaned_items;
  size_t segment_size;
  size_t cleaning_width;
  size_t cleaned_generated_segs;
  size_t cleaned_ext_frag_bytes;
  bool   memcachier_classes;
  double gfactor;
  size_t partitions;
  size_t min_mem;
  size_t target_mem;
  size_t epoch_len;
  size_t period_len;
  size_t hits_dram;
  size_t hits_flash;
  size_t writes_flash;
  size_t credit_limit;

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
    , cleaned_bytes{}
    , cleaned_items{}
    , segment_size{}
    , cleaning_width{}
    , cleaned_generated_segs{}
    , cleaned_ext_frag_bytes{}
    , memcachier_classes{}
    , gfactor{}
    , partitions{}
    , min_mem{}
    , target_mem{}
    , epoch_len{}
    , period_len{}
    , hits_dram{}
    , hits_flash{}
    , writes_flash{}
    , credit_limit{}
  {}

  double get_hit_rate() { return double(hits) / accesses; }
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
             "cleaning_width "
             "growth_factor "
             "hits accesses "
             "hit_rate "
             "bytes_cached "
             "evicted_bytes "
             "evicted_items "
             "cleaned_bytes "
             "cleaned_items " 
             "cleaned_generated_segs "
             "cleaned_ext_frag_bytes "
	     "hits_dram "
	     "hits_flash "
	     "writes_flash "
	     "credit_limit "
          << std::endl;
      out << appid << " "
          << policy << " "
          << global_mem << " "
          << segment_size << " "
          << cleaning_width << " "
          << gfactor << " "
          << hits << " "
          << accesses << " "
          << double(hits) / accesses << " "
          << bytes_cached << " "
          << evicted_bytes << " "
          << evicted_items << " "
          << cleaned_bytes << " "
          << cleaned_items << " "
          << cleaned_generated_segs << " "
          << cleaned_ext_frag_bytes << " "
	  << hits_dram << " "
	  << hits_flash << " "
	  << writes_flash << " "
	  << credit_limit << " "
          << std::endl;
  }
  
};

template <typename T>
std::string to_string_with_precision(const T a_value, const int n = 2)
{
  std::ostringstream out{};
  out << std::setprecision(n) << a_value;
  return out.str();
}

#endif

