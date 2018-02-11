#ifndef CLOCK_H
#define CLOCK_H

#include <list>
#include <unordered_map>
#include <cassert>
#include "policy.h"

extern size_t CLOCK_MAX_VALUE;

class Clock : public Policy {

private:
	typedef  std::list< std::pair<uint32_t, size_t> > ClockLru;
	
	struct ClockItem {
		uint32_t kId;
		int32_t size;
		ClockLru::iterator clockLruIt;

		ClockItem() : kId(0), size(0), clockLruIt() {}
	};

	ClockLru clockLru;
	std::unordered_map<uint32_t, ClockItem > allObjects;

	size_t lruSize;
	size_t noZeros;

	bool firstEviction;

	ClockLru::iterator clockIt;
	void deleteItem(uint32_t keyId);
public:
	Clock(stats stat);
	~Clock();
	size_t process_request(const Request* r, bool warmup);
	size_t get_bytes_cached() const;
	void dump_stats(void);
};
#endif
