#include <iostream>

#include "bad_idea.h"

bad_idea::bad_idea(uint64_t size)
  : policy(size)
  , accesses{}
  , hits{}
  , current_size{}
  , head{}
  , hash{}
  , queue{}
{
}

bad_idea::~bad_idea () {
}

// checks the hashmap for membership, if the key is found the item is promoted
// to the current insertion point replacing whatever was already there. If the
// key is not found, the item is added at the insertion point.
size_t bad_idea::proc(const request *r, bool warmup) {
  if (!warmup)
    ++accesses;

  auto it = hash.find(r->kid);
  
  if (it != hash.end()) {
      
  }

  return 0;
}

uint32_t bad_idea::get_size() {
  uint32_t size = 0;
  for (const auto& r: queue)
    size += r.size();

  return size;
}

void bad_idea::log() {
  std::cout << double(current_size) / global_mem << " "
            << double(current_size) / global_mem << " "
            << global_mem << " "
            << double(hits) / accesses << std::endl;
}
