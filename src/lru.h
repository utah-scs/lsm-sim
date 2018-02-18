#ifndef LRU_H
#define LRU_H

#include <unordered_map>
#include <list>
#include "policy.h"

class LRU : public Policy 
{

public:
  LRU();
  LRU(stats stat);
  ~LRU();
  
  // Performs lookup of request. 
  // (Conforms to Policy.h interface)
  //
  // If request already exists in cache and the existing request size is the
  // same with respect to the new request size, the existing request is promoted
  // to the front of the LRU chain and the cache remains otherwise unchanged.
  //
  // If the size has changed, the existing request is removed and the new
  // request request is added to the cache and placed at the front of the LRU
  // chain.
  //
  // If the request does not already exist in the cache, it is added and placed
  // at the fron of the LRU queue.
  //
  // @param r - a request object to be added to the queue.
  // @param warmup - boolean flag to indicate if warmup period is still active. 
  //
  // @return - an unsigned value representing the absolute value of change in
  //           bytes due to processing this request. i.e. bytes evicted - 
  //           bytes cached
  size_t process_request(const Request* r, bool warmup); 

  /// Constant Accessors. ///

  // Reports the current number of bytes utilized by cached request requests.
  // (Conforms to Policy.h interface)
  //
  // @return - Number of bytes utilized by cached request requests.
  size_t get_bytes_cached() const;                        

  // Used by policies that aggregate LRUs i.e. slab_multi, slab to report if 
  // adding a request would result in an eviction. The operation of actually
  // adding the request is not performed by this function and the cache remains
  // unchanged.
  bool would_cause_eviction(const Request& request) const;

  // Used by policies that aggregate LRUs i.e. lsc_multi to report if adding a
  // request would result in a hit. The operations of actually adding the
  // request is not performed by this function and the cache remains unchanged.
  bool would_hit(const Request& request) const;

  // Used by policies that aggreagte LRUs i.e. lsc_multi, to report the number
  // of bytes cached for each application that is utilizing the cache. 
  std::unordered_map<int32_t, size_t> get_per_app_bytes_in_use() const;

  /// Public Modifiers. ///

  // Used by policies that aggregate LRUs i.e. lsc_multi.
  bool try_add_tail(const Request *r);

  // Used by policies that aggreagte LRUs i.e. slab_multi.
  int64_t remove (const Request *r);

  // Used by policies that aggreagte LRUs i.e. slab, slab_multi.
  // Specifically, this is used to expand the size of a particular slab class.
  //
  // @param bytes - number of bytes by which to increase the size of the slab.
  void expand(const size_t bytes);

private:
  // Adds a request to the cache.
  //
  // If adequate space to add the request does not exist, existing requests are
  // evicted from the cache until adequate space is obtained. If the cache
  // becomes empty before adequate space is obtained, the request fails to be
  // added to the cache.
  // 
  // @param request - A request to be added to the queue.
  size_t add_request(const Request& request);

  std::unordered_map<uint32_t, std::list<Request>::iterator> map;
  std::list<Request> queue; 
};

#endif
