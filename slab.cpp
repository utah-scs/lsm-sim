#include "slab.h"
#include "lru.h"

slab::slab(uint64_t size)
  : policy(size)
  , accesses{}
  , hits{}	
	, growth{DEF_GFACT}
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

		uint32_t get_current_size(void) { return current_size; }	
		uint32_t get_free(void) { return this->global_mem - current_size; }
		void alloc(size_t size) { this->global_mem += size; }

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

		if((global_mem - current_size) < PAGE_SIZE) {	
			// mimic memcached behavior (say no to new slab classes when full)
			std::cout << "Not enough memory left to alloc a new slab class" << std::endl;
			return;
		}
		
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

	sclru *s = it->second;

	// Check current capacity of slab class, if there is not enough room
	// we need to allocate a new page. (increase the size of slab) and then
	// process the request normally. If, however, we are out of global mem
	// we cannot expand the slab, so just process the request and let LRU 
	// take care of the eviction/put process.
	if(s->get_free() < (size_t)r->size()) {
		if(global_mem - current_size >= PAGE_SIZE)
		s->alloc(PAGE_SIZE);	
	}	

	// If we reach this point it's safe to process the request.
	it->second->proc(r);

  return;
}

// Returns the overall memory allocated across all slabs.
uint32_t slab::get_size() {
  uint32_t size = 0;

 	for (const auto& s: slabs)
		size += s.second->get_current_size();

  return size;
}

void slab::log_header() {
  std::cout << "util util_oh cachesize hitrate" << std::endl;
}

void slab::log() {

	std::cout << "print progress" << std::endl;

  std::cout << double(get_size()) / global_mem << " "
            << double(get_size()) / global_mem << " "
            << global_mem << " "
            << double(hits) / accesses << std::endl;
}


 


