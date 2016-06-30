#include <iostream>
#include <math.h>
#include "flash_cache.h"

FlashCache::FlashCache(stats stat) : 
	policy(stat),
	dram(),
	dramLru(),
	flash(),
	globalLru(),
	allObjects(),
	credits(0),
	lastCreditUpdate(clock()),
	dramSize(0),
	flashSize(0)
{
}

FlashCache::~FlashCache() {}

size_t FlashCache::get_bytes_cached() const {
	return dramSize + flashSize;
}

size_t FlashCache::proc(const request* r, bool warmup) {
	if (!warmup) {stat.accesses++;}
	
	clock_t currTime = clock();
	updateCredits(currTime);
	updateDramFlashiness(currTime);
	
	auto searchRKId = allObjects.find(r->kid);
	if (searchRKId != allObjects.end()) {
		/*
		* The object exists in flashCache system. One needs to update the
		* hitrate and its place in the globalLru.
		* If it is in the cache, one needs as well to update the 
		* 'flashiness' value and its place in the dram MFU and dram LRU 
		*/
		FlashCache::Item& item = searchRKId->second;
		if (r->size() == item.size) {
			stat.hits++;
			globalLru.erase(item.globalLruIt);
			globalLru.emplace_front(item.kId);
			item.globalLruIt = globalLru.begin();
			if (item.isInDram) {
				dramLru.erase(item.dramLruIt);
				dramLru.emplace_front(item.kId);
				item.dramLruIt = dramLru.begin(); 
				std::pair<uint32_t, double> p = *(item.dramLocation);
				p.second += hitCredit(currTime);
				dramIt tmp = item.dramLocation;
				dramAdd(p, tmp, item);
				dram.erase(tmp);		
			}
			return 1;
		} else {
			globalLru.erase(item.globalLruIt);
			if(item.isInDram) {
				dram.erase(item.dramLocation);
				dramLru.erase(item.dramLruIt);
				dramSize -= item.size;
			} else {
				flash.erase(item.flashIt);
				flashSize -= item.size;
			}	
			allObjects.erase(item.kId);
		}
	}
	/*
	* The request doesn't exist in the system. We always insert new requests
	* in the DRAM. 
	* 2. While (object not inserted to the DRAM)
	*	2.1  if (item.size() + dramSize <= DRAM_SIZE) -
	*		insert item to the dram and return	
	*	2.2 if (not enough credits) - remove the least recently used items
	*		until there is a place in the dram. back to 2
	* 	2.3 if (possible to move from DRAM to flash) - 
	*		move items from DRAM to flash. back to 2.
	* 	2.2. if (item.size() + dramSize <= DRAM_SIZE) - insert element
	*	2.4 remove from global lru
	*/
	FlashCache::Item newItem;
	newItem.kId = r->kid;
	newItem.size = r->size();
	newItem.isInDram = true;
	assert(((unsigned int) newItem.size) < DRAM_SIZE);
	while (true) {
		if (newItem.size + dramSize <= DRAM_SIZE) {
			std::pair<uint32_t, double> p(newItem.kId, INITIAL_CREDIT);
			dramAdd(p, dram.begin(), newItem);		
			globalLru.emplace_front(newItem.kId);
			newItem.dramLruIt = globalLru.begin();
			allObjects[newItem.kId] = newItem;
			return PROC_MISS;
		}
		uint32_t mfuKid = dram.back().first;
		FlashCache::Item& mfuItem = allObjects[mfuKid];	
		assert(mfuItem.size > 0);	
		if (credits < (unsigned long long) mfuItem.size) {
			while (newItem.size + dramSize > DRAM_SIZE) {
				uint32_t lruKid = dramLru.back();
				FlashCache::Item& lruItem = allObjects[lruKid];
				assert(lruItem.size > 0);
				dram.erase(lruItem.dramLocation);
				dramLru.erase(lruItem.dramLruIt);
				globalLru.erase(lruItem.globalLruIt);
				dramSize -= lruItem.size;
				allObjects.erase(lruKid);
			}
			continue;
		} else {
			if (flashSize + mfuItem.size <= FLASH_SIZE) {
				mfuItem.isInDram = false;
				dram.erase(mfuItem.dramLocation);
				dramLru.erase(mfuItem.dramLruIt);
				flash.emplace_front(mfuKid);
				mfuItem.dramLocation = dram.end();
				mfuItem.dramLruIt = dramLru.end();
				mfuItem.flashIt = flash.begin(); 
				credits -= mfuItem.size;
				dramSize -= mfuItem.size;
				flashSize += mfuItem.size;
			}
			else {
				uint32_t globalLruKid = globalLru.back();
				FlashCache::Item& globalLruItem = allObjects[globalLruKid];
				globalLru.erase(globalLruItem.globalLruIt);
				if (globalLruItem.isInDram) {
					dram.erase(globalLruItem.dramLocation);
					dramLru.erase(globalLruItem.dramLruIt);
					dramSize -= globalLruItem.size;
				} else {
					flash.erase(globalLruItem.flashIt);
					flashSize -= globalLruItem.size;
				}
				allObjects.erase(globalLruKid);
			}	
		}
	}	
	return PROC_MISS;
}

void FlashCache::updateCredits(const clock_t& currTime) {
	double elapsed_secs = 
		double(currTime - lastCreditUpdate) / CLOCKS_PER_SEC;
	credits += ((int)floor(elapsed_secs)) * FLASH_RATE;
}

void FlashCache::updateDramFlashiness(const clock_t& currTime) {
	double elapsed_secs =
                double(currTime - lastCreditUpdate) / CLOCKS_PER_SEC;
        double mul = exp(-elapsed_secs);

        for(dramIt it = dram.begin(); it != dram.end(); it++) {
                it->second = it->second * mul;
	}
}

double FlashCache::hitCredit(const clock_t& currTime) const{
	double elapsed_secs = 
		double(currTime - lastCreditUpdate) / CLOCKS_PER_SEC;
	double mul = exp(-elapsed_secs);
	assert(elapsed_secs != 0);
	return ((1 - mul) * (1/elapsed_secs));
}

void FlashCache::dramAdd(const std::pair<uint32_t, double>& p, 
			dramIt beginPlace,
			Item& item){

	for(dramIt it = beginPlace; it != dram.end(); it++) {
		if (p.second < it->second) {
			dram.insert(it, p);
			item.dramLocation = --it;
			return;			
		}	
	}
	dram.emplace_back(p);
	dramIt tmp = dram.end();
	assert(dram.size() > 0);
	tmp--;
	item.dramLocation = tmp;
}

