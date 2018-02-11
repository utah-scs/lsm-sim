#ifndef RAM_SHIELD_H
#define RAM_SHIELD_H

#include <vector>
#include <iostream>
#include <unordered_map>
#include <list>
#include <cassert>
#include "policy.h"
#include "flash_cache.h"

class RamShield : public FlashCache {
protected:
	struct Block;
	typedef std::list<Block>::iterator blockIt;
	
	struct RItem : public FlashCache::Item{
		blockIt flashIt;
                bool isGhost;

		RItem() : FlashCache::Item(), flashIt{}, isGhost(false) {}
		RItem(const Request *r, size_t counter) : FlashCache::Item(), flashIt{}, isGhost(false) {
		        kId = r->kid;
		        size = r->size();
		        isInDram = true;
		        last_accessed = r->time;
		        lastAccessInTrace = counter;
		}
	};

	struct Block {
		size_t size;
		std::list<uint32_t> items;
		Block() : size{}, items{} {}
	};

	std::list<Block> flash;
	std::unordered_map<uint32_t, RItem> allObjects;
	size_t maxBlocks;
	size_t numBlocks;
	void add_item(RItem &newItem);
	virtual void evict_item(RItem& victimItem, bool warmup);
	void evict_block(blockIt victim_block);
	void allocate_flash_block(bool warmup);

public:
	RamShield(stats stat, size_t block_size);
	~RamShield();
	size_t proc(const Request *r, bool warmup);
	void dump_stats(void);

};

#endif
