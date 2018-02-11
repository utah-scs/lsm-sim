#include <iostream>
#include <math.h>
#include <fstream>
#include "ram_shield.h"

size_t blockSize = 1048576;
double allocation_threshold = 1;

RamShield::RamShield(stats stat, size_t block_size): 
	FlashCache(stat),
	flash{},
	allObjects{},
	maxBlocks{},
	numBlocks{}
{
	maxBlocks = FLASH_SIZE/block_size;
}

RamShield::~RamShield() {}


size_t RamShield::proc(const Request* r, bool warmup) {
	if (!warmup) {stat.accesses++;}
	counter++;

	double currTime = r->time;

	assert(dramSize + flashSize <= DRAM_SIZE + FLASH_SIZE*stat.threshold);
	auto searchRKId = allObjects.find(r->kid);
	if (searchRKId != allObjects.end()) {
		/*
		* The object exists in system. If the sizes of the 
		* current Request and previous Request differ then the previous
		* Request is removed. Otherwise, one 
		* needs to update the hitrate and its place in the globalLru.
		* If it is in the cache, one needs also to update the 
		* 'flashiness' value and its place in the dram MFU and dram LRU 
		*/
		RamShield::RItem& item = searchRKId->second;
		if (r->size() == item.size) {
	                //HIT
			if (!warmup) {stat.hits++;}
			if (!item.isGhost) {
				assert(item.globalLruIt != globalLru.end());
				globalLru.erase(item.globalLruIt);
			}
			globalLru.emplace_front(item.kId);
			item.globalLruIt = globalLru.begin();

			if (item.isInDram) {
				if (!warmup) {stat.hits_dram++;}

				//Update Flashiness
				std::pair<uint32_t, double> p = *(item.dramLocation);
				p.second += 1;
				dramIt tmp = item.dramLocation;
				dramAdd(p, tmp, item);
				dram.erase(tmp);
			} else {
				if (!warmup) {stat.hits_flash++;}
				if (item.isGhost) {
					item.isGhost = false;
					item.flashIt->size += item.size;
					flashSize += item.size;
					while (dramSize + flashSize > DRAM_SIZE + FLASH_SIZE*stat.threshold) {
			                        uint32_t globalLruKid = globalLru.back();
			                        RamShield::RItem& victimItem = allObjects[globalLruKid];
			                        assert(victimItem.size > 0);
			                        evict_item(victimItem, warmup);
					}
					assert(dramSize + flashSize <= DRAM_SIZE + FLASH_SIZE*stat.threshold);
				}
			}
			item.lastAccessInTrace = counter;
			item.last_accessed = currTime;

			return 1;
		} else {
			//UPDATE
			if (!item.isInDram)
				item.flashIt->items.remove(item.kId);
		
			if (!item.isGhost)
				evict_item(item, warmup);

 			if (!item.isInDram) {
				 assert(allObjects.find(item.kId) != allObjects.end());
				 allObjects.erase(item.kId);
			}
		}
	}
	//MISS

	/*
	* The Request doesn't exist in the system. We always insert new Requests
	* to the DRAM. 
	*/
	RamShield::RItem newItem(r, counter);
	assert(((unsigned int) newItem.size) <= DRAM_SIZE);
	assert(dramSize + flashSize <= DRAM_SIZE + FLASH_SIZE*stat.threshold);
	while (true) {
		if ((newItem.size + dramSize <= DRAM_SIZE) && (dramSize + flashSize + newItem.size <= DRAM_SIZE + FLASH_SIZE*stat.threshold)) {
			add_item(newItem);
			return PROC_MISS;
		}

		//Not enough space in DRAM, check flash
		assert(numBlocks <= maxBlocks);
		if ((dramSize + flashSize + newItem.size) > DRAM_SIZE + FLASH_SIZE*stat.threshold) {
			uint32_t globalLruKid = globalLru.back();
			RamShield::RItem& victimItem = allObjects[globalLruKid];
			assert(victimItem.size > 0);
			evict_item(victimItem, warmup);
			assert(dramSize + flashSize <= DRAM_SIZE + FLASH_SIZE*stat.threshold);
		} else if (numBlocks < maxBlocks) {
			allocate_flash_block(warmup);
		assert(dramSize + flashSize <= DRAM_SIZE + FLASH_SIZE*stat.threshold);
		} else {
			assert(0);
		}
		assert(numBlocks <= maxBlocks);
	}
	assert(0);
	return PROC_MISS;
}

void RamShield::add_item(RItem &newItem) {
	dramAddFirst(newItem);
	globalLru.emplace_front(newItem.kId);
	newItem.globalLruIt = globalLru.begin();
	allObjects[newItem.kId] = newItem;
	dramSize += newItem.size;
}


void RamShield::evict_item(RamShield::RItem& victimItem, bool warmup /*uint32_t &victimKid*/) {
	assert(!victimItem.isGhost);
	globalLru.erase(victimItem.globalLruIt);
	victimItem.globalLruIt = globalLru.end();
	if (victimItem.isInDram) {
		dram.erase(victimItem.dramLocation);
		dramSize -= victimItem.size;
		assert(allObjects.find(victimItem.kId) != allObjects.end());
		allObjects.erase(victimItem.kId);
	} else {
		victimItem.isGhost = true;
		blockIt curr_block = victimItem.flashIt;
		curr_block->size -= victimItem.size;
		flashSize -= victimItem.size;
		if ((curr_block->size/(double)stat.block_size) < stat.threshold) {
			assert(dramSize <= DRAM_SIZE);
			evict_block(curr_block);
			allocate_flash_block(warmup);
		}
	}
}

void RamShield::evict_block(blockIt victim_block) {
	for (keyIt it = victim_block->items.begin(); it != victim_block->items.end(); it++) {
		assert(allObjects.find(*it) != allObjects.end());
		RamShield::RItem& victim_item = allObjects[*it];
		if (victim_item.isGhost) {
			allObjects.erase(*it);
		} else {
			victim_item.isInDram = true;
			victim_item.flashIt = flash.end();
                        std::pair<uint32_t, double> p(victim_item.kId, INITIAL_CREDIT);
                        dramAddFirst(victim_item); 
                        dramSize += victim_item.size;
		}
	}
	flash.erase(victim_block);
	flashSize -= victim_block->size;
	numBlocks--;
	assert(numBlocks == flash.size());
}


void RamShield::allocate_flash_block(bool warmup) {
        assert(flashSize <= FLASH_SIZE);

        flash.emplace_front();
        RamShield::Block &curr_block = flash.front(); 

	auto mfu_it = --dram.end();
	while (mfu_it != dram.begin()) {
	        assert(!dram.empty());
                uint32_t mfuKid = mfu_it->first; 
		mfu_it--;
		RamShield::RItem &mfuItem = allObjects[mfuKid];
		assert(mfuItem.size > 0);

		if (curr_block.size + mfuItem.size > stat.block_size) {
			if (curr_block.size / (double)stat.block_size > allocation_threshold)
				break;
			else 
				continue;
		}

		dram.erase(mfuItem.dramLocation);
                mfuItem.isInDram = false;
		mfuItem.dramLocation = dram.end();
		mfuItem.flashIt = flash.begin();
		dramSize -= mfuItem.size;

	        curr_block.items.emplace_front(mfuKid);

		curr_block.size += mfuItem.size;

		assert(mfuItem.size > 0);
                assert(numBlocks <= maxBlocks);
 	};

	assert(curr_block.size <= stat.block_size);
	numBlocks++;
	flashSize += curr_block.size;
	assert(numBlocks <= maxBlocks);
	assert(flashSize <= FLASH_SIZE);

	if (!warmup) {
		stat.writes_flash++;
		stat.flash_bytes_written += stat.block_size;
	}
}

void RamShield::dump_stats(void) {
	Policy::dump_stats();
}
