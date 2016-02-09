#ifndef BAD_IDEA_H
#define BAD_IDEA_H

#include "policy.h"
#include <unordered_map>

// Implements a circular log such that the current position is always resting on
// the LRU item. Each put or update overwites whatever item is at this position
// and advances the position. Thus always overwriting the oldest item. This will
// probably be terrible since an update will always wipe out a potenitally
// valuable item. 

class bad_idea : public policy {

  typedef std::unordered_map<uint32_t, request*> hash_map;
  typedef std::list<request>::iterator iter;

  public:
    bad_idea(uint64_t size);
   ~bad_idea();
    void proc(const request *r);
    uint32_t get_size(); 

    void log_header();
    void log();

  private:
    // Number of access requests fed to the cache.
    size_t accesses;

    // Subset of accesses which hit in the simulated cache.
    size_t hits;

    // the current number of bytes in eviction queue
    uint32_t current_size; 

    // Location in log where next put or update should be placed, immediately
    // following the last put or update, and immediately preceding the next  LRU 
    // item in the log. 
    iter head; 
 
    hash_map hash; 
    std::list<request> queue; 

    

};






#endif
