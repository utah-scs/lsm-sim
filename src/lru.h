#ifndef LRU_H
#define LRU_H

#include <unordered_map>
#include <list>
#include "policy.h"

class lru : public Policy {

  typedef std::list<Request> lru_queue; 
  typedef std::unordered_map<uint32_t, lru_queue::iterator> hash_map;

  public:
    lru();
    lru(stats stat);
   ~lru();

    // Modifiers.
    size_t process_request(const Request* r, bool warmup);
    int64_t remove (const Request *r);
    bool would_cause_eviction(const Request *r);
    void expand(size_t bytes);
    bool would_hit(const Request *r);
    void add(const Request *r);
    bool try_add_tail(const Request *r);
  
    // Accessors.
    size_t get_bytes_cached() const;
    size_t get_hits(); 
    size_t get_accs();

    double get_running_hit_rate();
    double get_running_utilization();
    size_t get_evicted_bytes() { return stat.evicted_bytes; }
    size_t get_evicted_items() { return stat.evicted_items; }

    std::unordered_map<int32_t, size_t> get_per_app_bytes_in_use() const;

  protected:
    hash_map map; 
    lru_queue queue; 

};

#endif
