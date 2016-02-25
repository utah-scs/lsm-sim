#ifndef LRU_H
#define LRU_H

#include <unordered_map>
#include <list>
#include "policy.h"

class lru : public policy {
  typedef std::list<request> lru_queue; 

  typedef std::unordered_map<uint32_t, lru_queue::iterator> hash_map;

  public:
    lru(const std::string& filename_suffix, uint64_t size);
   ~lru();

    // Modifiers.
    size_t proc (const request *r, bool warmup);
    int64_t remove (const request *r);
  
    // Accessors.
    size_t get_bytes_cached();
    size_t get_hits(); 
    size_t get_accs();
    void log();

  protected:
    // Number of access requests fed to the cache.
    size_t accesses;

    // Subset of accesses which hit in the simulated cache.
    size_t hits;

		// The number of bytes currently cached.
    size_t bytes_cached; 

    hash_map hash; 
    lru_queue queue; 
};

#endif
