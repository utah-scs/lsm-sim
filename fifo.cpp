#include "fifo.h"


typedef std::pair<uint32_t, req_pair*> hash_pair;


Fifo::Fifo(uint64_t size) : Policy(size) { 

  this->current_size = 0;
}

Fifo::~Fifo () {

  // for (req_pair &a : eviction_queue)
  //  eviction_queue.remove_and_dispose(a, delete_disposer());
  
  std::cout << "DESTROY FIFO" << std::endl;
}


// checks the hashmap for membership, if the key is found
// returns a hit, otherwise the key is added to the hash
// and to the FIFO queue and returns a miss.
bool Fifo::proc(const request *r) {

  map_it i = hash.find(r->kid);
  
  if(i == hash.end() ) {
    insert_pair(r); 
    return false;
  }
 
  return true;
}

// inserts a request into the hashmap and the FIFO queue
// if adding the kv pair size requires eviction, pull items
// from back of queue until there is room, then place the
// item there.
uint32_t Fifo::insert_pair(const request *r) {

  uint64_t request_size = r->key_sz + r->val_sz;
  req_pair *req         = new req_pair(r->kid, request_size);

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
