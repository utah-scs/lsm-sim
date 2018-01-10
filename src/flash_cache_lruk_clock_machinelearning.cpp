#include <iostream>
#include <math.h>
#include <fstream>
#include "flash_cache_lruk_clock_machinelearning.h"
#include </usr/include/python2.7/Python.h>
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
#include <random>


size_t FC_K_LRU_CLK_ML = 8;
size_t DRAM_SIZE_FC_KLRU_CLK_ML = 51209600;
size_t FC_KLRU_QUEUE_SIZE_CLK_ML = DRAM_SIZE_FC_KLRU_CLK_ML / FC_K_LRU_CLK_ML;
size_t FLASH_SIZE_FC_KLRU_CLK_ML = 51209600;
size_t CLOCK_MAX_VALUE_KLRU_ML = 7;
int MIN_QUEUE_TO_MOVE_TO_FLASH_ML = 1;
double ML_START_TIME = 0;
double ML_END_TIME = 82799;
double ML_TIME_TO_RUN_SVM = 86400;
double ML_SVM_TH = 1;
uint32_t APP_NUMBER=0;

PyObject* ML_SVMPredictFunction=NULL;
PyObject* ML_myModuleString=NULL;
PyObject* ML_myModule = NULL;
PyObject* ML_SVMFitFunction = NULL;
PyObject* ML_SVMLoadFitFunction = NULL;

FlashCacheLrukClkMachineLearning::FlashCacheLrukClkMachineLearning(stats stat) :
	policy(stat),
	dram(FC_K_LRU_CLK_ML),
    dramItemList(),
    SVMCalculationItemList(),
	dramLru(0),
	clockLru(),
	kLruSizes(FC_K_LRU_CLK_ML, 0),
    kLruAmountOfSVM(FC_K_LRU_CLK_ML, 0),
    flash(),
	allObjects(),
	dramSize(0),
	flashSize(0),
	clockIt(),
    firstEviction(false),
	SVMCalculationRun(false),
    AmountOfSVM_1(0)
{

	Py_Initialize();
	PyRun_SimpleString("import sys");
	PyRun_SimpleString("sys.path.append(\".\")");

	ML_myModuleString = PyUnicode_FromString((char*) "machine_learning_flashiness");
	ML_myModule = PyImport_Import(ML_myModuleString);

	if (ML_myModule == NULL)
    {
		PyErr_Print();
			printf("ERROR importing module");
			exit(-1);
	 }

    ML_SVMFitFunction = PyObject_GetAttrString(ML_myModule,"FitFunction");
    ML_SVMPredictFunction = PyObject_GetAttrString(ML_myModule,"PredictFunction");
    ML_SVMLoadFitFunction = PyObject_GetAttrString(ML_myModule,"LoadSavedFunction");
}

FlashCacheLrukClkMachineLearning::~FlashCacheLrukClkMachineLearning() {}

size_t FlashCacheLrukClkMachineLearning::get_bytes_cached() const {
	return dramSize + flashSize;
}

size_t FlashCacheLrukClkMachineLearning::proc(const request* r, bool warmup) {
     
	if (!warmup) {stat.accesses++;}
	bool updateWrites = true;
	uint32_t mfuKid=0;

    std::random_device rd;
   	std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0, 1);

	//If time to run SVM calculation (and this run hasn't happened yet
	if (r->time >= ML_TIME_TO_RUN_SVM && !SVMCalculationRun )
	{
		SVMFunctionCalculation();
	}


	/* 1. When request is being inserted to the system, it being looked for
	 * if the request is in the DRAM or in the flash then the system returns 1,
	 * else return 0 and inserting it
	 */
	auto searchRKId = allObjects.find(r->kid);
	if (searchRKId != allObjects.end()) {
		// Object found

		FlashCacheLrukClkMachineLearning::Item& item = searchRKId->second;
		FlashCacheLrukClkMachineLearning::dramIt& clockItemIt = item.ClockIt;

		if (r->size() == item.size) {
			if (!warmup) {stat.hits++;}

			//reseting the clock
			clockItemIt->second = CLOCK_MAX_VALUE_KLRU_ML;

			if (item.isInDram)
			{

				if (!warmup) {stat.hits_dram++;}

				size_t qN = item.queueNumber;
				//--------dramLRU ---------------
                if (item.dramlruIt != dramLru.end()) {dramLru.erase(item.dramlruIt);}
                dramLru.emplace_front(item.kId);
                item.dramlruIt = dramLru.begin();
				//-------------------------------

				dram[qN].erase(item.dramLocation);
				kLruSizes[qN] -= item.size;
				dramSize -= item.size;

                if (item.hasDramItem)
                    if (item.DramItemListLocation->SVMResult)
                        kLruAmountOfSVM[qN]--;
                
				if ((qN + 1) != FC_K_LRU_CLK_ML)
                {
					qN++;
				} else {
					updateWrites = false;
				}

				std::vector<uint32_t> objects{item.kId};
				dramAddandReorder(objects, r->size(),qN, updateWrites, warmup);

			} else {
				if (!warmup)
				{
					stat.hits_flash++;
				}
			}

			//----------------------SVM-calculation-----------------------
			double v1 = dis(gen);
            if (item.hasDramItem)
            {
                if (!warmup)
                {
                    ColectItemDataAndPredict(r, warmup,true);
                }else{
                    ColectItemDataAndPredict(r, warmup,false);
                }
            }else{
                if (item.isInDram)
                {
                    FlashCacheLrukClkMachineLearning::DramItem dramitem;
                    item.hasDramItem=true;
                    dramitem.kId = item.kId;
                    dramItemList.emplace_front(dramitem);
                    item.DramItemListLocation = dramItemList.begin();
                    ColectItemDataAndPredict(r, warmup,true);
                }
            }
            if (warmup)
            {//Collect data only in warmup
                SVMWarmUPCalculation(r,v1,warmup);
            }
			//------------------------------------------------------------

            return 1;

		} else {// Size updated
			//-----------------------------SVM-calculation----------------
			if (inTimesforupdate(r))
			{
				if (item.hasSVMItem)
				{
					SVMCalculationItemList.erase(item.SVMObjectListLocation);
				}
		     }
			//------------------------------------------------------------

			if (clockItemIt == clockIt)
			{
			// changing the clockIt since this item is being deleted
				clockIt++;
				if (clockIt == clockLru.end()) {
					clockIt = clockLru.begin();
				}
			}
            
			deleteItem(r->kid);
		}
	}

	/*
	* The request doesn't exist in the system or it's size was updated.
	* The new request will be inseret to the DRAM at the beginning of the last dram queue.
	*
	* 2. While (object not inserted to the DRAM)
	*	2.1  if (item.size() + dramSize <= DRAM_SIZE_FC_KLRU) - //queue 0 is not bound by size
	*		insert item to the dram in queue 0 and return 0.
	* 	2.2	else
	* 		2.2.1 if there is an item to move from DRAM to flash - by Dram LRUK with considuration in SVM
	*			  2.1.1.1 if there is space in flash move it to the flash.
	*			  2.1.1.2 else - remove item by Global clock
	*		2.2.2 else - remove item from dram by dramLRU
	*/

	FlashCacheLrukClkMachineLearning::Item newItem;
	newItem.kId = r->kid;
	newItem.size = r->size();
	newItem.isInDram = true;

	assert(((unsigned int) newItem.size) <= FC_KLRU_QUEUE_SIZE_CLK_ML);

	while (true) {

		if (!firstEviction) { firstEviction = true;}
		bool FoundMfu = false;

		if (newItem.size + dramSize <= DRAM_SIZE_FC_KLRU_CLK_ML) {
			// If we have place in the dram insert the new item to the beginning of the last queue.

			allObjects[newItem.kId] = newItem;
            
            std::vector<uint32_t>  objects{newItem.kId};
            dramAdd(objects, r->size(),0, true, warmup, true);

			return PROC_MISS;
		}

		// If we don't have enough space in the dram, we can move MRU items to the flash
		// or to delete LRU items

        for (int i=FC_K_LRU_CLK_ML-1; i >=MIN_QUEUE_TO_MOVE_TO_FLASH_ML ; i--)
        {// Find the MRU item
            if (!warmup && AmountOfSVM_1){
                if (kLruAmountOfSVM[i] > 0)
                {
                    bool found=false;
                    for (std::list<uint32_t>::iterator tmpkId= (dram[i]).begin() ; tmpkId != (dram[i]).end();tmpkId++)
                    {
                        FlashCacheLrukClkMachineLearning::Item& tmpItem = allObjects[*tmpkId];
                        //Check if SVMResult is 1
                        if (tmpItem.hasDramItem)
                        {
                            if (tmpItem.DramItemListLocation->SVMResult)
                            {
                                mfuKid = *tmpkId;
                                found = true;
                                AmountOfSVM_1--;
                                kLruAmountOfSVM[i]--;
                                FoundMfu = true;
                                break;
                            }
                        }
                    }
                    if (found){break;}
                }else{FoundMfu=false;}
            }else{
                if (kLruSizes[i] > 0)
                {
                    mfuKid = (dram[i]).front();
                    FoundMfu = true;
                    break;
                }else{FoundMfu=false;}
            }
        }

		if (FoundMfu){

			FlashCacheLrukClkMachineLearning::Item& mfuItem = allObjects[mfuKid];
			size_t qN = mfuItem.queueNumber;
			assert(mfuItem.size > 0);

			if (flashSize + mfuItem.size <= FLASH_SIZE_FC_KLRU_CLK_ML) {
			// If we have enough space in the flash, we will insert the MRU item
			// to the flash

				//--------dramLRU ---------------
                if (mfuItem.dramlruIt != dramLru.end()) {dramLru.erase(mfuItem.dramlruIt);}
				//-------------------------------

				mfuItem.isInDram = false;
				mfuItem.queueNumber=8;
				dram[qN].erase(mfuItem.dramLocation);
				flash.emplace_front(mfuKid);
				mfuItem.dramLocation = ((dram[0]).end());
				dramSize -= mfuItem.size;
				mfuItem.flashIt = flash.begin();
				kLruSizes[qN] -= mfuItem.size;
				flashSize += mfuItem.size;

                if (mfuItem.hasDramItem)
                {
                    dramItemList.erase(mfuItem.DramItemListLocation);
                    mfuItem.hasDramItem = false;
                }

				if (!warmup) {
					stat.writes_flash++;
					stat.flash_bytes_written += mfuItem.size;
				}
			}else {
			// If we don't have space in the flash, we will delete the first item with clock 0
			// and make room for the new item
				ClockFindItemToErase(r);
			}
		}else{
			//If we don't find an MRU item to move to flash we will delete items from dram by LRU
			//--------dramLRU ---------------
			deleteItem(dramLru.back());
			//-------------------------------
		}
	}
	assert(false);	
	return PROC_MISS;
}

void FlashCacheLrukClkMachineLearning::dramAdd(std::vector<uint32_t>& objects,
		size_t sum __attribute__ ((unused)),
		size_t k,
		bool updateWrites __attribute__ ((unused)),
		bool warmup __attribute__ ((unused)),
		bool NewItem) {

	    for (auto elem : objects)
        {
            
				FlashCacheLrukClkMachineLearning::Item& item = allObjects[elem];
            
				dram[k].emplace_front(item.kId);
				item.dramLocation = dram[k].begin();
				item.queueNumber = k;
				dramSize += item.size;
				kLruSizes[k] += item.size;
                if (item.hasDramItem)
                {
                    if (item.DramItemListLocation->SVMResult)
                    {
                        kLruAmountOfSVM[k]++;
                    }
                }
            
            
				if (k != 0){ assert(kLruSizes[k] <= FC_KLRU_QUEUE_SIZE_CLK_ML);}

				if (NewItem)
                { //New Item, need to insert to clocklru and update clock
					std::pair<uint32_t,size_t> it;
					it =  std::make_pair(item.kId, CLOCK_MAX_VALUE_KLRU_ML);

					if (clockLru.size() == 0) {
						clockLru.emplace_front(it);
						clockIt = clockLru.begin();
						item.ClockIt = clockLru.begin();
					} else {
						clockLru.insert(clockIt,it);
						dramIt Clkit = clockIt;
						Clkit--;
						item.ClockIt = Clkit;
					}
                    //--------dramLRU ---------------
					dramLru.emplace_front(item.kId);
                    item.dramlruIt = dramLru.begin();
                    //-------------------------------
                }
		}
}



void FlashCacheLrukClkMachineLearning::dramAddandReorder(std::vector<uint32_t>& objects,
		size_t sum,
		size_t k,
		bool updateWrites,
		bool warmup) {

        assert(k < FC_K_LRU_CLK_ML);

		std::vector<uint32_t> newObjects;

		size_t newSum = 0;

		if (k != 0)
		{
			while (sum + kLruSizes[k] > FC_KLRU_QUEUE_SIZE_CLK_ML)
            {
				assert(kLruSizes[k] > 0);
				assert(dram[k].size() > 0);
                uint32_t elem = dram[k].back();
                FlashCacheLrukClkMachineLearning::Item& item = allObjects[elem];
                
                dram[k].pop_back();
                if (item.hasDramItem)
                {
                    if (item.DramItemListLocation->SVMResult)
                    {
                        kLruAmountOfSVM[k]--;
                    }
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

void FlashCacheLrukClkMachineLearning::deleteItem(uint32_t keyId) {

	auto searchRKId = allObjects.find(keyId);
	assert(searchRKId != allObjects.end());

	FlashCacheLrukClkMachineLearning::Item& item = searchRKId->second;
	if (item.ClockIt == clockIt)
	{
		clockIt++;
		if (clockIt == clockLru.end()) {
			clockIt = clockLru.begin();
		}
	}
	clockLru.erase(item.ClockIt);

	if (item.isInDram) {
        
        //--------dramLRU ---------------
        dramLru.erase(item.dramlruIt);
        //-------------------------------
        
        size_t dGqN = item.queueNumber;
        dram[dGqN].erase(item.dramLocation);
        kLruSizes[dGqN] -= item.size;
        dramSize -= item.size;
        
        if (item.hasDramItem)
        {
            if (item.DramItemListLocation->SVMResult)
            {
                kLruAmountOfSVM[item.queueNumber]--;
                AmountOfSVM_1--;
            }
            dramItemList.erase(item.DramItemListLocation);
            item.hasDramItem = false;
        }

	} else {
		flash.erase(item.flashIt);
		flashSize -= item.size;
	}

	allObjects.erase(keyId);
}

bool FlashCacheLrukClkMachineLearning::inTimesforinsert(const request *r){
	if (r->time > ML_START_TIME && r->time < ML_END_TIME)
		return true;
	return false;
}

bool FlashCacheLrukClkMachineLearning::inTimesforupdate(const request *r){
	if (r->time > ML_START_TIME && r->time < ML_END_TIME + 3600)
		return true;
	return false;
}

void FlashCacheLrukClkMachineLearning::SVMWarmUPCalculation(const request *r,double v1, bool warmup __attribute__ ((unused)))
{
	FlashCacheLrukClkMachineLearning::Item& item = allObjects[r->kid];

	if (inTimesforupdate(r))
	{
        if (item.hasSVMItem)
        {
            FlashCacheLrukClkMachineLearning::SVMItem& svmItem = *item.SVMObjectListLocation;
            
            svmItem.AvgTimeBetweenhits = (svmItem.AvgTimeBetweenhits *(svmItem.AmountsOfHitsTillEviction) + (r->time - svmItem.LastAction))/ (svmItem.AmountsOfHitsTillEviction + 1);
            svmItem.AmountsOfHitsTillEviction++;
            svmItem.TimeBetweenLastAction = r->time - svmItem.LastAction;
            svmItem.AmountOfHitsSinceArrivel++;
            svmItem.LastAction = r->time;
            if (svmItem.TimeBetweenLastAction > svmItem.MaxTimeBetweenHits)
                        	{svmItem.MaxTimeBetweenHits= svmItem.TimeBetweenLastAction;}

			if (v1 < svmItem.probability)
			{
				svmItem.r_TimeBetweenLastAction     = svmItem.TimeBetweenLastAction;
                svmItem.r_AvgTimeBetweenhits        = svmItem.AvgTimeBetweenhits;
                svmItem.r_AmountOfHitsSinceArrivel  = svmItem.AmountOfHitsSinceArrivel - 1;
				svmItem.r_MaxTimeBetweenHits        = svmItem.MaxTimeBetweenHits;
                svmItem.r_AmountsOfHitsTillEviction = -1;
			}
			svmItem.r_AmountsOfHitsTillEviction++;
            svmItem.probability = 1/svmItem.AmountOfHitsSinceArrivel;
            
		}else{
            //If Item doesnt have SVM Item
			FlashCacheLrukClkMachineLearning::SVMItem newSVMItem;
            
            newSVMItem.LastAction = r->time;
            newSVMItem.r_FirstHitTimePeriod         = r->time - item.LastAction;
			newSVMItem.r_MaxTimeBetweenHits         = r->time - item.LastAction;
            newSVMItem.MaxTimeBetweenHits           = r->time - item.LastAction;
            newSVMItem.r_TimeBetweenLastAction      = r->time - item.LastAction;
            newSVMItem.TimeBetweenLastAction        = r->time - item.LastAction;
            newSVMItem.r_AvgTimeBetweenhits         = r->time - item.LastAction;
            newSVMItem.AvgTimeBetweenhits           = r->time - item.LastAction;
            newSVMItem.r_AmountOfHitsSinceArrivel   = 1;
            newSVMItem.AmountOfHitsSinceArrivel     = 1;
            
            SVMCalculationItemList.emplace_front(newSVMItem);
			item.SVMObjectListLocation = SVMCalculationItemList.begin();
			item.hasSVMItem=true;
		}
		
        item.LastAction= r->time;
	}
	//------------------------------------------------------------
}

void FlashCacheLrukClkMachineLearning::ColectItemDataAndPredict(const request *r, bool warmup __attribute__ ((unused)), bool Predict)
{
    FlashCacheLrukClkMachineLearning::Item& item = allObjects[r->kid];
    
    if (item.isInDram)
    {
        FlashCacheLrukClkMachineLearning::DramItem& dramitem = *item.DramItemListLocation;
        
        dramitem.AvgTimeBetweenhits = (dramitem.AvgTimeBetweenhits *(dramitem.AmountOfHitsSinceArrivel) + (r->time - dramitem.LastAction))/ (dramitem.AmountOfHitsSinceArrivel + 1);
        dramitem.TimeBetweenLastAction = r->time - dramitem.LastAction;
        dramitem.AmountOfHitsSinceArrivel++;
        if (dramitem.TimeBetweenLastAction > dramitem.MaxTimeBetweenHits)
            {dramitem.MaxTimeBetweenHits= dramitem.TimeBetweenLastAction;}
        
        if (Predict)
        {
            PyObject* args = PyTuple_Pack(6,PyFloat_FromDouble(dramitem.FirstHitTimePeriod),PyFloat_FromDouble(dramitem.AvgTimeBetweenhits),PyFloat_FromDouble(dramitem.TimeBetweenLastAction),PyFloat_FromDouble(dramitem.MaxTimeBetweenHits),PyFloat_FromDouble(dramitem.AmountOfHitsSinceArrivel),PyInt_FromLong((long)APP_NUMBER));
            PyObject* myResult = PyObject_CallObject(ML_SVMPredictFunction, args);
            
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

void FlashCacheLrukClkMachineLearning::SVMFunctionCalculation()
{
	//run the SVM calculation
	std::string filename{"my_file" + std::to_string(APP_NUMBER) + ".csv"};
	std::ofstream out{filename};
    PyObject* args = NULL;
    PyObject* myResult = NULL;

    int counter=0;
    int ones_counter=0;
    
	//creating an csv file for the python script
	out << "kId,First_Hit_Time_period,Avg_Time_Between_Hits,Time_Between_Last_Hits,Max_Time_between_Hits,Amount_Of_Hits_Till_Arrival,target" << std::endl;
	for (auto SVMitem = SVMCalculationItemList.begin() ; SVMitem != SVMCalculationItemList.end(); SVMitem++)
    {
		int target =0;
        counter++;
		if (SVMitem->r_AmountsOfHitsTillEviction > ML_SVM_TH)
        {
            target=1;
            ones_counter++;
        }
		out << "0," << std::fixed << SVMitem->r_FirstHitTimePeriod << "," << SVMitem->r_AvgTimeBetweenhits << ","
				<< SVMitem->r_TimeBetweenLastAction << "," << SVMitem->r_MaxTimeBetweenHits << "," << SVMitem->r_AmountOfHitsSinceArrivel <<  "," << target  << std::endl;

	}
    printf("amount of 1: %d, of total %d objects\n",ones_counter,counter);
	//After finishing dumping all objects to csv, we can delete the list
	SVMCalculationItemList.clear();

	//Calling the python function to calculate the SVM function
    args = PyTuple_Pack(1,PyInt_FromLong((long)APP_NUMBER));
	PyObject_CallObject(ML_SVMFitFunction,args);

    
	//update every item in allObjects
	for (auto it = dramItemList.begin(); it != dramItemList.end(); it++)
	{
		counter++;

		args = PyTuple_Pack(5,PyFloat_FromDouble(it->FirstHitTimePeriod),PyFloat_FromDouble(it->AvgTimeBetweenhits),PyFloat_FromDouble(it->TimeBetweenLastAction),PyFloat_FromDouble(it->MaxTimeBetweenHits),PyFloat_FromDouble(it->AmountOfHitsSinceArrivel));
		myResult = PyObject_CallObject(ML_SVMPredictFunction, args);

		//Predict the class labele for test data sample
		double tmpResults = PyFloat_AsDouble(myResult);
		it->SVMResult = tmpResults;
		if (it->SVMResult == 1)
        {
            FlashCacheLrukClkMachineLearning::Item& item = allObjects[it->kId];
            
            ones_counter++;
            kLruAmountOfSVM[item.queueNumber]++;
            AmountOfSVM_1++;
        }
        
        Py_XDECREF(args);
        Py_XDECREF(myResult);
	}
    AmountOfSVM_1 = ones_counter;
	printf("amount of 1: %d, of total %d objects\n",ones_counter,counter);
	//close SVM running
	SVMCalculationRun = true;
}

void FlashCacheLrukClkMachineLearning::ClockFindItemToErase(const request *r __attribute__ ((unused)))
{
	bool isDeleted = false;
	dramIt tmpIt, startIt = clockIt;

	while(clockIt != clockLru.end()) {
		assert(clockIt->second <= CLOCK_MAX_VALUE_KLRU_ML);
		if (clockIt->second == 0) {
			// This item need to be delete
			tmpIt = clockIt;
			tmpIt++;
			deleteItem(clockIt->first);
			isDeleted = true;
			break;
		} else {
			clockIt->second--;
		}
		clockIt++;
	}

	if (!isDeleted) {
		// No item was deleted moving to items on the other
		// half of the watch
		clockIt = clockLru.begin();
		assert(clockIt->second <= CLOCK_MAX_VALUE_KLRU_ML);

		while (clockIt != startIt) {
			if (clockIt->second == 0) {
				tmpIt = clockIt;
				tmpIt++;
				deleteItem(clockIt->first);
				isDeleted = true;
				break;
			} else {
				clockIt->second--;
			}
			clockIt++;
		}
	}

	if (!isDeleted) {
		// After a full cycle no item was deleted
		// Delete the item you started with
		assert(clockLru.size() > 0);
		assert(clockIt != clockLru.end());
		tmpIt = clockIt;
		tmpIt++;
		deleteItem(clockIt->first);
	}
	//reseting the clockIt again
	clockIt = (tmpIt == clockLru.end()) ? clockLru.begin() : tmpIt;
}


void FlashCacheLrukClkMachineLearning::dump_stats(void) {

	std::string filename{stat.policy
			+ "-app" + std::to_string(APP_NUMBER)};
	std::ofstream out{filename};

	out << "dram size " << DRAM_SIZE_FC_KLRU_CLK_ML << std::endl;
	out << "flash size " << FLASH_SIZE_FC_KLRU_CLK_ML << std::endl;
	out << "#accesses "  << stat.accesses << std::endl;
	out << "#global hits " << stat.hits << std::endl;
	out << "#dram hits " << stat.hits_dram << std::endl;
	out << "#flash hits " << stat.hits_flash << std::endl;
	out << "hit rate " << double(stat.hits) / stat.accesses << std::endl;
	out << "#writes to flash " << stat.writes_flash << std::endl;
	out << "credit limit " << stat.credit_limit << std::endl; 
	out << "#bytes written to flash " << stat.flash_bytes_written << std::endl;
	out << std::endl << std::endl;

	out << "Amount of lru queues: " << FC_K_LRU_CLK_ML << std::endl;
	for (size_t i =0 ; i < FC_K_LRU_CLK_ML ; i++){
		out << "dram[" << i << "] has " << dram[i].size() << " items" << std::endl;
		out << "dram[" << i << "] size written " << kLruSizes[i] << std::endl;
	}
	out << "Total dram filled size " << dramSize << std::endl;
	//for (auto it = allFlashObjects.begin(); it != allFlashObjects.end(); it++) {
	//	out << it->second.kId << "," << it->second.queueWhenInserted << "," << it->second.hitTimesAfterInserted << "," << it->second.size << "," << it->second.rewrites << std::endl;
	//}
}

