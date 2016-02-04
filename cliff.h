#ifndef CLIFF_H
#define CLIFF_H

#include "policy.h"

// policy derived from Cliffhanger paper
class cliff : public policy {
  public:
    cliff(uint64_t size);
    ~cliff();
    void proc(const request *r);
    uint32_t get_size();

    void log_header();
    void log();

  private:
    uint32_t get_slab_class(uint32_t size);

    // Number of access requests fed to the cache.
    size_t accesses;

    // Subset of accesses which hit in the simulated cache.
    size_t hits;

    queue eviction_queue;
};




#endif
