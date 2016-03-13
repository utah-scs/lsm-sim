#include <iostream>

#include "slab.h"
#include "lru.h"

slab::slab(const std::string& filename_suffix, uint64_t size)
  : policy{filename_suffix, size}
	, growth{DEF_GFACT}
  , current_size{}
  , slab_for_key{}
	, slabs{}
{
  init_slabs();
}

slab::~slab () {
}

class slab::sclru : public lru {
  public:
    sclru(slab* owner, size_t chunk_sz)
    : lru("", PAGE_SIZE) 
    ,	chunk_sz{chunk_sz}
    , owner{owner}
    {}
   ~sclru() {}

    uint32_t get_free(void) { return global_mem - bytes_cached; }
    void alloc(size_t size) { global_mem += size; }
    size_t get_hits() { return hits; }
    size_t get_accs() { return accesses; }
    size_t get_global_mem() { return global_mem; }
    size_t get_bytes_cached() { return bytes_cached; }
  private: 
    // Chunk size for this slab class.
    size_t chunk_sz;	

    slab* owner; 

    sclru(const sclru&) = delete;
    sclru& operator=(const sclru&) = delete;
    sclru(const sclru&&) = delete;
    sclru& operator=(sclru&&) = delete;
};




size_t slab::proc(const request *r, bool warmup) {

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

  // See if a slab assignment already exists for this key. If so compare the
  // size of the slab where the key is currently with the chunk size for the
  // request. If processing would result in reclassification, remove the 
  // current association and add the new one.
  auto csit = slab_for_key.find(r->kid);
  if(csit != slab_for_key.end()) {
    if(csit->second != chunk) {
      auto nsit = slabs.find(csit->second);
      if(nsit != slabs.end()) {
        nsit->second->remove(r);
        slab_for_key.insert(ks_pr(r->kid, chunk));
      }
      else
        std::cerr << "Expected slab class for corresponding key" << std::endl; 
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
    sc_ptr sc(new sclru(this, size));   
    slabs.insert(std::make_pair(size, std::move(sc)));
    size *= DEF_GFACT;
    i++; 
  }  
  return i - 1; 
}
size_t slab::get_bytes_cached() {
size_t b = 0;  
  for (const auto &p : slabs)
    b += p.second->get_bytes_cached(); 
  return b; 
}



// Iterate over the slab classes and sum hits.
size_t slab::hits() {
size_t h = 0;
  for (const auto &p : slabs) 
    h += p.second->get_hits();
  return h; 
}

// Iterate over the slab classes and sum accesses.
size_t slab::accesses() {
size_t a = 0;
  for (const auto &p : slabs)
    a += p.second->get_accs();
  return a;
}

size_t slab::global_cache_size() {
size_t c = 0;
  for (const auto &p : slabs)
    c += p.second->get_global_mem();
  return c;
}

void slab::log() {

  size_t gbl = global_cache_size();

  std::cout << double(current_size) / gbl << " "
            << double(current_size) / gbl << " "
            << gbl << " "
            << double(hits()) / accesses() << std::endl;
}
