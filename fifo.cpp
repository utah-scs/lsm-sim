#include "fifo.h"

fifo::fifo(uint64_t size)
  : policy(size)
  , accesses{}
  , hits{}
  , current_size{}
  , hash{}
  , queue{}
{
}

fifo::~fifo () {
}

// checks the hashmap for membership, if the key is found
// returns a hit, otherwise the key is added to the hash
// and to the FIFO queue and returns a miss.
bool fifo::proc(const request *r) {
  ++accesses;

  auto it = hash.find(r->kid);
  if (it != hash.end()) {
    ++hits;
    return true;
  }

  // Throw out enough junk to make room for new record.
  while (global_mem - current_size < uint32_t(r->size())) {
      request* victim = &queue.back();
      current_size -= victim->size();
      hash.erase(victim->kid);
      queue.pop_back();
  }

  // Add the new request.
  queue.emplace_front(*r);
  hash[r->kid] = &queue.front();
  current_size += r->size();
 
  return false;
}

uint32_t fifo::get_size() {
  uint32_t size = 0;
  for (const auto& r: queue)
    size += r.size();

  return size;
}

void fifo::log_header() {
  std::cout << "util util_oh cachesize hitrate" << std::endl;
}

void fifo::log() {
  std::cout << double(current_size) / global_mem << " "
            << double(current_size) / global_mem << " "
            << global_mem << " "
            << double(hits) / accesses << std::endl;
}
