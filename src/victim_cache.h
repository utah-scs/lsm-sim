#ifndef VICTIM_CACHE_H
#define VICTIM_CACHE_H

#include <iostream>
#include <list>
#include <unordered_map>
#include <cassert>
#include "policy.h"
#include "flash_cache.h"

class VictimCache : public Policy {
private:
	typedef std::list<uint32_t>::iterator keyIt;

	std::list<uint32_t> dram;
	std::list<uint32_t> flash;
	
	std::unordered_map<uint32_t, FlashCache::Item> allObjects;	
	
	size_t dramSize;
	size_t flashSize;

	size_t missed_bytes;

	std::ofstream out;
	void insertToDram(FlashCache::Item& item, bool warmup);	
public:
	VictimCache(stats stat);
	~VictimCache();
	size_t process_request(const Request *r, bool warmup);
	size_t get_bytes_cached() const;
	void dump_stats(void);
};

#endif
