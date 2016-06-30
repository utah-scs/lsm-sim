#ifndef FLASH_CACHE_H
#define FLASH_CACHE_H

#include <vector>
#include <iostream>
#include <ctime>
#include <unordered_map>
#include <list>
#include <cassert>
#include "policy.h"

/*
* These parameters define how many bytes the DRAM
* and flash can hold. These parametrs can be changed
*/
const unsigned long long DRAM_SIZE = 1024 * 1024 * 1024;
const unsigned long long FLASH_SIZE = 500 * DRAM_SIZE;
const unsigned long long FLASH_RATE = 1024 * 1024;
const unsigned int INITIAL_CREDIT = 10;
const unsigned int K = 1;

class FlashCache : public policy {
private:
	typedef std::list<std::pair<uint32_t, double> >::iterator dramIt;
	typedef std::list<uint32_t>::iterator keyIt;

	struct Item {
		uint32_t kId;
		int32_t size;
		bool isInDram;
		dramIt dramLocation;
		keyIt dramLruIt;
		keyIt flashIt;
		keyIt globalLruIt;
	
		Item() : kId(0), size(0), isInDram(true), dramLocation(),
			dramLruIt(), flashIt(), globalLruIt(){}
	};

	std::list< std::pair<uint32_t, double> > dram;
	std::list<uint32_t> dramLru;
	std::list<uint32_t> flash;
	std::list<uint32_t> globalLru;

	std::unordered_map<uint32_t, Item> allObjects;	
	/* 
	* One can move objects from the DRAM to the flash only if he has enough
	* credits. Number of current credits should be higher then the object 
	* size. Each second (FLASH_RATE * sec) are added. 
	*/
	unsigned long long credits;
	
	/*
	* The last time the credits where updates
	*/
	clock_t lastCreditUpdate;
	
	unsigned long long dramSize;
	unsigned long long flashSize;

	void updateCredits(const clock_t& currTime);
	void updateDramFlashiness(const clock_t& currTime);
	double hitCredit(const clock_t& currTime) const;
	void dramAdd(const std::pair<uint32_t, double>& p, 
			dramIt beginPlace,
			Item& item);
public:
	FlashCache(stats stat);
	~FlashCache();
	size_t proc(const request *r, bool warmup);
	size_t get_bytes_cached() const;
};

#endif
