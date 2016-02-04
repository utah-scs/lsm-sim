#ifndef FIFO_H
#define FIFO_H

#include "policy.h"
#include <unordered_map>

class fifo : public policy {

  typedef std::unordered_map<uint32_t, req_pair*> hash_map;
  typedef hash_map::const_iterator map_it;
  typedef std::pair<uint32_t, req_pair*> hash_pair;

  public:
    fifo(uint64_t size);
   ~fifo();
    bool proc (const request *r);
    uint32_t get_size(); 

  private:
    uint32_t current_size;  // the current number of bytes in eviction queue
    uint32_t insert_pair(const request *r);
    hash_map hash; 
};






#endif
