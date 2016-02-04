#ifndef CLIFF_H
#define CLIFF_H

#include "policy.h"

// policy derived from Cliffhanger paper
class cliff : public policy {
  public:
    cliff(uint64_t size);
    ~cliff();
    bool proc (const request *r);
    uint32_t get_size();

    void log_header();
    void log();

  private:
    uint32_t get_slab_class(uint32_t size);

    queue eviction_queue;
};




#endif
