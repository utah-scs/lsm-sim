#ifndef FIFO_H
#define FIFO_H

#include "policy.h"
#include <unordered_map>
#include <list>

class fifo : public Policy {
  typedef std::unordered_map<uint32_t, Request*> hash_map;

  public:
    fifo(stats stat);
   ~fifo();

    size_t process_request(const Request *r, bool warmup); 
    size_t get_bytes_cached() const;

  private:
    uint32_t current_size;  // the current number of bytes in eviction queue

    hash_map hash; 
    std::list<Request> queue; 
};






#endif
