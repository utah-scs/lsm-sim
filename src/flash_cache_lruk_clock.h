#ifndef FLASH_CACHE_LRUK_CLK_H
#define FLASH_CACHE_LRUK_CLK_H

#include <vector>
#include <iostream>
#include <unordered_map>
#include <list>
#include <cassert>
#include "policy.h"

/*
* These parameters define how many bytes the DRAM
* and flash can hold. These parametrs can be changed
*/
extern size_t FC_K_LRU_CLK;
extern size_t FC_KLRU_QUEUE_SIZE_CLK;
extern size_t DRAM_SIZE_FC_KLRU_CLK;
extern size_t FLASH_SIZE_FC_KLRU_CLK;
extern size_t CLOCK_MAX_VALUE_KLRU;
const size_t LRUK_FLASH_RATE_CLK = 1024 * 1024;
const size_t CLOCK_START_VAL = 3;

class FlashCacheLrukClk : public Policy {
private:
	typedef std::list<std::pair<uint32_t, size_t> >::iterator dramIt;
	typedef std::list<uint32_t>::iterator keyIt;

	struct Item {
		uint32_t kId;
		int32_t size;
		bool isInDram;
		keyIt dramLocation;
		size_t queueNumber;
		keyIt flashIt;
		keyIt dramLruIt;
		dramIt ClockIt;
		size_t clockJumpStatus;
	
		Item() : kId(0), size(0), isInDram(true), dramLocation(), queueNumber(0), flashIt(), dramLruIt(), ClockIt(), clockJumpStatus(CLOCK_START_VAL){}
	};

	struct RecItem {
		uint32_t kId;
		int32_t size;
		size_t queueWhenInserted;
		size_t hitTimesAfterInserted;
		size_t rewrites;
		RecItem() : kId(0), size(0), queueWhenInserted(0), hitTimesAfterInserted(0), rewrites(0){}
	};

	std::vector< std::list<uint32_t>> dram;
	std::list<uint32_t> dramLru;
	std::list<std::pair<uint32_t, size_t> > clockLru;
	std::vector<size_t> kLruSizes;
	std::list<uint32_t> flash;

	std::unordered_map<uint32_t, Item> allObjects;	
	std::unordered_map<uint32_t, RecItem> allFlashObjects;
	
	size_t dramSize;
	size_t flashSize;
	dramIt clockIt;
	bool firstEviction;

	void dramAdd(std::vector<uint32_t>& objects,
			size_t sum,
			size_t k,
			bool updateWrites,
			bool warmup,
			bool NewItem);
	void dramAddandReorder(std::vector<uint32_t>& objects,
			size_t sum,
			size_t k,
			bool updateWrites,
			bool warmup);

	void deleteItem(uint32_t keyId);
	friend class VictimCache;

public:
	FlashCacheLrukClk(stats stat);
	~FlashCacheLrukClk();
	size_t process_request(const Request *r, bool warmup);
	size_t get_bytes_cached() const;
	void dump_stats(void);
};

#endif
