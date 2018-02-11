#ifndef SEGMENT_UTIL_H
#define SEGMENT_UTIL_H

#include <vector>
#include <unordered_map>
#include <cassert>
#include <iostream>
#include <math.h>
#include "policy.h"

static const size_t top_data_bound = 1024u * 1024u * 1024u;
static const size_t segment_util = 512 * 1024u * 1024u;
static const size_t page_size = 1024; 
static const size_t number_of_pages = segment_util / page_size;
static const size_t num_hash_functions = 8;
static const size_t bits_for_page = log2(number_of_pages);

typedef __uint128_t uint128_t;

class SegmentUtil  : public Policy {
private:

	struct SUItem {
		uint32_t kId;
		size_t size;
		double time;
		bool inserted;
		int32_t keySize;
		int32_t valSize;

		SUItem() : kId(0), size(0), time(0), inserted(false),
			keySize(0), valSize(0) {}
		SUItem(	const uint32_t& rkId, 
			const size_t& rsize, 
			const double& rtime,
			const int32_t& rkeySize,
			const int32_t& rvalSize) :
			kId(rkId), size(rsize), time(rtime), inserted(false),
			keySize(rkeySize), valSize(rvalSize) {} 
	};	

	std::vector<SUItem> objects;
	std::unordered_map<uint32_t, bool> allObjects;

	size_t dataSize;
	size_t bytesCached;
	size_t numHash;
	size_t numInserted;

	static bool compareSizes(const SegmentUtil::SUItem& item1, 
			const SegmentUtil::SUItem& item2);

	inline static uint128_t MurmurHash3_x64_128(
				const void * key, 
				const int len,
				const uint32_t seed);

public:
	SegmentUtil(stats stat);
	~SegmentUtil();
	size_t process_request(const Request *r, bool warmup);
	size_t get_bytes_cached() const;
	void dump_stats(void);
};

#endif
