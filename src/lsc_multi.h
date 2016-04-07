#include <experimental/optional>
#include <list>
#include <unordered_map>
#include <vector>

#include "policy.h"
#include "common.h"

#ifndef LSC_MULTI_H
#define LSC_MULTI_H

template <class T>
using optional = std::experimental::optional<T>;
constexpr auto nullopt = std::experimental::nullopt;


class lsc_multi : public policy {
  private:
    class segment;

    class item {
      public:
        item(segment* seg, const request& req)
          : seg{seg}
          , req{req}
        {}

        segment* seg;
        request req;
    };

    typedef std::list<item> lru_queue; 
    typedef std::unordered_map<uint32_t, lru_queue::iterator> hash_map;

    class segment {
      public:
        segment()
          : queue{}
          , filled_bytes{}
          , access_count{}
          , low_timestamp{}
        {}

        lru_queue queue;
        size_t filled_bytes;

        uint64_t access_count;
        double low_timestamp;
    };

    class application {
      public:
        application(size_t min_mem, size_t target_mem)
        : min_mem{}
        , target_mem{}
        , owed_mem{}
        , average_hits{} 
      {}
 
      size_t min_mem;
      size_t target_mem;
      size_t owed_mem;
      double average_hits;
    };

  public:
    enum class cleaning_policy { RANDOM, RUMBLE, ROUND_ROBIN, OLDEST_ITEM };

    lsc_multi(stats sts);
    ~lsc_multi();

    size_t proc(const request *r, bool warmup);
    size_t get_bytes_cached() const;
   
    double get_running_hit_rate();
    size_t get_evicted_bytes() { return stat.evicted_bytes; }
    size_t get_evicted_items() { return stat.evicted_items; }

    void dump_util(const std::string& filename);

  private:

    void rollover(double timestamp);
    void clean();
    void dump_usage();

    std::vector<segment*> choose_cleaning_sources();
    std::vector<segment*> choose_cleaning_sources_random();
    std::vector<segment*> choose_cleaning_sources_rumble();
    std::vector<segment*> choose_cleaning_sources_round_robin();
    std::vector<segment*> choose_cleaning_sources_oldest_item();
    segment* choose_cleaning_destination();

    void dump_cleaning_plan(std::vector<segment*> srcs,
                            std::vector<segment*> dsts);

    cleaning_policy cleaner;

    std::unordered_map<size_t, application> applications;

    hash_map map; 

    segment* head;
    std::vector<optional<segment>> segments;
    size_t free_segments;

    lsc_multi(const lsc_multi&) = delete;
    lsc_multi& operator=(const lsc_multi&) = delete;
    lsc_multi(lsc_multi&&) = delete;
    lsc_multi& operator=(lsc_multi&&) = delete;
};

#endif
