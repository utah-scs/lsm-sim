#include <iostream>
#include "clock.h"

size_t CLOCK_MAX_VALUE = 15;

Clock::Clock(stats stat) : 
		Policy(stat), 
		clockLru(), 
		allObjects(),
		lruSize(0),
		noZeros(0),
		firstEviction(false),
		clockIt()
{
}

Clock::~Clock() {}

size_t Clock::get_bytes_cached() const { return lruSize; }

size_t Clock::process_request(const Request* r, bool warmup) {
	if (!warmup) {stat.accesses++;}

	auto searchRKId = allObjects.find(r->kid);
	if (searchRKId != allObjects.end()) {
		Clock::ClockItem& item = searchRKId->second;
		ClockLru::iterator& clockItemIt = item.clockLruIt;

		if (r->size() == item.size ) { 
			if (!warmup) {stat.hits++;}
			clockItemIt->second = CLOCK_MAX_VALUE;
			return 1;
		} else {
			if (clockItemIt == clockIt) {
				clockIt++;
				if (clockIt == clockLru.end()) {
					clockIt = clockLru.begin();
				}
			}
			clockLru.erase(clockItemIt);
			lruSize -= item.size;
			allObjects.erase(item.kId);
		}	
	}
	Clock::ClockItem newItem;
	newItem.kId = r->kid;
	newItem.size = r->size();
	
	assert((size_t)newItem.size <= stat.global_mem);

	while (lruSize + newItem.size > stat.global_mem) {	
		if (!firstEviction) { firstEviction = true;}		
		bool isDeleted = false;
		ClockLru::iterator tmpIt, startIt = clockIt;

		while(clockIt != clockLru.end()) {
			assert(clockIt->second <= CLOCK_MAX_VALUE);
			if (clockIt->second == 0) {
				tmpIt = clockIt;
				tmpIt++;
				deleteItem(clockIt->first);
				isDeleted = true;
				break;
			} else {
				clockIt->second--;
			}
			clockIt++;
		}
		if (!isDeleted) {
			clockIt = clockLru.begin();
			assert(clockIt->second <= CLOCK_MAX_VALUE);
			while (clockIt != startIt) {
				if (clockIt->second == 0) {
					tmpIt = clockIt;
					tmpIt++;
					deleteItem(clockIt->first);
					isDeleted = true;
					break;
				} else {
					clockIt->second--;
				}
				clockIt++;
			}
		}
		if (!isDeleted) {
			if (!warmup) { noZeros++;}
			assert(clockLru.size() > 0);
			assert(clockIt != clockLru.end());
			tmpIt = clockIt;
			tmpIt++;
			deleteItem(clockIt->first);
		}				
		clockIt = (tmpIt == clockLru.end()) ? clockLru.begin() : tmpIt;
	}
	
	std::pair<uint32_t,size_t> p;
	p = firstEviction 
			? std::make_pair(newItem.kId, CLOCK_MAX_VALUE)
			: std::make_pair(newItem.kId, (size_t) 0);
	if (clockLru.size() == 0) {
		clockLru.emplace_front(p);
		clockIt = clockLru.begin();
		newItem.clockLruIt = clockLru.begin();
	} else {
		clockLru.insert(clockIt,p);
		ClockLru::iterator it = clockIt;
		it--;
		newItem.clockLruIt = it;
	}
	allObjects[newItem.kId] = newItem;
	lruSize += newItem.size;
	return PROC_MISS;
}


void Clock::deleteItem(uint32_t keyId) {
	auto searchRKId = allObjects.find(keyId);
	assert(searchRKId != allObjects.end());
	Clock::ClockItem& item = searchRKId->second;
	clockLru.erase(item.clockLruIt);
	lruSize -= item.size;
	allObjects.erase(keyId);
}

void Clock::dump_stats(void) {
	assert(stat.apps->size() == 1);
	uint32_t appId = 0;
	for(const auto& app : *stat.apps) {appId = app;}	
	std::string filename{stat.policy
			+ "-app" + std::to_string(appId)
			+ "-globalMemory" + std::to_string(stat.global_mem)
			+ "-clockMaxValue" + std::to_string(CLOCK_MAX_VALUE)};
	std::ofstream out{filename};
	out << "global_mem " << stat.global_mem  << std::endl;
	out << "clock max value " << CLOCK_MAX_VALUE << std::endl;
	out << "#accesses "  << stat.accesses << std::endl;
	out << "#global hits " << stat.hits << std::endl;
	out << "hit rate " << double(stat.hits) / stat.accesses  << std::endl;
	out << "noZeros " << noZeros << std::endl;
}
