#ifndef APPLICATION_H
#define APPLICATION_H

#include<string>

class application {

  public:
    application(size_t min_mem, size_t target_mem);
    ~application();
  
  private:
    size_t min_mem;
    size_t target_mem;
    size_t mem_balance;
    double average_hits;
};

#endif
