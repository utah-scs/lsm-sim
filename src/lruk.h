#ifndef LRUK_H
#define LRUK_H

#include <iostream>
#include <vector>
#include <list>
#include <unordered_map>
#include "policy.h"

extern size_t K_LRU;
extern size_t KLRU_QUEUE_SIZE;

class Lruk : public Policy {
private:
	typedef std::list<uint32_t>::iterator keyIt;

	struct LKItem {
		uint32_t kId;
		int32_t size;
		keyIt iter;
		size_t queueNumber;
		
		LKItem() : kId(0), size(0), iter(), queueNumber(0){}		
	};

	std::vector<size_t> kLruSizes;
	std::vector< std::list<uint32_t> > kLru;
	std::unordered_map<uint32_t, LKItem> allObjects;
	std::vector<size_t> kLruHits;
	std::vector<size_t> kLruNumWrites;

	void insert(std::vector<uint32_t>& objects, 
		size_t sum, 
		size_t k, 
		bool updateWrites,
		bool warmup);
public:
	Lruk(stats stat);
	~Lruk();
	size_t process_request(const Request *r, bool warmup);
	size_t get_bytes_cached() const;
	void dump_stats(void);
};

#endif
