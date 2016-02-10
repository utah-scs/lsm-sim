#include <iostream>

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
  init_slabs();
}

slab::~slab () {
}

class slab::sclru : public lru {
  public:
    sclru(slab &owner, size_t chunk_sz)
    : lru(PAGE_SIZE) 
    ,	chunk_sz{chunk_sz}
    , owner(owner)
    {}
   ~sclru() {}

    uint32_t get_current_size(void) { return current_size; }	
    uint32_t get_free(void) { return this->global_mem - current_size; }
    void alloc(size_t size) { this->global_mem += size; }
    size_t get_hits() { return hits; }
    size_t get_accs() { return accesses; }

  private: 
    // Chunk size for this slab class.
    size_t chunk_sz;	

    slab &owner; 
};




int64_t slab::proc(const request *r, bool warmup) {

  // TODO : FIX SO THIS ROUNDS PROPERLY

  // Determine slab class LRU chain for request. 
  size_t size = r->size() > 0 ? r->size() : 0;
  uint32_t chunk = MIN_CHUNK;
  while(size > chunk) 
    chunk *= DEF_GFACT;
  
  auto it = slabs.find(chunk);
  if(it == slabs.end()) {
    std::cerr << "Object size too large for slab config" << std::endl;
    std::cerr << "obj_size: " << size << std::endl;
    return 0;
  }
  
  const auto &s = it->second;
    
  // Check current capacity of slab class, if there is not enough room
  // we need to allocate a new page. (increase the size of slab) and then
  // process the request normally. If, however, we are out of global mem
  // we cannot expand the slab, so just process the request and let LRU 
  // take care of the eviction/put process.
  if(s->get_free() < size) {
    
    if(global_mem - current_size >= PAGE_SIZE && current_size <= global_mem) {
      s->alloc(PAGE_SIZE); 
    }
  }

  // If we reach this point it's safe to process the request. 
  current_size += s->proc(r, warmup);
  return 0;
}


// TODO: FIX SO THIS ROUNDS PROPERLY

uint16_t slab::init_slabs (void) {
  int i = 0;  
  size_t size = MIN_CHUNK;   
 
  std::cout << "Creating slab classes" << std::endl;  
  while(i < MAX_CLASSES && size <= MAX_SIZE / DEF_GFACT) {
    std::cout << i << " " << size << std::endl;    
    sc_ptr sc(new sclru(*this, size));   
    slabs.insert(std::make_pair(size, std::move(sc)));
    size *= DEF_GFACT;
    i++; 
  }  
  return i - 1; 
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
