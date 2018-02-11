#include <algorithm>
#include <math.h>
#include "segment_util.h"

SegmentUtil::SegmentUtil(stats stat) :	Policy(stat),
					objects(), 
					allObjects(),
					dataSize(0), 
					bytesCached(0),
					numHash(0),
					numInserted(0)
{
	assert(exp2(bits_for_page) == number_of_pages); 
	assert(segment_util % page_size == 0); 
}

SegmentUtil::~SegmentUtil() {}

size_t SegmentUtil::process_request(const Request *r, bool woormup __attribute__ ((unused))) {
	if (r->size() + dataSize > top_data_bound) {
		std::sort(objects.begin(),objects.end(), compareSizes);
		std::vector<size_t> pageSizes(number_of_pages, 0);
		for (size_t i = 0; i < objects.size(); i++) {
			SegmentUtil::SUItem& item = objects[i];
			uint128_t lastHash = item.kId;
			for (size_t j = 0 ; j < num_hash_functions; j++) {
				lastHash = MurmurHash3_x64_128((void*)&lastHash, sizeof(lastHash), 0);
				uint32_t pageId = lastHash & ((1lu << bits_for_page) - 1);
				assert(pageId < number_of_pages);
				// no space in the page
				if (pageSizes[pageId] == page_size) {continue;}
				assert(page_size >= pageSizes[pageId]);
				size_t head = page_size - pageSizes[pageId];
				if (item.size <= head) {
					item.inserted = true;
					pageSizes[pageId] += item.size;
					bytesCached += item.size;
					numHash += (j + 1);
					numInserted++;
					break;
				}
				assert(item.size > head);
				size_t numPages = (item.size - head) / page_size;
				assert(item.size >= (head + numPages * page_size));
				size_t tail = item.size - (head + numPages * page_size);
				// no continuous memory 
				if (pageId + numPages >= number_of_pages) { continue; }
				if (tail > 0 && (pageId + numPages + 1 >= number_of_pages)) { continue; }
				
				bool isEmpty = true;
				for (size_t i = 1; i <= numPages; i++) {
					if (pageSizes[pageId + i] != 0) {
						isEmpty = false;
					}
				}
				if (!isEmpty) { continue; }
				if (pageSizes[pageId + numPages + 1] + tail > page_size) { continue; }
				item.inserted = true;
				bytesCached += item.size;
				numHash += (j + 1);
				numInserted++;
				pageSizes[pageId] += head;
				for (size_t i = 1; i <= numPages; i++) {
					pageSizes[pageId + i] += page_size;
				}
				pageSizes[pageId + numPages + 1] += tail;
				break;
			}
		}
		dump_stats();
		exit(0);
	} else {
    		static int32_t next_dump = 10 * 1024 * 1024;
		if (allObjects.find(r->kid) == allObjects.end()) {
			SegmentUtil::SUItem item(r->kid,(size_t)r->size(),r->time,r->key_sz, r->val_sz);	
			objects.emplace_back(item);
			allObjects[r->kid] = true;
			dataSize += r->size();
      			if (next_dump < r->size()) {
        			std::cerr << "Progress: " << dataSize  / 1024 / 1024 << " MB" << std::endl;
        			next_dump += 10 * 1024 * 1024;
      			}
      			next_dump -= r->size();
		}
	}
	return 0;
}

bool SegmentUtil::compareSizes(const SegmentUtil::SUItem& item1, 
		const SegmentUtil::SUItem& item2) {
	return item1.size > item2.size;
}

size_t SegmentUtil::get_bytes_cached() const { return bytesCached;}

void SegmentUtil::dump_stats(void) {
	assert(stat.apps->size() == 1);
        uint32_t appId = 0;
        for(const auto& app : *stat.apps) {appId = app;}
	std::string filename{stat.policy
			+ "-app" + std::to_string(appId)
			+ "-segment_size" + std::to_string(segment_util)
			+ "-hash" + std::to_string(num_hash_functions)
			+ "-page" + std::to_string(page_size)};
	std::ofstream out{filename};
	out << "segment size: " << segment_util << std::endl;
	out << "page size: " << page_size << std::endl;
	out << "#hash functions: " << num_hash_functions << std::endl;
	out << "avg hash function used: " << (double) numHash/ ((double) numInserted) << std::endl;
	out << "bits per page: " << bits_for_page << std::endl;
	out << "total bytes cached: " << bytesCached << std::endl;
	out << "util: " << ((double) bytesCached) / segment_util << std::endl;
	//out << "key,size,inserted" << std::endl;
	// for (auto& item : objects) {
	//	out << item.kId << "," << item.size << "," << item.inserted << std::endl;
	//}
}

inline static uint64_t rotl64 ( uint64_t x, int8_t r ) {
    return (x << r) | (x >> (64 - r));
}

inline static uint64_t fmix64(uint64_t k) {
	k ^= k >> 33;
	k *= 0xff51afd7ed558ccdllu;
	k ^= k >> 33;
	k *= 0xc4ceb9fe1a85ec53llu;
	k ^= k >> 33;

	return k;
}


inline static uint64_t getblock64(const uint64_t * p, int i) {
	return p[i];
}



uint128_t SegmentUtil:: MurmurHash3_x64_128(const void * key,
					const int len,
					const uint32_t seed) {

	const uint8_t * data = (const uint8_t*)key;
	const int nblocks = len / 16;

	uint64_t h1 = seed;
	uint64_t h2 = seed;

	uint64_t c1 = 0x87c37b91114253d5llu;
	uint64_t c2 = 0x4cf5ad432745937fllu;

	//----------
	// body

	const uint64_t * blocks = (const uint64_t *)(data);

	for(int i = 0; i < nblocks; i++) {

		uint64_t k1 = getblock64(blocks,i*2+0);
		uint64_t k2 = getblock64(blocks,i*2+1);

		k1 *= c1; 
		k1  = rotl64(k1,31); 
		k1 *= c2; 
		h1 ^= k1;

		h1 = rotl64(h1,27); 
		h1 += h2; 
		h1 = h1*5+0x52dce729;

		k2 *= c2; 
		k2  = rotl64(k2,33); 
		k2 *= c1; h2 ^= k2;

		h2 = rotl64(h2,31); 
		h2 += h1; 
		h2 = h2*5+0x38495ab5;
	}			

	//----------
	// tail

	const uint8_t * tail = (const uint8_t*)(data + nblocks*16);

	uint64_t k1 = 0;
	uint64_t k2 = 0;

	switch(len & 15) {
		case 15: k2 ^= (uint64_t)(tail[14]) << 48;
		case 14: k2 ^= (uint64_t)(tail[13]) << 40;
		case 13: k2 ^= (uint64_t)(tail[12]) << 32;
		case 12: k2 ^= (uint64_t)(tail[11]) << 24;
		case 11: k2 ^= (uint64_t)(tail[10]) << 16;
		case 10: k2 ^= (uint64_t)(tail[ 9]) << 8;
		case  9: k2 ^= (uint64_t)(tail[ 8]) << 0;
		k2 *= c2; k2  = rotl64(k2,33); k2 *= c1; h2 ^= k2;

		case  8: k1 ^= (uint64_t)(tail[ 7]) << 56;
		case  7: k1 ^= (uint64_t)(tail[ 6]) << 48;
		case  6: k1 ^= (uint64_t)(tail[ 5]) << 40;
		case  5: k1 ^= (uint64_t)(tail[ 4]) << 32;
		case  4: k1 ^= (uint64_t)(tail[ 3]) << 24;
		case  3: k1 ^= (uint64_t)(tail[ 2]) << 16;
		case  2: k1 ^= (uint64_t)(tail[ 1]) << 8;
		case  1: k1 ^= (uint64_t)(tail[ 0]) << 0;
		k1 *= c1; k1  = rotl64(k1,31); k1 *= c2; h1 ^= k1;
	};

	//----------
	// finalization

	h1 ^= len; 
	h2 ^= len;

	h1 += h2;
	h2 += h1;

	h1 = fmix64(h1);
	h2 = fmix64(h2);

	h1 += h2;
	h2 += h1;

	return ((uint128_t)h2 << 64) | h1;
}
