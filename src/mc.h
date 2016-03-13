#ifndef MC_H 
#define MC_H

//static uint16_t slab_count;

uint16_t slabs_init(const double factor);
std::pair<uint32_t, uint32_t> slabs_clsid(const size_t size);

#endif
