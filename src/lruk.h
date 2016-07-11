#ifndef LRUK_H
#define LRUK_H

#include <iostream>
#include <vector>
#include <list>
#include <unordered_map>
#include "policy.h"

extern unsigned int K_LRU;
extern unsigned int KLRU_QUEUE_SIZE;

class Lruk : public policy {
private:
	typedef std::list<uint32_t>::iterator keyIt;

	struct LKItem {
		uint32_t kId;
		int32_t size;
		keyIt iter;
		int queueNumber;
		
		LKItem() : kId(0), size(0), iter(), queueNumber(0){}		
	};

	std::vector<unsigned int> kLruSizes;
	std::vector< std::list<uint32_t> > kLru;
	std::unordered_map<uint32_t, LKItem> allObjects;
	std::vector<unsigned int> kLru_hits;

	void insert(std::vector<uint32_t>& objects, unsigned int sum, int k);
public:
	Lruk(stats stat);
	~Lruk();
	size_t proc(const request *r, bool warmup);
	size_t get_bytes_cached() const;
	void dump_stats(void);
};

#endif
