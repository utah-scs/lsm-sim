#include "application.h"


application::application(size_t min_mem, size_t target_mem) 
: min_mem{min_mem}
, target_mem{target_mem}
, mem_balance{}
, average_hits{}
{}

application::~application() {}




