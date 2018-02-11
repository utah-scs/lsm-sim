#ifndef FLASH_CACHE_LRUK_CLK_MACHINE_LEARNING_H
#define FLASH_CACHE_LRUK_CLK_MACHINE_LEARNING_H

#include <vector>
#include <iostream>
#include <unordered_map>
#include <list>
#include <cassert>
#include "policy.h"

/*
* These parameters define how many bytes the DRAM
* and flash can hold. These parametrs can be changed
*/
extern size_t FC_K_LRU_CLK_ML;
extern size_t FC_KLRU_QUEUE_SIZE_CLK_ML;
extern size_t DRAM_SIZE_FC_KLRU_CLK_ML;
extern size_t FLASH_SIZE_FC_KLRU_CLK_ML;
extern size_t CLOCK_MAX_VALUE_KLRU_ML;
extern uint32_t APP_NUMBER;
extern double ML_SVM_TH;

class FlashCacheLrukClkMachineLearning : public Policy {
private:
	typedef std::list<std::pair<uint32_t, size_t> >::iterator dramIt;
	typedef std::list<uint32_t>::iterator keyIt;
    typedef std::list<uint32_t>::iterator DramkeyIt;

    struct DramItem {
        
        //SVM calculation
        uint32_t 	kId;
        double 		AvgTimeBetweenhits;
        double 		TimeBetweenLastAction;
        uint32_t	AmountOfHitsSinceArrivel;
        double 		LastAction;
        double		MaxTimeBetweenHits;
        double		FirstHitTimePeriod;
        double      SVMResult;
        
        DramItem() :kId(0),AvgTimeBetweenhits(0),TimeBetweenLastAction(0),AmountOfHitsSinceArrivel(0),LastAction(0),MaxTimeBetweenHits(0),FirstHitTimePeriod(0),SVMResult(0){}
    };
    
    struct SVMItem {
        double		probability;
        
        //SVM calculation
        double 		AvgTimeBetweenhits;
        double 		TimeBetweenLastAction;
        uint32_t	AmountOfHitsSinceArrivel;
        uint32_t	AmountsOfHitsTillEviction;
        double 		LastAction;
        double		MaxTimeBetweenHits;
        
        //SVM calculation
        double 		r_AvgTimeBetweenhits;
        double 		r_TimeBetweenLastAction;
        uint32_t	r_AmountOfHitsSinceArrivel;
        uint32_t	r_AmountsOfHitsTillEviction;
        double		r_MaxTimeBetweenHits;
        double		r_FirstHitTimePeriod;
        
        SVMItem() :probability(1),AvgTimeBetweenhits(0),TimeBetweenLastAction(0),AmountOfHitsSinceArrivel(0),AmountsOfHitsTillEviction(0),LastAction(0),MaxTimeBetweenHits(0),r_AvgTimeBetweenhits(0),r_TimeBetweenLastAction(0),r_AmountOfHitsSinceArrivel(0),r_AmountsOfHitsTillEviction(0),r_MaxTimeBetweenHits(0),r_FirstHitTimePeriod(0){}
    };
    
	struct Item {
		uint32_t 	kId;
		int32_t 	size;
		bool 		isInDram;
		uint8_t		queueNumber;

        //SVM calculation
        bool 		hasDramItem;
        bool 		hasSVMItem;
        double      LastAction;

        
        //Pointers
        std::list<DramItem>::iterator          DramItemListLocation;
		std::list<SVMItem>::iterator          SVMObjectListLocation;
        DramkeyIt 	dramLocation;
        keyIt 		flashIt;
        dramIt 		ClockIt;
        keyIt       dramlruIt;
	
		Item() : kId(0), size(0), isInDram(true), queueNumber(0),
               hasDramItem(false), hasSVMItem(false), LastAction(0),
               DramItemListLocation(), SVMObjectListLocation(), dramLocation(), flashIt(), ClockIt(),dramlruIt(){}
	};


	std::vector< std::list<uint32_t>> dram;
    std::list<DramItem> dramItemList;
    std::list<SVMItem> SVMCalculationItemList;
	std::list<uint32_t> dramLru;
	std::list<std::pair<uint32_t, size_t> > clockLru;
	std::vector<size_t> kLruSizes;
    std::vector<size_t> kLruAmountOfSVM;
	std::list<uint32_t> flash;

	std::unordered_map<uint32_t, Item> allObjects;	
	
	size_t dramSize;
	size_t flashSize;
	dramIt clockIt;
	bool firstEviction;
	bool SVMCalculationRun;
    double AmountOfSVM_1;

	void dramAdd(std::vector<uint32_t>& objects,
			size_t sum,
			size_t k,
			bool updateWrites,
			bool warmup,
			bool NewItem);
    void dramAddandReorder(std::vector<uint32_t>& objects,
           size_t sum,
           size_t k,
           bool updateWrites,
           bool warmup);

	void deleteItem(uint32_t keyId);
	bool inTimesforinsert(const Request *r);
	bool inTimesforupdate(const Request *r);
	void SVMWarmUPCalculation(const Request *r,double v1, bool warmup);
	void ClockFindItemToErase(const Request *r);
	void SVMFunctionCalculation();
    void ColectItemDataAndPredict(const Request *r, bool warmup, bool Predict);
	friend class VictimCache;

public:
	FlashCacheLrukClkMachineLearning(stats stat);
	~FlashCacheLrukClkMachineLearning();
	size_t process_request(const Request *r, bool warmup);
	size_t get_bytes_cached() const;
	void dump_stats(void);
};

#endif
