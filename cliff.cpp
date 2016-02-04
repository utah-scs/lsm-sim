#include "cliff.h"

cliff::cliff(uint64_t size) : policy(size) {

}


cliff::~cliff() {

  std::cout << "DESTROY CLIFF" << std::endl;

} 



bool cliff::proc(const request *r) {
  
  uint64_t kv_size      = r->key_sz + r->val_sz;
  uint64_t request_size = get_slab_class(kv_size);
  int global_pos = -1;
  uint64_t k = 0;
  global_queue_size = 0;

  //iterate over keys in the lru queue
  for (req_pair &a : eviction_queue) {
    
    k++;
    global_queue_size += a.size;
            
    if (a.id == r->kid) {
            
      global_pos = k;
      global_queue_size += request_size;
      global_queue_size -= a.size;
            
      // remove the request from its current position in the queue
      eviction_queue.remove_and_dispose(a, delete_disposer());
      break;
    }
  }

  // append the request to the back of the LRU queue
  req_pair *req = new req_pair(r->kid, request_size);
  eviction_queue.push_front(*req);

  if(global_pos != -1)
    return global_queue_size <= global_mem;
  else
    return false;
}


uint32_t cliff::get_size() {

  uint64_t size = 0;
  for(const auto& pair : eviction_queue)
    size += pair.size;

  return size;
}



uint32_t cliff::get_slab_class(uint32_t size) {
  if (size < 64)
    return 64;
  --size;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    return size + 1;
}

