#include <list>
#include <unordered_map>
#include <vector>
#include <deque>
#include <algorithm>

#include "common.h"
#include "lru.h"
#include "policy.h"

#ifndef LSC_MULTI_H
#define LSC_MULTI_H

class lsc_multi : public Policy {
  public:
    enum class Cleaning_policy { RANDOM, RUMBLE, ROUND_ROBIN, OLDEST_ITEM, LOW_NEED };
    enum class Subpolicy { NORMAL, GREEDY, STATIC };

    static constexpr size_t idle_mem_secs = 5 * 60 * 60;

  private:
    class segment;

    class item {
      public:
        item(segment* seg, const Request& req)
          : seg{seg}
          , req{req}
        {}

        // Sorting by Request timestamp is ok, because we also
        // store the Request object even when the access is a
        // hit to an existing object. That is, timestamps
        // represent access times as a result.
        static bool rcmp(const item* left, const item* right) {
          return left->req.time > right->req.time;
        }

        segment* seg;
        Request req;
    };

    typedef std::list<item> LRU_queue; 
    typedef std::unordered_map<uint32_t, LRU_queue::iterator> hash_map;

    class segment {
      public:
        segment()
          : queue{}
          , app_bytes{}
          , filled_bytes{}
          , access_count{}
          , low_timestamp{}
        {}

        LRU_queue queue;
        std::unordered_map<int32_t, size_t> app_bytes;

        size_t filled_bytes;

        uint64_t access_count;
        double low_timestamp;
    };

    class application {
      public:
        application(size_t appid,
                    size_t min_mem_pct,
                    size_t target_mem,
                    size_t steal_size);
        ~application();

        size_t bytes_limit() const {
          return target_mem + credit_bytes;
        }

        bool try_steal_from(application& other, size_t bytes) {
          if (other.bytes_limit() < bytes)
            return false;

          if (&other == this)
            return false;

          const size_t would_become = other.bytes_limit() - bytes;
          if (would_become < other.min_mem)
            return false;

          other.credit_bytes -= bytes;
          credit_bytes += bytes;

          return true;
        }

        bool would_hit(const Request *r) {
          return shadow_q.would_hit(*r);
        }

        void add_to_cleaning_queue(item* item) {
          cleaning_q.emplace_back(item);
        }

        void sort_cleaning_queue() {
          std::sort(cleaning_q.begin(),
                    cleaning_q.end(),
                    item::rcmp);
          cleaning_it = cleaning_q.begin();
        }

        static void dump_stats_header() {
          std::cout << "time "
                    << "app "
                    << "Subpolicy "
                    << "target_mem "
                    << "credit_bytes "
                    << "share "
                    << "min_mem "
                    << "min_mem_pct "
                    << "steal_size "
                    << "bytes_in_use "
                    << "need "
                    << "hits "
                    << "accesses "
                    << "shadow_q_hits "
                    << "survivor_items "
                    << "survivor_bytes "
                    << "evicted_items "
                    << "evicted_bytes "
                    << "hit_rate "
                    << std::endl;
        }

        void dump_stats(double time, Subpolicy Policy) {
          const char* Policy_name[3] = { "normal"
                                       , "greedy"
                                       , "static"
                                       };
          std::cout << int64_t(time) << " "
                    << appid << " "
                    << Policy_name[uint32_t(Policy)] << " "
                    << target_mem << " "
                    << credit_bytes << " "
                    << target_mem + credit_bytes << " "
                    << min_mem << " "
                    << min_mem_pct << " "
                    << steal_size << " "
                    << bytes_in_use << " "
                    << need() << " "
                    << hits << " "
                    << accesses << " "
                    << shadow_q_hits << " "
                    << survivor_items << " "
                    << survivor_bytes << " "
                    << evicted_items << " "
                    << evicted_bytes << " "
                    << double(hits) / accesses << " "
                    << std::endl;
        }

        double need() {
          return double(target_mem + credit_bytes) / bytes_in_use;
        }

        const size_t appid;
        const size_t min_mem_pct;
        const size_t target_mem;
        const size_t min_mem;
        const size_t steal_size;

        ssize_t credit_bytes;

        size_t bytes_in_use;

        size_t accesses;
        size_t hits;
        size_t shadow_q_hits;
        size_t survivor_items;
        size_t survivor_bytes;
        size_t evicted_items;
        size_t evicted_bytes;

        LRU shadow_q;

        std::deque<item*> cleaning_q;
        std::deque<item*>::iterator cleaning_it;
    };

  public:
    lsc_multi(stats sts, Subpolicy eviction_policy);
    ~lsc_multi();

    void add_app(size_t appid,
                 size_t min_memory_pct,
                 size_t target_memory,
                 size_t steal_size);

    void set_tax(double tax_rate)
    {
      use_tax = true;
      this->tax_rate = tax_rate;
    }

    size_t process_request(const Request *r, bool warmup);
    size_t get_bytes_cached() const;
   
    double get_running_hit_rate();
    size_t get_evicted_bytes() { return stat.evicted_bytes; }
    size_t get_evicted_items() { return stat.evicted_items; }

    void dump_util(const std::string& filename);

    virtual void dump_stats(void) {}

  private:

    void rollover(double timestamp);
    void clean();
    void dump_usage();

    std::vector<segment*> choose_cleaning_sources();
    std::vector<segment*> choose_cleaning_sources_random();
    std::vector<segment*> choose_cleaning_sources_rumble();
    std::vector<segment*> choose_cleaning_sources_round_robin();
    std::vector<segment*> choose_cleaning_sources_oldest_item();
    std::vector<segment*> choose_cleaning_sources_low_need();
    segment* choose_cleaning_destination();
    auto choose_survivor() -> application*;

    void dump_cleaning_plan(std::vector<segment*> srcs,
                            std::vector<segment*> dsts);

    void dump_app_stats(double time);

    void compute_idle_mem(double time);

    double last_idle_check;
    double last_dump;

    bool use_tax;
    double tax_rate;

    Subpolicy eviction_policy;
    Cleaning_policy cleaner;

    std::unordered_map<size_t, application> apps;

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
