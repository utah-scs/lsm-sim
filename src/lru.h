#ifndef LRU_H
#define LRU_H

#include <unordered_map>
#include <list>
#include "policy.h"

class lru : public policy {
  typedef std::list<request> lru_queue; 

  typedef std::unordered_map<uint32_t, lru_queue::iterator> hash_map;

  public:
    lru();
    lru(stats stat);
   ~lru();

    // Modifiers.
    size_t proc (const request *r, bool warmup);
    int64_t remove (const request *r);
    bool would_cause_eviction(const request *r);
    void expand(size_t bytes);
    bool would_hit(const request *r);
    void add(const request *r);
    bool try_add_tail(const request *r);
  
    // Accessors.
    size_t get_bytes_cached() const;
    size_t get_hits(); 
    size_t get_accs();

    double get_running_hit_rate();
    double get_running_utilization();
    size_t get_evicted_bytes() { return stat.evicted_bytes; }
    size_t get_evicted_items() { return stat.evicted_items; }

  protected:
    hash_map map; 
    lru_queue queue; 

};

#endif
