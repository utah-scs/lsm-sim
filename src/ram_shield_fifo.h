#ifndef RAM_SHIELD_FIFO_H
#define RAM_SHIELD_FIFO_H

#include <vector>
#include <iostream>
#include <unordered_map>
#include <list>
#include <cassert>
#include "policy.h"
#include "ram_shield.h"

class RamShield_fifo : public RamShield {
	virtual void evict_item(RamShield::RItem& victimItem, bool warmup);

public:
	RamShield_fifo(stats stat, size_t block_size);
	~RamShield_fifo();
	size_t proc(const Request *r, bool warmup);
};

#endif
