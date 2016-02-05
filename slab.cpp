#include "slab.h"

slab::slab(uint64_t size, double growth)
  : policy(size)
  , accesses{}
  , hits{}	
	, growth{growth}
  , current_size{}
	, slabs{}
{
}

slab::~slab () {
}

void slab::proc(const request *r) {
 
  return;
}

uint32_t slab::get_size() {
  uint32_t size = 0;

  return size;
}

void slab::log_header() {
  std::cout << "util util_oh cachesize hitrate" << std::endl;
}

void slab::log() {
  std::cout << double(current_size) / global_mem << " "
            << double(current_size) / global_mem << " "
            << global_mem << " "
            << double(hits) / accesses << std::endl;
}

class slab::slru {
 
 	public:
 		slru(slab & owner)
 		: owner(owner)
		,	chunk{}
 		, hash{}
 		, queue{}
 		{}
  	 ~slru() {}

		void query (const request *r) {
			++(owner.accesses);
			auto it = this->hash.find(r->kid);
			
			if (it != hash.end()) {
				auto& list_it = it->second;
				request& prior_request = *list_it;
				
				if(prior_request.size() == r->size()) {
					++(owner.hits);				
	
					// Promote this item to front of queue.
					queue.emplace_front(prior_request);
					queue.erase(list_it);
					hash[r->kid] = queue.begin();

					return;
				} else {
				
					// Size has changed. Even though it is in the cache it must have already been
					// evicted or show down. Since then it must have already been replaced as well.
					// This means that there must have been some intervening get miss for it. So we
					// need to count an extra access here (but not an extra hit). We do need to remove
					// prior_request from the hash table, but it gets overwritten below anyway when r 
					// gets put in the cache.
				
					// Count the get/miss that came between r and prior_request.
					++(owner.accesses);
					// Finally, we need to really put the correct sized value somewhere
					// in the LRU queue. So fall through to the evict+insert clauses.
				}
			}

			// Throw out enough junk to make room for the new record.
			while (owner.global_mem - owner.current_size < uint32_t(r->size())) {
				// If the queue is already empty, then we are in trouble. The cache
				// just isn't big enough to hold this object under any circumstances.
				// Though, you probably shouldn't be setting the cache size smaller
				// than the max memcache object size.
				if(queue.empty())
					return;

				request* victim = &queue.back();
				owner.current_size -= victim->size();
				hash.erase(victim->kid);
				queue.pop_back();
			}
		
			// Add the new request.
			queue.emplace_front(*r);
			hash[r->kid] = queue.begin();
			owner.current_size += r->size();
		
			// Count this request as a hit.
			++(owner.hits);

			return;
		}

 
 	private:

	slab & owner;

	// Chunk size for this slab class 
 	uint32_t chunk;	
 
	// Fast indexing for items in the LRU queue.
 	hash_map hash;
 
	lru_queue queue;	
 
};

