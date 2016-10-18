#include <iostream>
#include <math.h>
#include <fstream>
#include "flash_shield.h"
#include </usr/include/python2.7/Python.h>
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
#include <random>

size_t FLASH_SHILD_K_LRU_QUEUES = 8;
size_t FLASH_SHILD_DRAM_SIZE = 51209600;
size_t FLASH_SHILD_KLRU_QUEUE_SIZE = FLASH_SHILD_DRAM_SIZE / FLASH_SHILD_K_LRU_QUEUES;
size_t FLASH_SHILD_FLASH_SIZE = 51209600;
size_t FLASH_SHILD_CLOCK_MAX_VALUE = 7;
int FLASH_SHILD_MIN_QUEUE_TO_MOVE_TO_FLASH = 1;

uint32_t FLASH_SHILD_APP_NUMBER =0;
bool DidntFindMFU = false;
double NumberOfSetsOperations=0;
double SizeOfSetsOperations=0;

double Min_Block_Fill_Threshold=0.95;
double FLASH_SHILD_BLOCK_SIZE = 8*1024*1024;
double AmountOfSizeToGoOverWhileAllocatingBlock = 2*FLASH_SHILD_BLOCK_SIZE;
double START_TIME = 0;
double END_TIME = 82799;
double TIME_TO_RUN_SVM = 86400;
double SVM_TH = 1;

//Python variables
PyObject* SVMPredictFunction=NULL;
PyObject* myModuleString=NULL;
PyObject* myModule = NULL;
PyObject* SVMFitFunction = NULL;
PyObject* SVMLoadFitFunction = NULL;
PyObject* SVMLoadedFitFunction = NULL;

flashshield::flashshield(stats stat):
FlashCache(stat),
flash{},
allObjects{},
maxBlocks{},
numBlocks{},
dram(FLASH_SHILD_K_LRU_QUEUES),
dramItemList(),
dramLru(0),
clockLru(),
kLruSizes(FLASH_SHILD_K_LRU_QUEUES, 0),
kLruAmountOfSVM(FLASH_SHILD_K_LRU_QUEUES,0),
GlobalclockIt(),
SVMCalculationRun(false),
AmountOfSVM_1(0),
dramSize(0),
flashSize(0)
{
    maxBlocks = FLASH_SHILD_FLASH_SIZE/FLASH_SHILD_BLOCK_SIZE;
    
    //Initialize SVM Functions
    Py_Initialize();
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("sys.path.append(\".\")");
    
    myModuleString = PyUnicode_FromString((char*) "machine_learning_flashiness");
    myModule = PyImport_Import(myModuleString);
    
    if (myModule == NULL) {
        PyErr_Print();
        printf("ERROR importing module");
        exit(-1);
    }
    
    SVMFitFunction = PyObject_GetAttrString(myModule,"FitFunction");
    SVMPredictFunction = PyObject_GetAttrString(myModule,"PredictFunction");
    SVMLoadFitFunction = PyObject_GetAttrString(myModule,"LoadSavedFunction");
}

uint32_t S_KID=0;
double S_TIME=0;

flashshield::~flashshield() {}

size_t flashshield::proc(const request* r, bool warmup) {
    
    S_KID = r->kid;
    S_TIME= r->time;
    
    if (!warmup) {stat.accesses++;}
    //counter++;
    bool updateWrites = true;
    //double currTime = r->time;
    
    assert(dramSize + flashSize <= FLASH_SHILD_DRAM_SIZE + FLASH_SHILD_FLASH_SIZE*stat.threshold);
    auto searchRKId = allObjects.find(r->kid);
    if (searchRKId != allObjects.end()) {
        /*
         * The object exists in system. If the sizes of the
         * current request and previous request differ then the previous
         * request is removed. Otherwise, one
         * needs to update the hitrate and its place in the globalLru.
         * If it is in the cache, one needs also to update the
         * 'flashiness' value and its place in the dram MFU and dram LRU
         */
        flashshield::RItem& item = searchRKId->second;
        flashshield::ClockIt& clockItemIt = item.clockIt;
        
        if (r->size() == item.size) {
            //HIT
            if (!warmup) {stat.hits++;}
            
            if (!item.isGhost)
            {
                clockItemIt->second = FLASH_SHILD_CLOCK_MAX_VALUE;
            //    assert(item.globalLruIt != globalLru.end());
            //    globalLru.erase(item.globalLruIt);
            }
            //globalLru.emplace_front(item.kId);
            //item.globalLruIt = globalLru.begin();
            
            if (item.isInDram) {
                if (!warmup) {stat.hits_dram++;}
                
                size_t qN = item.queueNumber;
                
                //--------dramLRU ---------------
                if (item.dramlruIt != dramLru.end()) {dramLru.erase(item.dramlruIt);}
                dramLru.emplace_front(item.kId);
                item.dramlruIt = dramLru.begin();
                //-------------------------------

                dram[qN].erase(item.dramLocation);
                kLruSizes[qN] -= item.size;
                
                if (item.hasItem)
                {
                    if (item.DramItemListLocation->SVMResult)
                    {
                        kLruAmountOfSVM[qN]--;
                    }
                }
                
                dramSize -= item.size;

                if ((qN + 1) != FLASH_SHILD_K_LRU_QUEUES) {
                    qN++;
                } else {
                    updateWrites = false;
                }
                
                std::vector<uint32_t> objects{item.kId};
                dramAddandReorder(objects, r->size(),qN, updateWrites, warmup);
                
            } else {//Item is in flash
                if (!warmup) {stat.hits_flash++;}
                
                if (item.isGhost)
                {//If got hit for item that is a ghost need to return it to action
                    item.isGhost = false;
                    item.flashIt->size += item.size;
                    flashSize += item.size;
                    
                    std::pair<uint32_t,size_t> it;
                    it =  std::make_pair(item.kId, FLASH_SHILD_CLOCK_MAX_VALUE);
                    
                    if (clockLru.size() == 0) {
                        clockLru.emplace_front(it);
                        GlobalclockIt = clockLru.begin();
                        item.clockIt = clockLru.begin();
                    } else {
                        clockLru.insert(GlobalclockIt,it);
                        ClockIt Clkit = GlobalclockIt;
                        Clkit--;
                        item.clockIt = Clkit;
                    }
                    
                    while (((double)(dramSize + flashSize) > (double)(FLASH_SHILD_DRAM_SIZE + (double)FLASH_SHILD_FLASH_SIZE*stat.threshold)))
                    {
                        uint32_t globalLruKid = ClockFindItemToErase(r);
                        flashshield::RItem& victimItem = allObjects[globalLruKid];
                        assert(victimItem.size > 0);
                        evict_item(victimItem, warmup,r);
                    }
                    
                    assert(dramSize + flashSize <= FLASH_SHILD_DRAM_SIZE + FLASH_SHILD_FLASH_SIZE*stat.threshold);
                }
            }
            
            //----------------------SVM-calculation-----------------------
            if (item.hasItem)
            {
                ColectItemDataAndPredict(r, warmup,true);
            }else{
                if (item.isInDram)
                {// create SVMItem only for dram items
                    //**********
                    flashshield::DramItem dramitem;
                    dramitem.FirstHitTimePeriod= r->time - item.LastAction;
                    item.hasItem=true;
                    dramItemList.emplace_front(dramitem);
                    item.DramItemListLocation = dramItemList.begin();
                    ColectItemDataAndPredict(r, warmup,true);
                }
            }
            //------------------------------------------------------------
            
            item.LastAction = r->time;
            return 1;
            
        } else {
            //UPDATE
            
            if (!item.isInDram)
                item.flashIt->items.remove(item.kId);
            
            if (!item.isGhost)
            {
                evict_item(item, warmup,r);
            }
            
            if (!item.isInDram)
            {
                //item is a ghost - need to erase it from allObjects list
                assert(allObjects.find(item.kId) != allObjects.end());
                allObjects.erase(item.kId);
            }

        }
    }
    //MISS
    
    /*
     * The request doesn't exist in the system. We always insert new requests
     * to the DRAM.
     */
    
    flashshield::RItem newItem;
    newItem.kId = r->kid;
    newItem.size = r->size();
    newItem.isInDram = true;
    newItem.LastAction = r->time;
    
    assert(((unsigned int) newItem.size) <= FLASH_SHILD_KLRU_QUEUE_SIZE);
    assert(dramSize + flashSize <= FLASH_SHILD_DRAM_SIZE + FLASH_SHILD_FLASH_SIZE*stat.threshold);
    DidntFindMFU = false;
    
    while (true)
    {
        if ((newItem.size + dramSize <= FLASH_SHILD_DRAM_SIZE) && ((double)(dramSize + flashSize + newItem.size) <= (double)(FLASH_SHILD_DRAM_SIZE + (double)FLASH_SHILD_FLASH_SIZE*stat.threshold)))
        {
            allObjects[newItem.kId] = newItem;
            
            std::vector<uint32_t>  objects{newItem.kId};
            dramAdd(objects, r->size(),0, true, warmup, true);
            //add_item(newItem);
            
            NumberOfSetsOperations++;
            SizeOfSetsOperations+=newItem.size;
            return PROC_MISS;
        }
        
        assert(numBlocks <= maxBlocks);
        
        //Not enough space in DRAM, check flash
        if ((double)(dramSize + flashSize + newItem.size) >= (double)(FLASH_SHILD_DRAM_SIZE + (double)FLASH_SHILD_FLASH_SIZE*stat.threshold))
        {
         //If not enough space both in dram and in flash - evict item by global LRU (Clock)
            uint32_t globalLruKid = ClockFindItemToErase(r);
            flashshield::RItem& victimItem = allObjects[globalLruKid];
            assert(victimItem.size > 0);
            evict_item(victimItem, warmup,r);
            assert(dramSize + flashSize <= FLASH_SHILD_DRAM_SIZE + FLASH_SHILD_FLASH_SIZE*stat.threshold);
        }
        else if (numBlocks == maxBlocks)
        {
            evict_block(--flash.end(),warmup,r);
            assert(dramSize + flashSize <= FLASH_SHILD_DRAM_SIZE + FLASH_SHILD_FLASH_SIZE*stat.threshold);
        }
        else if (numBlocks < maxBlocks)
        {//If we have spce in flash for block- allocate block and move it to flash
            allocate_flash_block(warmup,r);
        }
        else
        {
            //if there is no place in dram and all the blocks are full (The flash isnt full
            //since it's size might be bigger then the the size of all the blocks
            assert(0);
        }
        assert(numBlocks <= maxBlocks);
    }

    assert(0);
    return PROC_MISS;
}

void flashshield::evict_item(flashshield::RItem& victimItem, bool warmup /*uint32_t &victimKid*/,const request *r)
{
    //globalLru.erase(victimItem.globalLruIt);
    //victimItem.globalLruIt = globalLru.end();
    assert(!victimItem.isGhost);
    
    //Eviciting from global LRU list
    if (victimItem.clockIt == GlobalclockIt)
    {
        GlobalclockIt++;
        if (GlobalclockIt == clockLru.end())
        {
            GlobalclockIt = clockLru.begin();
        }
    }
    
    if (victimItem.clockIt != clockLru.end())
        clockLru.erase(victimItem.clockIt);
    
    if (victimItem.isInDram)
    {
        //--------dramLRU ---------------
        dramLru.erase(victimItem.dramlruIt);
        //-------------------------------
        
        size_t dGqN = victimItem.queueNumber;
        dram[dGqN].erase(victimItem.dramLocation);
        kLruSizes[dGqN] -= victimItem.size;
        dramSize -= victimItem.size;
        
        if (victimItem.hasItem)
        {
            if (victimItem.DramItemListLocation->SVMResult)
            {
                kLruAmountOfSVM[dGqN]--;
                AmountOfSVM_1--;
            }
            dramItemList.erase(victimItem.DramItemListLocation);
            victimItem.hasItem = false;
        }
        
        allObjects.erase(victimItem.kId);

    } else {
        //Item is in flash

        victimItem.isGhost = true;
        blockIt curr_block = victimItem.flashIt;
        curr_block->size -= victimItem.size;
        flashSize -= victimItem.size;
        
        //if ((double)((double)curr_block->size/(double)FLASH_SHILD_BLOCK_SIZE) < stat.threshold)
        //{
        //    assert(dramSize <= DRAM_SIZE);
        //    evict_block(curr_block,warmup,r);
            
        //    DidntFindMFU = false;
        //    while (dramSize > FLASH_SHILD_DRAM_SIZE)
        //    {// If there is no way to allocate new block start erase dramLru
        //        allocate_flash_block(warmup,r);
        //    }
        //}

    }
}

void flashshield::evict_block(blockIt victim_block, bool warmup,const request *r)
{
    for (keyIt it = victim_block->items.begin(); it != victim_block->items.end(); it++)
    {//For every Item in the evicted block

        if (allObjects.find(*it) != allObjects.end())
        {//If a ghost was updawted we wont find it in the allObjects.find
            flashshield::RItem& victim_item = allObjects[*it];
        
            if (victim_item.isGhost)
            {//If item is Ghost delete it from memory
                allObjects.erase(victim_item.kId);
            } else {
                //Need to insert the item into dram again
                victim_item.isInDram = true;
                victim_item.flashIt = flash.end();
                
                dram[0].emplace_front(victim_item.kId);
                victim_item.dramLocation = dram[0].begin();
                victim_item.queueNumber = 0;
                dramSize += victim_item.size;
                kLruSizes[0]+=victim_item.size;
                
                //--------dramLRU ---------------
                dramLru.emplace_front(victim_item.kId);
                victim_item.dramlruIt = dramLru.begin();
                //-------------------------------
                
                victim_item.LastAction= r->time;
            }
        }
    }
    
    flash.erase(victim_block);
    flashSize -= victim_block->size;
    numBlocks--;
}


void flashshield::allocate_flash_block(bool warmup,const request *r)
{//For every item in the sent list need to evict from dram and
    assert(flashSize <= FLASH_SHILD_FLASH_SIZE);
    
    double SumOfMruObjects=0;
    double AmountOfDataChecked=0;
    std::list<uint32_t>  MRUobjects;
    bool FoundMfu = false;
    
    /* every MRU item that being found need to add to the list
     and check every time if we passed the amount needed. If does break else keep adding*/
    if (AmountOfSVM_1 && !DidntFindMFU)
    {//If we even have Item to move to flash
        
        for (int i=FLASH_SHILD_K_LRU_QUEUES-1; i >=FLASH_SHILD_MIN_QUEUE_TO_MOVE_TO_FLASH ; i--)
        {// Find the MRU item
            if (kLruAmountOfSVM[i] > 0)
            {
                bool found=false;
                for (std::list<uint32_t>::iterator tmpkId= (dram[i]).begin() ; tmpkId != (dram[i]).end();tmpkId++)
                {
                    flashshield::RItem& tmpItem = allObjects[*tmpkId];
                    //Check if SVMResult is 1
                    AmountOfDataChecked+=tmpItem.size;

                    if (tmpItem.hasItem)
                    {
                        if (tmpItem.DramItemListLocation->SVMResult)
                        {
                            if (SumOfMruObjects + (double) tmpItem.size < (double) FLASH_SHILD_BLOCK_SIZE)
                            {
                                MRUobjects.emplace_back(*tmpkId);
                                SumOfMruObjects +=tmpItem.size;
                                AmountOfSVM_1--;
                                kLruAmountOfSVM[i]--;
                                if ((double)SumOfMruObjects >= (double)FLASH_SHILD_BLOCK_SIZE * Min_Block_Fill_Threshold)
                                {//We found enough items to move to flash
                                    found = true;       /* queue's For */
                                    FoundMfu = true;    /* items in queue's For */
                                    break;
                                }
                                
                                if (AmountOfDataChecked > AmountOfDataChecked)
                                {
                                    found = true;
                                    break;
                                }
                                
                            }
                        }
                    }
                }
                if (found){break;}
            }else{FoundMfu=false;}
        }
    }
    
    if (!FoundMfu)
    {//If at the end we didnt found enough need to restore all items
        
        for (std::list<uint32_t>::iterator tmpkId= (MRUobjects).begin() ; tmpkId != (MRUobjects).end();tmpkId++)
        {
            AmountOfSVM_1++;
            flashshield::RItem& tmpItem = allObjects[*tmpkId];
            kLruAmountOfSVM[tmpItem.queueNumber]++;
        }
        
        DidntFindMFU = true;
        
        //If we don't find an MRU item to move to flash we will delete items from dram by LRU
        //--------dramLRU ---------------
        uint32_t globalLruKid = dramLru.back();
        flashshield::RItem& victimItem = allObjects[globalLruKid];
        evict_item(victimItem, warmup,r);
        //-------------------------------
        
    }else{
        
        flash.emplace_front();
        flashshield::Block &curr_block = flash.front();
    
        //auto mfu_it = --dram.end();
        for (auto mfuKid : MRUobjects)
        {
            //assert(!dram.empty());
            //uint32_t mfuKid = mfu_it->first;
            //mfu_it--;
        
            flashshield::RItem &mfuItem = allObjects[mfuKid];
            assert(mfuItem.size > 0);
        
            //--------dramLRU ---------------
            dramLru.erase(mfuItem.dramlruIt);
            mfuItem.dramlruIt = dramLru.end();
            //-------------------------------
            
            size_t qN = mfuItem.queueNumber;
            mfuItem.isInDram = false;
            dram[qN].erase(mfuItem.dramLocation);
            kLruSizes[qN]-=mfuItem.size;
            
            mfuItem.dramLocation = dram[0].end();
            mfuItem.flashIt = flash.begin();
            dramSize -= mfuItem.size;
            mfuItem.queueNumber = 8;
        
            curr_block.items.emplace_front(mfuKid);
            curr_block.size += mfuItem.size;
        
            if (mfuItem.hasItem)
            {
                if (mfuItem.DramItemListLocation->SVMResult)
                {
                    kLruAmountOfSVM[mfuItem.queueNumber]--;
                    AmountOfSVM_1--;
                }
                dramItemList.erase(mfuItem.DramItemListLocation);
                mfuItem.hasItem = false;
            }
        
            assert(!mfuItem.hasItem);
            
            assert(mfuItem.size > 0);
            assert(numBlocks <= maxBlocks);
        }
    
        assert(curr_block.size <= FLASH_SHILD_BLOCK_SIZE);
        numBlocks++;
        flashSize += curr_block.size;
        
        assert(numBlocks <= maxBlocks);
        assert(flashSize <= FLASH_SHILD_FLASH_SIZE);
    
        if (!warmup)
        {
            stat.writes_flash++;
            stat.flash_bytes_written += FLASH_SHILD_BLOCK_SIZE;
        }
    }
}

void flashshield::dramAdd(
        std::vector<uint32_t>& objects,
        size_t sum,
        size_t k,
        bool updateWrites,
        bool warmup,
        bool NewItem) {
    
    for (auto elem : objects)
    {
        flashshield::RItem& item = allObjects[elem];
        
        dram[k].emplace_front(item.kId);
        item.dramLocation = dram[k].begin();
        item.queueNumber = k;
        dramSize += item.size;
        kLruSizes[k] += item.size;
        
        if (item.hasItem)
        {
            if (item.DramItemListLocation->SVMResult)
                kLruAmountOfSVM[k]++;
        }
        
        if (k != 0){ assert(kLruSizes[k] <= FLASH_SHILD_KLRU_QUEUE_SIZE);}
        
        if (NewItem)
        { //New Item, need to insert to clocklru and update clock
            std::pair<uint32_t,size_t> it;
            it =  std::make_pair(item.kId, FLASH_SHILD_CLOCK_MAX_VALUE);
            
            if (clockLru.size() == 0) {
                clockLru.emplace_front(it);
                GlobalclockIt = clockLru.begin();
                item.clockIt = clockLru.begin();
            } else {
                clockLru.insert(GlobalclockIt,it);
                ClockIt Clkit = GlobalclockIt;
                Clkit--;
                item.clockIt = Clkit;
            }
            //--------dramLRU ---------------
            dramLru.emplace_front(item.kId);
            item.dramlruIt = dramLru.begin();
            //-------------------------------
        }
    }
}

void flashshield::dramAddandReorder(
       std::vector<uint32_t>& objects,
       size_t sum,
       size_t k,
       bool updateWrites,
       bool warmup) {
    
    assert(k < FLASH_SHILD_K_LRU_QUEUES);
    
    std::vector<uint32_t> newObjects;
    
    size_t newSum = 0;
    
    if (k != 0)
    {
        while (sum + kLruSizes[k] > FLASH_SHILD_KLRU_QUEUE_SIZE)
        {
            assert(kLruSizes[k] > 0);
            assert(dram[k].size() > 0);
            
            uint32_t elem = dram[k].back();
            
            flashshield::RItem& item = allObjects[elem];
            
            dram[k].pop_back();
            if (item.hasItem)
            {
                if (item.DramItemListLocation->SVMResult)
                    kLruAmountOfSVM[k]--;
            }
            kLruSizes[k] -= item.size;
            dramSize -= item.size;
            //saving the extracted items in order to put them in lower queue
            newSum += item.size;
            
            newObjects.emplace_back(elem);
        }
    }
    
    if (!updateWrites)
    {
        assert(newObjects.size() == 0);
        assert(newSum == 0);
    }
    
    dramAdd(objects,sum,k,updateWrites,warmup, false);
    
    if (k > 0 && newObjects.size() > 0)
    {
        dramAddandReorder(newObjects, newSum, k-1, true, warmup);
    }
}
uint32_t SvictimkId;

uint32_t flashshield::ClockFindItemToErase(const request *r)
{
    bool isDeleted = false;
    ClockIt tmpIt, startIt = GlobalclockIt;
    uint32_t victimkId;
    
    while(GlobalclockIt != clockLru.end()) {
        assert(GlobalclockIt->second <= FLASH_SHILD_CLOCK_MAX_VALUE);
        if (GlobalclockIt->second == 0) {
            // This item need to be delete
            tmpIt = GlobalclockIt;
            tmpIt++;
            victimkId=GlobalclockIt->first;
            //deleteItem(clockIt->first,r);
            isDeleted = true;
            break;
        } else {
            GlobalclockIt->second--;
        }
        GlobalclockIt++;
    }
    
    if (!isDeleted) {
        // No item was deleted moving to items on the other
        // half of the watch
        GlobalclockIt = clockLru.begin();
        assert(GlobalclockIt->second <= FLASH_SHILD_CLOCK_MAX_VALUE);
        
        while (GlobalclockIt != startIt) {
            if (GlobalclockIt->second == 0) {
                tmpIt = GlobalclockIt;
                tmpIt++;
                victimkId=GlobalclockIt->first;
                //deleteItem(clockIt->first,r);
                isDeleted = true;
                break;
            } else {
                GlobalclockIt->second--;
            }
            GlobalclockIt++;
        }
    }
    
    if (!isDeleted)
    {
        // After a full cycle no item was deleted
        // Delete the item you started with
        assert(clockLru.size() > 0);
        assert(GlobalclockIt != clockLru.end());
        tmpIt = GlobalclockIt;
        tmpIt++;
        victimkId=GlobalclockIt->first;
        //deleteItem(clockIt->first,r);
    }
    //reseting the clockIt again
    GlobalclockIt = (tmpIt == clockLru.end()) ? clockLru.begin() : tmpIt;
    
    SvictimkId = victimkId;
    return victimkId;
}

void flashshield::ColectItemDataAndPredict(const request *r, bool warmup, bool Predict)
{
    flashshield::RItem& item = allObjects[r->kid];
    
    if (item.isInDram)
    {
        flashshield::DramItem& dramitem = *item.DramItemListLocation;
        
        dramitem.AvgTimeBetweenhits = (dramitem.AvgTimeBetweenhits *(dramitem.AmountOfHitsSinceArrivel) + (r->time - dramitem.LastAction))/ (dramitem.AmountOfHitsSinceArrivel + 1);
        dramitem.TimeBetweenLastAction = r->time - dramitem.LastAction;
        dramitem.AmountOfHitsSinceArrivel++;
        if (dramitem.TimeBetweenLastAction > dramitem.MaxTimeBetweenHits)
            {dramitem.MaxTimeBetweenHits= dramitem.TimeBetweenLastAction;}
        
        if (Predict)
        {
            PyObject* args = PyTuple_Pack(6,PyFloat_FromDouble(dramitem.FirstHitTimePeriod),PyFloat_FromDouble(dramitem.AvgTimeBetweenhits),PyFloat_FromDouble(dramitem.TimeBetweenLastAction),PyFloat_FromDouble(dramitem.MaxTimeBetweenHits),PyFloat_FromDouble(dramitem.AmountOfHitsSinceArrivel),PyInt_FromLong((long)FLASH_SHILD_APP_NUMBER));
            PyObject* myResult = PyObject_CallObject(SVMPredictFunction, args);
            

            
            
            //Predict the class labele for test data sample
            dramitem.SVMResult = PyFloat_AsDouble(myResult);
            if (dramitem.SVMResult)
            {
                kLruAmountOfSVM[item.queueNumber]++;
                AmountOfSVM_1++;
            }
            
            Py_XDECREF(args);
            Py_XDECREF(myResult);
        }
        dramitem.LastAction = r->time;
    }
}

void flashshield::dump_stats(void) {
    //policy::dump_stats();

    std::string filename{stat.policy
        + "-app" + std::to_string(FLASH_SHILD_APP_NUMBER)};
    std::ofstream out{filename};
    
    out << "dram size " << FLASH_SHILD_DRAM_SIZE << std::endl;
    out << "flash size " << FLASH_SHILD_FLASH_SIZE << std::endl;
    out << "#accesses "  << stat.accesses << std::endl;
    out << "#global hits " << stat.hits << std::endl;
    out << "#dram hits " << stat.hits_dram << std::endl;
    out << "#flash hits " << stat.hits_flash << std::endl;
    out << "hit rate " << double(stat.hits) / stat.accesses << std::endl;
    out << "#writes to flash " << stat.writes_flash << std::endl;
    out << "credit limit " << stat.credit_limit << std::endl;
    out << "#bytes written to flash " << stat.flash_bytes_written << std::endl;
    out << "#of SET Operations " << NumberOfSetsOperations << std::endl;
    out << "Total size if SET operations " << SizeOfSetsOperations << std::endl;

    out << std::endl << std::endl;
    
    out << "Amount of lru queues: " << FLASH_SHILD_K_LRU_QUEUES << std::endl;
    for (size_t i =0 ; i < FLASH_SHILD_K_LRU_QUEUES ; i++){
        out << "dram[" << i << "] has " << dram[i].size() << " items" << std::endl;
        out << "dram[" << i << "] size written " << kLruSizes[i] << std::endl;
    }
    out << "Total dram filled size " << dramSize << std::endl;
}
