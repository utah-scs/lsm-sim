#include "victim_cache.h"

VictimCache::VictimCache(stats stat) :
	policy(stat),
        dram(),
        flash(),
	allObjects(),
	dramSize(0),
	flashSize(0)
{
}

VictimCache::~VictimCache() {}

size_t VictimCache::get_bytes_cached() const {return dramSize + flashSize;}

size_t VictimCache::proc(const request* r, bool warmup) {
	if (!warmup) {stat.accesses++;}	

	auto searchRKId = allObjects.find(r->kid);
	bool hitFlash = false;
	if (searchRKId != allObjects.end()) {
		FlashCache::Item& item = searchRKId->second;
		if (item.isInDram) {
			dram.erase(item.dramLruIt);
			dramSize -= item.size;
		} else {
			flash.erase(item.flashIt);
			flashSize -= item.size;
			hitFlash = true;
		}		
		if ( r->size() == item.size) {
			if (!warmup) {
				stat.hits++;
				if (hitFlash) {
					stat.hits_flash++;
				} else {
					stat.hits_dram++;
				}
			}
			insertToDram(item, warmup);
			return 1;
		} else {
			allObjects.erase(item.kId);	
		}

	}

	FlashCache::Item newItem;
	newItem.kId = r->kid;
	newItem.size = r->size();
	newItem.isInDram = true;
	assert(((unsigned int) newItem.size) <= DRAM_SIZE);
	insertToDram(newItem, warmup);
	allObjects[newItem.kId] = newItem; 
	return PROC_MISS;
}

void VictimCache::insertToDram(FlashCache::Item& item, bool warmup) {
	while (item.size + dramSize > DRAM_SIZE) {
		uint32_t lruDramItemKey = dram.back();
		FlashCache::Item& lruDramItem = allObjects[lruDramItemKey];
		dram.erase(lruDramItem.dramLruIt);
		dramSize -= lruDramItem.size;
		while (lruDramItem.size + flashSize > FLASH_SIZE) {
			uint32_t flashDramItemKey = flash.back();
			FlashCache::Item& lruFlashItem = allObjects[flashDramItemKey];
			flash.erase(lruFlashItem.flashIt);
			flashSize -= lruFlashItem.size;
			allObjects.erase(lruFlashItem.kId);
		}
		flash.emplace_front(lruDramItemKey);
		lruDramItem.flashIt = flash.begin();
		lruDramItem.isInDram = false;
		flashSize += lruDramItem.size;
		if (!warmup) {
			stat.writes_flash++;
			stat.flash_bytes_written += lruDramItem.size;
		}	
	}
	dram.emplace_front(item.kId);
	item.dramLruIt = dram.begin();
	item.isInDram = true;
	dramSize += item.size;	
}

void VictimCache::dump_stats(void) {
	assert(stat.apps->size() == 1);
	uint32_t appId = 0;
	for(const auto& app : *stat.apps) {appId = app;}
	std::string filename{stat.policy
			+ "-app" + std::to_string(appId)
			+ "-flash_mem" + std::to_string(FLASH_SIZE)
			+ "-dram_mem" + std::to_string(DRAM_SIZE)};
	std::ofstream out{filename};
	out << "dram size " << DRAM_SIZE << std::endl;
	out << "flash size " << FLASH_SIZE << std::endl;
	out << "#accesses "  << stat.accesses << std::endl;
	out << "#global hits " << stat.hits << std::endl;
	out << "#dram hits " << stat.hits_dram << std::endl;
	out << "#flash hits " << stat.hits_flash << std::endl;
	out << "hit rate " << double(stat.hits) / stat.accesses << std::endl;
	out << "#writes to flash " << stat.writes_flash << std::endl;
	out << "#bytes written to flash " << stat.flash_bytes_written << std::endl;
}
