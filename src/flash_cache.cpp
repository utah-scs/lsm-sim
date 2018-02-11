#include <iostream>
#include <math.h>
#include <fstream>
#include "flash_cache.h"

size_t DRAM_SIZE = 51209600;
size_t FLASH_SIZE = 51209600;
double K = 1;
size_t L_FC = 1;
double P_FC = 0.3;

// #define COMPARE_TIME
// #define RELATIVE

FlashCache::FlashCache(stats stat) : 
	Policy(stat),
	dram(),
	dramLru(),
	flash(),
	globalLru(),
	allObjects(),
	credits(0),
	lastCreditUpdate(0),
	dramSize(0),
	flashSize(0),
	counter(0)
{
}

FlashCache::~FlashCache() {}

size_t FlashCache::get_bytes_cached() const {
	return dramSize + flashSize;
}

size_t FlashCache::process_request(const Request* r, bool warmup) {
	if (!warmup) {stat.accesses++;}
	counter++;

	double currTime = r->time;
	updateCredits(currTime);

#ifdef COMPARE_TIME
	updateDramFlashiness(currTime);
#else
	updateDramFlashiness();
#endif	
	
	auto searchRKId = allObjects.find(r->kid);
	if (searchRKId != allObjects.end()) {
		/*
		* The object exists in flashCache system. If the sizes of the 
		* current Request and previous Request differ then the previous
		* Request is removed from the flashcache system. Otherwise, one 
		* needs to update the hitrate and its place in the globalLru.
		* If it is in the cache, one needs as well to update the 
		* 'flashiness' value and its place in the dram MFU and dram LRU 
		*/
		FlashCache::Item& item = searchRKId->second;
		if (r->size() == item.size) {
			if (!warmup) {stat.hits++;}
			globalLru.erase(item.globalLruIt);
			globalLru.emplace_front(item.kId);
			item.globalLruIt = globalLru.begin();
			if (item.isInDram) {
				if (!warmup) {stat.hits_dram++;}
				dramLru.erase(item.dramLruIt);
				dramLru.emplace_front(item.kId);
				item.dramLruIt = dramLru.begin(); 
				std::pair<uint32_t, double> p = *(item.dramLocation);
#ifdef COMPARE_TIME
				p.second += hitCredit(item, currTime);
#else
				p.second += hitCredit(item);
#endif
				dramIt tmp = item.dramLocation;
				dramAdd(p, tmp, item);
				dram.erase(tmp);		
			} else {
				if (!warmup) {stat.hits_flash++;}
			}
			item.lastAccessInTrace = counter;
			item.last_accessed = currTime;
			lastCreditUpdate = r->time;
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
	* The Request doesn't exist in the system. We always insert new Requests
	* to the DRAM. 
	* 2. While (object not inserted to the DRAM)
	*	2.1  if (item.size() + dramSize <= DRAM_SIZE) -
	*		insert item to the dram and return	
	*	2.2 if (not enough credits) - remove the least recently used item
	*		in the dram until there is a place. return to 2
	* 	2.3 if (possible to move from DRAM to flash) - 
	*		move items from DRAM to flash. back to 2.
	*	2.4 remove from global lru. back to 2
	*/
	FlashCache::Item newItem;
	newItem.kId = r->kid;
	newItem.size = r->size();
	newItem.isInDram = true;
	newItem.last_accessed = r->time;
	newItem.lastAccessInTrace = counter;
	assert(((unsigned int) newItem.size) <= DRAM_SIZE);
	while (true) {
		if (newItem.size + dramSize <= DRAM_SIZE) {
#ifdef RELATIVE
			dramAddFirst(newItem);
#else
			std::pair<uint32_t, double> p(newItem.kId, INITIAL_CREDIT);
			dramAdd(p, dram.begin(), newItem);
#endif
			dramLru.emplace_front(newItem.kId);
			newItem.dramLruIt = dramLru.begin();
			globalLru.emplace_front(newItem.kId);
			newItem.globalLruIt = globalLru.begin();
			allObjects[newItem.kId] = newItem;
			lastCreditUpdate = r->time;
			dramSize += newItem.size;
			return PROC_MISS;
		}
		uint32_t mfuKid = dram.back().first;
		FlashCache::Item& mfuItem = allObjects[mfuKid];	
		assert(mfuItem.size > 0);	
		if (credits < (double) mfuItem.size) {
			if (!warmup) {stat.credit_limit++;}
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
				if (!warmup) {
					stat.writes_flash++;
					stat.flash_bytes_written += mfuItem.size;
				}
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
	assert(false);	
	return PROC_MISS;
}

void FlashCache::updateCredits(const double& currTime) {
	double elapsed_secs = currTime - lastCreditUpdate;
	credits += elapsed_secs * FLASH_RATE;
}

void FlashCache::updateDramFlashiness(const double& currTime) {
	double mul;
#ifdef COMPARE_TIME
	assert(currTime >= 0);
	double elapsed_secs = currTime - lastCreditUpdate;
        mul = exp(-elapsed_secs / K);
#else
	assert(currTime == -1);
	mul = exp(-1 / K);
#endif
	for(dramIt it = dram.begin(); it != dram.end(); it++) {
                it->second = it->second * mul;
	}
}


double FlashCache::hitCredit(const Item& item, const double& currTime) const{
	double diff;
#ifdef COMPARE_TIME
	diff = currTime - item.last_accessed;
#else
	assert(currTime == -1);
	assert(item.lastAccessInTrace < counter);
	diff = counter - item.lastAccessInTrace;
#endif
	assert(diff != 0);
	double mul = exp(-diff / K);
	return ((1 - mul) * (L_FC / diff));
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

void FlashCache::dramAddFirst(Item& item) {
	if (dram.size() == 0) {
		std::pair<uint32_t, double> p(item.kId, INITIAL_CREDIT);
		dram.emplace_front(p);
		item.dramLocation = dram.begin();
		return;
	}
	dramIt it = dram.begin();
	std::advance(it, ceil(std::distance(dram.begin(),dram.end()) * P_FC ));
	std::pair<uint32_t, double> p;
	if (it == dram.end())  {
		assert(dram.size() > 0);
		it--;
		p = std::make_pair(item.kId, it->second);
		dram.emplace_back(p);
		it++;
		assert(it != dram.end());
		item.dramLocation = it;
		return;
	}
	p = std::make_pair(item.kId, it->second);
	dram.insert(it, p);
	it--;
	item.dramLocation = it;
}

void FlashCache::dump_stats(void) {
	assert(stat.apps->size() == 1);
	uint32_t appId = 0;
	for(const auto& app : *stat.apps) {appId = app;}
	std::string filename{stat.policy
#ifdef RELATIVE
			+ "-relative" + std::to_string(P_FC)
#endif
#ifdef COMPARE_TIME
			+ "-time"
#else
			+ "-place"
#endif
			+ "-app" + std::to_string(appId)
			+ "-flash_mem" + std::to_string(FLASH_SIZE)
			+ "-dram_mem" + std::to_string(DRAM_SIZE)
			+ "-K" + std::to_string(K)};
	std::ofstream out{filename};
	out << "dram size " << DRAM_SIZE << std::endl;
	out << "flash size " << FLASH_SIZE << std::endl;
	out << "initial credit " << INITIAL_CREDIT << std::endl;
	out << "#credits per sec " << FLASH_RATE << std::endl;
#ifdef COMPARE_TIME
	out << "Time" << std::endl;
#else
	out << "Place" << std::endl;
#endif

#ifdef RELATIVE
	out << "P_FC " << P_FC << std::endl;
#endif
	out << "K " << K << std::endl;
	out << "#accesses "  << stat.accesses << std::endl;
	out << "#global hits " << stat.hits << std::endl;
	out << "#dram hits " << stat.hits_dram << std::endl;
	out << "#flash hits " << stat.hits_flash << std::endl;
	out << "hit rate " << double(stat.hits) / stat.accesses << std::endl;
	out << "#writes to flash " << stat.writes_flash << std::endl;
	out << "credit limit " << stat.credit_limit << std::endl; 
	out << "#bytes written to flash " << stat.flash_bytes_written << std::endl;
	out << std::endl << std::endl;
	out << "key,rate" << std::endl;
	for (dramIt it = dram.begin(); it != dram.end(); it++) {
		out << it->first << "," << it->second << std::endl;
	}
}

