#include "fifo.h"


typedef std::pair<uint32_t, req_pair*> hash_pair;

Fifo::Fifo(uint64_t size) : Policy(size) {

}

Fifo::~Fifo () {

  std::cout << "DESTROY FIFO" << std::endl;
    
}

bool Fifo::proc(const request *r) {

 // std::cout << "using FIFO" << std::endl;

  // check hash for key, if not found this will
  // be a get/put situation
  hitr i = hash.find(r->kid);
  if(i == hash.end() ) {
    insert_pair(r);
  //  std::cout << "miss" << std::endl;
    return false;
  }
 // std::cout << "hit" << std::endl;
  return true;
}


uint32_t Fifo::insert_pair(const request *r) {

  uint64_t request_size = r->key_sz + r->val_sz;
  req_pair *req = new req_pair(r->kid, request_size);

  // add to hashmap
  hash_pair p (r->kid, req);
  hash.insert (p);

  // if room exists in eviction queue, just add
  if (current_size + request_size <= global_mem)
    eviction_queue.push_front(*req);

  else {
  
    uint32_t freed = 0;

    // remove items from back of queue until enough room
    while(freed < request_size) {
  
     req_pair p = eviction_queue.back();
     freed += p.size;
     hash.erase(p.id);
    }

    // adjust current size offset by removals/addition
    current_size -= freed;
    current_size += request_size;
    
    // finally, place the item 
    eviction_queue.push_front(*req);
  }

  return 0;
}



uint32_t Fifo::get_size() {

  uint32_t size = 0;
  for (const auto& pair : eviction_queue)
    size += pair.size;

  return size;
}
