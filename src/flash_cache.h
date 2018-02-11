#ifndef FLASH_CACHE_H
#define FLASH_CACHE_H

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
extern size_t DRAM_SIZE;
extern size_t FLASH_SIZE;
const size_t FLASH_RATE = 1024 * 1024;
const size_t INITIAL_CREDIT = 1;
extern double K;
extern size_t L_FC;
extern double P_FC;

class FlashCache : public Policy {
protected:
	typedef std::list<std::pair<uint32_t, double> >::iterator dramIt;
	typedef std::list<uint32_t>::iterator keyIt;

	struct Item {
		uint32_t kId;
		int32_t size;
		double last_accessed;
		size_t lastAccessInTrace;
		bool isInDram;
		dramIt dramLocation;
		keyIt dramLruIt;
		keyIt flashIt;
		keyIt globalLruIt;
	
		Item() : kId(0), size(0), last_accessed(0), lastAccessInTrace(0),
			isInDram(true), dramLocation(),dramLruIt(), flashIt(), globalLruIt(){}
	};

	std::list< std::pair<uint32_t, double> > dram;
	std::list<uint32_t> dramLru;
	std::list<uint32_t> flash;
	std::list<uint32_t> globalLru;

	std::unordered_map<uint32_t, Item> allObjects;	
	/* 
	* One can move objects from the DRAM to the flash only if he has enough
	* credits. Number of current credits should be higher then the object 
	* size. Each delta T (FLASH_RATE * delta T) are added. 
	*/
	double credits;
	
	/*
	* The last time the credits where updates
	*/
	double lastCreditUpdate;
	
	size_t dramSize;
	size_t flashSize;

	size_t counter;

	void updateCredits(const double& currTime);
	void updateDramFlashiness(const double& currTime = -1);
	double hitCredit(const Item& item, const double& currTime = -1) const;
	void dramAdd(const std::pair<uint32_t, double>& p, 
			dramIt beginPlace,
			Item& item);
	void dramAddFirst(Item& item);
	friend class VictimCache;
public:
	FlashCache(stats stat);
	~FlashCache();
	size_t process_request(const Request *r, bool warmup);
	size_t get_bytes_cached() const;
	void dump_stats(void);
};

#endif
