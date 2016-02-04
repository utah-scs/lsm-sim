#ifndef CLIFF_H
#define CLIFF_H

#include "policy.h"

// policy derived from Cliffhanger paper
class cliff : public policy {

  private:
    uint32_t get_slab_class(uint32_t size);

  public:
    cliff(uint64_t size);
    ~cliff();
    bool proc (const request *r);
    uint32_t get_size();

    void log_header();
    void log();
};




#endif
