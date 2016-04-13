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

class lsc_multi : public policy {
  private:
    class segment;

    class item {
      public:
        item(segment* seg, const request& req)
          : seg{seg}
          , req{req}
        {}

        static bool rcmp(const item* left, const item* right) {
          return left->req.time > right->req.time;
        }

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
        application(size_t appid, size_t min_mem, size_t target_mem);
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
          if (would_become < min_mem)
            return false;

          other.credit_bytes -= bytes;
          credit_bytes += bytes;

          return true;
        }

        size_t proc(const request *r) {
          return shadow_q.proc(r, false);
        }

        void add_to_cleaning_queue(item* item) {
          cleaning_q.emplace_back(item);
        }

        void sort_cleaning_queue() {
          std::sort(cleaning_q.begin(),
                    cleaning_q.end(),
                    item::rcmp);
          cleaning_it = cleaning_q.begin();
          std::cout << "App " << appid
                    << " target_mem " << target_mem
                    << " credit_bytes " << credit_bytes
                    << " share " << target_mem + credit_bytes
                    << " min_mem " << min_mem
                    << " bytes_in_use " << bytes_in_use
                    << " need " << need()
                    << " hits " << hits
                    << " accesses " << accesses
                    << " shadow_q_hits " << shadow_q_hits
                    << " hit_rate " << double(hits) / accesses
                    << std::endl;
        }

        double need() {
          return double(target_mem + credit_bytes) / bytes_in_use;
        }

        const size_t appid;
        const size_t min_mem;

        const size_t target_mem;
        ssize_t credit_bytes;

        size_t bytes_in_use;

        size_t accesses;
        size_t hits;
        size_t shadow_q_hits;

        lru shadow_q;

        std::deque<item*> cleaning_q;
        std::deque<item*>::iterator cleaning_it;
    };

  public:
    enum class cleaning_policy { RANDOM, RUMBLE, ROUND_ROBIN, OLDEST_ITEM };

    lsc_multi(stats sts);
    ~lsc_multi();

    void add_app(size_t appid, size_t min_memory, size_t target_memory);

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
    auto find_app_most_in_need() -> application*;

    void dump_cleaning_plan(std::vector<segment*> srcs,
                            std::vector<segment*> dsts);

    cleaning_policy cleaner;

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
