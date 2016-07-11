#include <cassert>
#include "lruk.h"

unsigned int K_LRU = 8;
unsigned int KLRU_QUEUE_SIZE = 1024;


Lruk::Lruk(stats stat) 
	: policy(stat)
	, kLruSizes(K_LRU, 0)
	, kLru(K_LRU)
	, allObjects()
	, kLru_hits(K_LRU,0)
{
	assert(K_LRU >= 1);
}

Lruk::~Lruk() {}

size_t Lruk::get_bytes_cached() const {
	unsigned int bytesCached = 0;
	for (int sizeQueue : kLruSizes) {
		bytesCached += sizeQueue;	
	}
	return bytesCached;
}

size_t Lruk::proc(const request *r, bool warmup) {
	if (!warmup) {stat.accesses++;}

	auto searchRKId = allObjects.find(r->kid);
	if (searchRKId != allObjects.end()) {
		Lruk::LKItem& item = searchRKId->second;
		int qN = item.queueNumber;
		kLru[qN].erase(item.iter);
		kLruSizes[qN] -= item.size;
		if (r->size() == item.size){
			if (!warmup) {
				stat.hits++;
				kLru_hits[qN]++;
			}
			if ((qN + 1) != (int)K_LRU) { qN++; }
			std::vector<uint32_t> objects{r->kid};
			insert(objects, r->size(),qN);
			return 1;	
		} else {
			allObjects.erase(item.kId);
		}	
	}
	Lruk::LKItem newItem;
	newItem.kId = r->kid;
	newItem.size = r->size();
	newItem.queueNumber = 0;
	allObjects[r->kid] = newItem;
	std::vector<uint32_t> objects{r->kid};
	insert(objects,r->size(),0);
	return PROC_MISS;
}

void Lruk::insert(std::vector<uint32_t>& objects, unsigned int sum, int k) {
	assert(0 <= k && k < (int) K_LRU);
	std::vector<uint32_t> newObjects;
	int newSum = 0;
	while (sum + kLruSizes[k] > KLRU_QUEUE_SIZE) {
		assert(kLruSizes[k] > 0);
		uint32_t elem = kLru[k].back();
		kLru[k].pop_back();
		Lruk::LKItem& item = allObjects[elem];
		kLruSizes[k] -= item.size;
		if (k > 0) {
			newSum += item.size;
			newObjects.emplace_back(elem);
		} else {
			allObjects.erase(elem);	
		}
	}

	for (const uint32_t& elem : objects) {
		Lruk::LKItem& item = allObjects[elem];
		kLru[k].emplace_front(elem);
		item.iter = kLru[k].begin();
		item.queueNumber = k;
		kLruSizes[k] += item.size;
	}	
	if (k > 0 && newObjects.size() > 0) {
		insert(newObjects, newSum, k-1);
	}	
}

void Lruk::dump_stats(void) {
	assert(stat.apps.size() == 1);
	uint32_t appId = 0;
	for(const auto& app : stat.apps) {appId = app;}
	std::string filename{stat.policy
			+ "-app" + std::to_string(appId)
			+ "-K" + std::to_string(K_LRU)
			+ "-QSize" + std::to_string(KLRU_QUEUE_SIZE)};
	std::ofstream out{filename};
	out << "K_LRU (number of queues) " << K_LRU << std::endl;
	out << "queue size " << KLRU_QUEUE_SIZE << std::endl;
        out << "#accesses "  << stat.accesses << std::endl;
        out << "#global hits " << stat.hits << std::endl;
	out << "hit rate " << double(stat.hits) / stat.accesses  << std::endl;
	for (unsigned int i = 0 ; i < kLru_hits.size(); i++) {
		out << "queue " << i << " hits: " << kLru_hits[i] << std::endl;
		out << "queue " << i << " hit rate: " << double(kLru_hits[i]) / stat.accesses << std::endl;
	}
}

