#include "slab.h"

slab::slab(uint64_t size)
  : policy(size)
  , accesses{}
  , hits{}
	, slabs{}
  , current_size{}
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
