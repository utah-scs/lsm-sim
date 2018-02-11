#include <iostream>
#include <math.h>
#include <fstream>
#include "flash_cache_lruk.h"

size_t FC_K_LRU = 8;
size_t DRAM_SIZE_FC_KLRU = 51209600;
size_t FC_KLRU_QUEUE_SIZE = DRAM_SIZE_FC_KLRU / FC_K_LRU;
size_t FLASH_SIZE_FC_KLRU = 51209600;
double K_FC_KLRU = 1;
size_t L_FC_KLRU = 1;
double P_FC_KLRU = 0.3;

// #define COMPARE_TIME
// #define RELATIVE

FlashCacheLruk::FlashCacheLruk(stats stat) :
	Policy(stat),
	dram(FC_K_LRU),
	kLruSizes(FC_K_LRU, 0),
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

FlashCacheLruk::~FlashCacheLruk() {}

size_t FlashCacheLruk::get_bytes_cached() const {
	return dramSize + flashSize;
}

size_t FlashCacheLruk::process_request(const Request* r, bool warmup) {
	if (!warmup) {stat.accesses++;}
	counter++;

	double currTime = r->time;
	updateCredits(currTime);
	bool updateWrites = true;
	uint32_t mfuKid=0;

//#ifdef COMPARE_TIME
//	updateDramFlashiness(currTime);
//#else
//	updateDramFlashiness();
//#endif
	
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
		FlashCacheLruk::Item& item = searchRKId->second;
		if (r->size() == item.size) {
			if (!warmup) {stat.hits++;}

			globalLru.erase(item.globalLruIt);
			globalLru.emplace_front(item.kId);
			item.globalLruIt = globalLru.begin();

			if (item.isInDram) {

				if (!warmup) {stat.hits_dram++;}

				size_t qN = item.queueNumber;
//				std::pair<uint32_t, double> p = *(item.dramLocation);
//#ifdef COMPARE_TIME
//				p.second += hitCredit(item, currTime);
//#else
//				p.second += hitCredit(item);
//#endif

				dram[qN].erase(item.dramLocation);
				kLruSizes[qN] -= item.size;
				dramSize -= item.size;

				if ((qN + 1) != FC_K_LRU) {
						qN++;
				} else {
						updateWrites = false;
				}

				std::vector<uint32_t> objects{r->kid};
				dramAddandReorder(objects, r->size(),qN, updateWrites, warmup);

			} else {
				if (!warmup) {stat.hits_flash++;}
			}
			item.lastAccessInTrace = counter;
			item.last_accessed = currTime;
			lastCreditUpdate = r->time;
			return 1;
		} else {// Size changed

			globalLru.erase(item.globalLruIt);

			if(item.isInDram) {
				size_t qN = item.queueNumber;
				dram[qN].erase(item.dramLocation);
				kLruSizes[qN] -= item.size;
				dramSize -= item.size;

			} else {// in flash
				flash.erase(item.flashIt);
				flashSize -= item.size;
			}	

			allObjects.erase(item.kId);
		}
	}
	/*
	* The Request doesn't exist in the system. We always insert new Requests
	* to the DRAM at the beginning of the last queue.
	*
	* 2. While (object not inserted to the DRAM)
	*	2.1  if (item.size() + dramSize <= DRAM_SIZE_FC_KLRU) -
	*		insert item to the dram and return	
	*	2.2 if (not enough credits) - remove the least recently used item
	*		in the dram until there is a place. return to 2
	* 	2.3 if (possible to move from DRAM to flash) - 
	*		move items from DRAM to flash. back to 2.
	*	2.4 remove from global lru. back to 2
	*/

	FlashCacheLruk::Item newItem;
	newItem.kId = r->kid;
	newItem.size = r->size();
	newItem.isInDram = true;
	newItem.last_accessed = r->time;
	newItem.lastAccessInTrace = counter;

	assert(((unsigned int) newItem.size) <= DRAM_SIZE_FC_KLRU);


	while (true) {
		if (newItem.size + dramSize <= DRAM_SIZE_FC_KLRU) {
			// If we have place in the dram insert the new item to the beginning of the last queue.

			globalLru.emplace_front(newItem.kId);
			newItem.globalLruIt = globalLru.begin();
			allObjects[newItem.kId] = newItem;
			lastCreditUpdate = r->time;
			
                        std::vector<uint32_t> objects{r->kid};
                        dramAdd(objects, r->size(),0, true, warmup);

			return PROC_MISS;
		}
		// If we don't have enough space in the dram, we can move MRU items to the flash
		// or to delete LRU items

		for (int i=FC_K_LRU-1; i >=0 ; i--)
		{// Find the MRU item
			if (kLruSizes[i] > 0){
				mfuKid = ((dram[i]).front()).first;
				break;
			}
		}

		FlashCacheLruk::Item& mfuItem = allObjects[mfuKid];
		size_t qN = mfuItem.queueNumber;

		assert(mfuItem.size > 0);	
//		if (credits < (double) mfuItem.size) {
//			// If we can't write into the flash we need to make room in the dram
//			if (!warmup) {stat.credit_limit++;}
//
//			while (newItem.size + dramSize > DRAM_SIZE_FC_KLRU ) {
//				// -------------
//				// Need to extract the last items from the last queue until
//				// there will be enough space. Then to insert the new item at the
//				// beginning of the last queue
//				// ------------
//
//				uint32_t lruKid = ((dram[0]).back()).first;
//				FlashCacheLruk::Item& lruItem = allObjects[lruKid];
//
//				assert(lruItem.size > 0);
//				dram[0].erase(lruItem.dramLocation);
//				globalLru.erase(lruItem.globalLruIt);
//				kLruSizes[0] -= lruItem.size;
//				dramSize -= lruItem.size;
//				allObjects.erase(lruKid);
//			}
//			continue;
//		} else {
		// We can write items to the flash

			if (flashSize + mfuItem.size <= FLASH_SIZE_FC_KLRU) {
				// If we have enough space in the flash, we will insert the MRU item
				// to the flash

				mfuItem.isInDram = false;
				dram[qN].erase(mfuItem.dramLocation);
				flash.emplace_front(mfuKid);
				mfuItem.dramLocation = ((dram[0]).end());
				dramSize -= mfuItem.size;
				mfuItem.flashIt = flash.begin(); 
				credits -= mfuItem.size;
				kLruSizes[qN] -= mfuItem.size;
				flashSize += mfuItem.size;
				if (!warmup) {
					stat.writes_flash++;
					stat.flash_bytes_written += mfuItem.size;
				}
			}
			else {
				// If we don't have space in the flash, we will delete the GLRU item
				// and make room for the new item
				uint32_t globalLruKid = globalLru.back();
				FlashCacheLruk::Item& globalLruItem = allObjects[globalLruKid];
				globalLru.erase(globalLruItem.globalLruIt);
				if (globalLruItem.isInDram) {
					size_t dGqN = globalLruItem.queueNumber;
					dram[dGqN].erase(globalLruItem.dramLocation);
					kLruSizes[dGqN] -= globalLruItem.size;
					dramSize -= globalLruItem.size;
				} else {
					flash.erase(globalLruItem.flashIt);
					flashSize -= globalLruItem.size;
				}
				allObjects.erase(globalLruKid);
			}	
//		}
	}
	assert(false);	
	return PROC_MISS;
}

void FlashCacheLruk::updateCredits(const double& currTime) {
	double elapsed_secs = currTime - lastCreditUpdate;
	credits += elapsed_secs * LRUK_FLASH_RATE;
}

//void FlashCacheLruk::updateDramFlashiness(const double& currTime) {
//	double mul;
//#ifdef COMPARE_TIME
//	assert(currTime >= 0);
//	double elapsed_secs = currTime - lastCreditUpdate;
 //       mul = exp(-elapsed_secs / K_FC_KLRU);
//#else
//	assert(currTime == -1);
//	mul = exp(-1 / K_FC_KLRU);
//#endif
//	for(dramIt it = dram.begin(); it != dram.end(); it++) {
//                it->second = it->second * mul;
//	}
//}


double FlashCacheLruk::hitCredit(const Item& item, const double& currTime) const{
	double diff;
#ifdef COMPARE_TIME
	diff = currTime - item.last_accessed;
#else
	assert(currTime == -1);
	assert(item.lastAccessInTrace < counter);
	diff = counter - item.lastAccessInTrace;
#endif
	assert(diff != 0);
	double mul = exp(-diff / K_FC_KLRU);
	return ((1 - mul) * (L_FC_KLRU / diff));
}

void FlashCacheLruk::dramAdd(std::vector<uint32_t>& objects,
		size_t sum __attribute__ ((unused)),
		size_t k,
		bool updateWrites __attribute__ ((unused)),
		bool warmup __attribute__ ((unused))) {

	    for (const uint32_t& elem : objects) {
				FlashCacheLruk::Item& item = allObjects[elem];
				std::pair<uint32_t, double> it;
				it.first = elem;
				it.second=0;
				dram[k].emplace_front(it);
				item.dramLocation = dram[k].begin();
				item.queueNumber = k;
				dramSize += item.size;
				kLruSizes[k] += item.size;
				if (k != 0){
					assert(kLruSizes[k] <= FC_KLRU_QUEUE_SIZE);}	
				}
}

void FlashCacheLruk::dramAddandReorder(std::vector<uint32_t>& objects,
		size_t sum,
		size_t k,
		bool updateWrites,
		bool warmup) {

	    assert(k < FC_K_LRU);

		std::vector<uint32_t> newObjects;

		size_t newSum = 0;

		if (k != 0)
		{
			while (sum + kLruSizes[k] > FC_KLRU_QUEUE_SIZE) {
				assert(kLruSizes[k] > 0);
				assert(dram[k].size() > 0);
				uint32_t elem = (dram[k].back()).first;
				FlashCacheLruk::Item& item = allObjects[elem];
				dram[k].pop_back();
				kLruSizes[k] -= item.size;
				dramSize -= item.size;
				//saving the extracted items in order to put them in lower queue
				newSum += item.size;
				newObjects.emplace_back(elem);
			}
		} else {
			while (sum + dramSize > DRAM_SIZE_FC_KLRU) {
				//shouldnt get to here
				assert(0);
				assert(kLruSizes[k] > 0);
				assert(dram[k].size() > 0);
				uint32_t elem = (dram[k].back()).first;
				FlashCacheLruk::Item& item = allObjects[elem];
				dram[k].pop_back();
				kLruSizes[k] -= item.size;
				dramSize -= item.size;
				// in k=0 we don't need to save the extracted items since we are dumping them forever
				globalLru.erase(item.globalLruIt);
			}
		}

		if (!updateWrites) {
			assert(newObjects.size() == 0);
			assert(newSum == 0);
		}

//		for (const uint32_t& elem : objects) {
//			FlashCacheLruk::Item& item = allObjects[elem];
//			std::pair<uint32_t, double> it;
//			it.first = elem;
//			it.second=0;
//			dram[k].emplace_front(it);
//			item.dramLocation = dram[k].begin();
//			item.queueNumber = k;
//			kLruSizes[k] += item.size;
//			dramSize += item.size;
//			if (k > 0)
//				{assert(kLruSizes[k] <= FC_KLRU_QUEUE_SIZE);}
//		}
		dramAdd(objects,sum,k,updateWrites,warmup);

		if (k > 0 && newObjects.size() > 0) {
			dramAddandReorder(newObjects, newSum, k-1, true, warmup);
		}
}

void FlashCacheLruk::dump_stats(void) {
	assert(stat.apps->size() == 1);
	uint32_t appId = 0;
	for(const auto& app : *stat.apps) {appId = app;}
	std::string filename{stat.policy
#ifdef RELATIVE
			+ "-relative" + std::to_string(P_FC_KLRU)
#endif
#ifdef COMPARE_TIME
			+ "-time"
#else
			+ "-place"
#endif
			+ "-app" + std::to_string(appId)
			+ "-flash_mem" + std::to_string(FLASH_SIZE_FC_KLRU)
			+ "-dram_mem" + std::to_string(DRAM_SIZE_FC_KLRU)
			+ "-K" + std::to_string(K_FC_KLRU)};
	std::ofstream out{filename};
	out << "dram size " << DRAM_SIZE_FC_KLRU << std::endl;
	out << "flash size " << FLASH_SIZE_FC_KLRU << std::endl;
	out << "initial credit " << LRUK_INITIAL_CREDIT << std::endl;
	out << "#credits per sec " << LRUK_FLASH_RATE << std::endl;
#ifdef COMPARE_TIME
	out << "Time" << std::endl;
#else
	out << "Place" << std::endl;
#endif

#ifdef RELATIVE
	out << "P_FC " << P_FC_KLRU << std::endl;
#endif
	out << "K " << K_FC_KLRU << std::endl;
	out << "#accesses "  << stat.accesses << std::endl;
	out << "#global hits " << stat.hits << std::endl;
	out << "#dram hits " << stat.hits_dram << std::endl;
	out << "#flash hits " << stat.hits_flash << std::endl;
	out << "hit rate " << double(stat.hits) / stat.accesses << std::endl;
	out << "#writes to flash " << stat.writes_flash << std::endl;
	out << "credit limit " << stat.credit_limit << std::endl; 
	out << "#bytes written to flash " << stat.flash_bytes_written << std::endl;
	out << std::endl << std::endl;
	out << "hwadsfasfsafasfsdaffasgasfdsdafasfgsfg" << std::endl;
	out << "key,rate" << std::endl;
	out << "Amount of lru queues: " << FC_K_LRU << std::endl;
	for (size_t i =0 ; i < FC_K_LRU ; i++){
		out << "dram[" << i << "] has " << dram[i].size() << " items" << std::endl;
		out << "dram[" << i << "] size written " << kLruSizes[i] << std::endl;
	}
	out << "Total dram filled size " << dramSize << std::endl;
	//for (dramIt it = dram.begin(); it != dram.end(); it++) {
	//	out << it->first << "," << it->second << std::endl;
	//}
}

