#include <iostream>
#include <math.h>
#include <fstream>
#include "ram_shield_sel.h"

RamShield_sel::RamShield_sel(stats stat, size_t block_size):
	RamShield(stat, block_size)
{
}

RamShield_sel::~RamShield_sel() {}


size_t RamShield_sel::proc(const Request* r, bool warmup) {
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
		if (newItem.size + dramSize <= DRAM_SIZE && (dramSize + flashSize + newItem.size <= DRAM_SIZE + FLASH_SIZE*stat.threshold)) {
			add_item(newItem);
			assert(dramSize + flashSize <= DRAM_SIZE + FLASH_SIZE*stat.threshold);
			return PROC_MISS;
		}

		//Not enough space in DRAM
		assert(numBlocks <= maxBlocks);
		if ((dramSize + flashSize + newItem.size) > DRAM_SIZE + FLASH_SIZE*stat.threshold) {
			uint32_t globalLruKid = globalLru.back();
			RamShield::RItem& victimItem = allObjects[globalLruKid];
			assert(victimItem.size > 0);
			evict_item(victimItem, warmup);
			assert(dramSize + flashSize <= DRAM_SIZE + FLASH_SIZE*stat.threshold);
		} else if (numBlocks == maxBlocks) {
			blockIt victim_block = flash.begin();
			for (blockIt curr_block = flash.begin(); curr_block != flash.end(); curr_block++) {
				if (curr_block->size < victim_block->size)
					victim_block = curr_block;
			}
			assert(victim_block != flash.end());
			evict_block(victim_block);
			assert(dramSize + flashSize <= DRAM_SIZE + FLASH_SIZE*stat.threshold);
		} else if (numBlocks < maxBlocks) {
			allocate_flash_block(warmup);
			assert(dramSize + flashSize <= DRAM_SIZE + FLASH_SIZE*stat.threshold);
			assert(numBlocks <= maxBlocks);
			assert(dramSize <= DRAM_SIZE);
		} else {
			assert(0);
		}

		assert(numBlocks <= maxBlocks);
	}
	assert(0);
	return PROC_MISS;
}

void RamShield_sel::evict_item(RamShield::RItem& victimItem, bool warmup) {
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
	}
}
