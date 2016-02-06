#include "slab.h"
#include "lru.h"

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


class slab::sclru : public lru {

 	public:
 		sclru(size_t size, slab &owner, size_t chunk_sz)
 		: lru(size)
		, owner(owner)
		,	chunk_sz{chunk_sz}
 		{}
  	 ~sclru() {}

 	private:

	// Report hits to the owner class instead of locally.
	void inc_hits(void) override { ++(owner.hits); }
	void inc_acss(void) override { ++(owner.accesses); }

	slab &owner;

	// Chunk size for this slab class 
 	size_t chunk_sz;	
  
};

void slab::proc(const request *r) {

	// Determine slab class LRU chain for request. 
	size_t size = (size_t)r->size();
	uint32_t chunk = MIN_CHUNK;
	while(size > chunk)
		chunk *= DEF_GFACT;

	// If the LRU chain exists just perform the request query. 	
	// If slab class does not exist, create the slab and and 
  // add to slab container. Creation of new slab class is ignored
	// if there is not enough free global memory to slice off a 1MB page. 
	auto it = slabs.find(chunk);
	if(it == slabs.end()) {
		if((global_mem - current_size) < PAGE_SIZE)
			return;

		// This should not fail, but just in case log it and still
		// count it as an access.
		auto s = slabs.emplace(chunk, new sclru(PAGE_SIZE, *this, chunk));
		if(!s.second) {
			std::cerr << "Failed to allocate new SCLRU << " << std::endl;
			++accesses;
			return;
		}				
		it = s.first;
	}

	// If we reach this point it's safe to process the request.
	it->second->proc(r);
	
  return;
}

// Returns the overall memory allocated across all slabs.
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


 


