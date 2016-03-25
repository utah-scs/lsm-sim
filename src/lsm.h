#include <experimental/optional>
#include <list>
#include <unordered_map>
#include <vector>

#include "policy.h"
#include "common.h"

#ifndef LSM_H
#define LSM_H

template <class T>
using optional = std::experimental::optional<T>;
constexpr auto nullopt = std::experimental::nullopt;

class lsm : public policy {
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
        {}

        lru_queue queue;
        size_t filled_bytes;
    };

  public:
    lsm(stats sts);
    ~lsm();

    size_t proc(const request *r, bool warmup);
    size_t get_bytes_cached() const;
    void log();

    double get_running_hit_rate();
    size_t get_evicted_bytes() { return stat.evicted_bytes; }
    size_t get_evicted_items() { return stat.evicted_items; }

    void dump_util(const std::string& filename);

  private:
    void rollover();
    void clean();
    void dump_usage();

    std::vector<segment*> choose_cleaning_sources();
    segment* choose_cleaning_destination();

    void dump_cleaning_plan(std::vector<segment*> srcs,
                            std::vector<segment*> dsts);
    hash_map map; 

    segment* head;
    std::vector<optional<segment>> segments;
    size_t free_segments;


    lsm(const lsm&) = delete;
    lsm& operator=(const lsm&) = delete;
    lsm(lsm&&) = delete;
    lsm& operator=(lsm&&) = delete;
};

#endif
