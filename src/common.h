#ifndef COMMON_H
#define COMMON_H

#include <cinttypes>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>

typedef std::vector<std::string> string_vec;

// breaks a CSV string into a vector of tokens
int csv_tokenize(const std::string &s, string_vec *tokens);

struct stats {
  std::string policy;
  size_t appid;
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
  bool   memcachier_classes;
  double gfactor;
  size_t partitions;

  stats(std::string policy, 
        size_t appid, 
        size_t global_mem)
    : policy{policy}
    , appid{appid}
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
    , memcachier_classes{}
    , gfactor{}
    , partitions{}
  {}

  double get_hit_rate() { return double(hits) / accesses; }
  double get_utilization() { return double(bytes_cached) / global_mem; }
};

template <typename T>
std::string to_string_with_precision(const T a_value, const int n = 2)
{
  std::ostringstream out{};
  out << std::setprecision(n) << a_value;
  return out.str();
}

#endif

