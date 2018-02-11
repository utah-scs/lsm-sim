#ifndef FLASH_SHIELD_H
#define FLASH_SHIELD_H

#include <vector>
#include <iostream>
#include <unordered_map>
#include <list>
#include <cassert>
#include "policy.h"
#include "flash_cache.h"

extern  uint32_t FLASH_SHILD_APP_NUMBER;
extern  size_t   FLASH_SHILD_DRAM_SIZE;
extern  size_t   FLASH_SHILD_FLASH_SIZE;
extern  uint32_t FLASH_SHILD_TH;

class flashshield : public FlashCache {
protected:
    struct Block;
    typedef std::list<Block>::iterator blockIt;
    typedef std::list<std::pair<uint32_t, size_t> >::iterator ClockIt;
    typedef std::list<uint32_t>::iterator keyIt;
    
    struct DramItem {
        
        //SVM calculation
        double 		AvgTimeBetweenhits;
        double 		TimeBetweenLastAction;
        uint32_t	AmountOfHitsSinceArrivel;
        double 		LastAction;
        double		MaxTimeBetweenHits;
        double		FirstHitTimePeriod;
        double      SVMResult;
        
        DramItem() :AvgTimeBetweenhits(0),TimeBetweenLastAction(0),AmountOfHitsSinceArrivel(0),LastAction(0),MaxTimeBetweenHits(0),FirstHitTimePeriod(0),SVMResult(0){}
    };
    
    typedef std::list<DramItem>::iterator DramListIt;
    
    struct RItem {
        blockIt flashIt;
        bool isGhost;

        uint32_t 	kId;
        int32_t 	size;
        bool 		isInDram;
        uint8_t		queueNumber;
        bool        hasItem;
        double          LastAction;
        
        //Pointers
        keyIt           dramLocation;
        keyIt           dramlruIt;
        DramListIt      DramItemListLocation;
        ClockIt         clockIt;
        
        
        RItem() : flashIt{}, isGhost(false), kId(0), size(0), isInDram(true), queueNumber(0),hasItem(false),LastAction(0),
                dramLocation(), dramlruIt(), DramItemListLocation(), clockIt() {}
        
//        RItem(const Request *r, size_t counter) : FlashCache::Item(), flashIt{}, isGhost(false) {
//            kId = r->kid;
//            size = r->size();
//            isInDram = true;
//            last_accessed = r->time;
//            lastAccessInTrace = counter;
//        }
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
    
    std::vector<std::list<uint32_t>> dram;
    std::list<DramItem> dramItemList;
    std::list<uint32_t> dramLru;
    std::list<std::pair<uint32_t, size_t> > clockLru;
    std::vector<size_t> kLruSizes;
    std::vector<size_t> kLruAmountOfSVM;
    
    ClockIt     GlobalclockIt;
    bool        SVMCalculationRun;
    double      AmountOfSVM_1;
    size_t      dramSize;
    size_t      flashSize;
    size_t      svm_size;
   
    
    void add_item(RItem &newItem);
    virtual void evict_item(RItem& victimItem, bool warmup,const Request *r);
    void evict_block(blockIt victim_block, bool warmup,const Request *r);
    uint32_t ClockFindItemToErase(const Request *r);
    void ColectItemDataAndPredict(const Request *r, bool warmup, bool Predict);
    void allocate_flash_block(bool warmup,const Request *r);
    
    void dramAddandReorder(
         std::vector<uint32_t>& objects,
         size_t sum,
         size_t k,
         bool updateWrites,
         bool warmup);
    
    void dramAdd(
         std::vector<uint32_t>& objects,
         size_t sum,
         size_t k,
         bool updateWrites,
         bool warmup,
         bool NewItem);
   std::ofstream out;
 
public:
    flashshield(stats stat);
    ~flashshield();
    size_t proc(const Request *r, bool warmup);
    void dump_stats(void);
    
};

#endif
