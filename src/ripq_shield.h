#include <unordered_map>
#include <list>
#include <memory>
#include <boost/enable_shared_from_this.hpp>

#include "policy.h"
#include "common.h"
#include "ripq.h"

#ifndef RIPQ_SHIELD_H
#define RIPQ_SHIELD_H

//static int global_bid;

class ripq_shield : public ripq {

    protected:
    class dram_section;
    class item;

    typedef std::shared_ptr<item> item_ptr;
    typedef std::shared_ptr<dram_section> dsection_ptr;

    typedef std::vector<dsection_ptr> dsection_vector;
    typedef std::unordered_map<uint32_t, item_ptr> hash_map;


    dsection_vector dsections;
    size_t num_dsections;
    size_t dsection_size;
    hash_map dram_map;

    class item : public ripq::item {
      public:
        item(const Request req, dsection_ptr ds)
          : ripq::item(req,NULL)
//          , in_dram(true)
          , ds(ds)
          , dram_it{}
        {in_dram = true;}
  
//      bool in_dram;
      dsection_ptr ds; 
      std::list<item_ptr>::iterator dram_it; 
    };
 
    class dram_section : public std::enable_shared_from_this<dram_section> {
      public:
      std::list<item_ptr> items;
      size_t filled_bytes;
      int num_items;
      int id; //Used for debugging

        dram_section(int id)
          : items{}
          , filled_bytes{}
          , num_items{}
          , id(id)
        {
        }

//        item_ptr add(const Request *req);
        const Request* evict();
        item_ptr add(const Request *req);
        void remove(item_ptr curr_item);
    };
      
  public:
    ripq_shield(stats stat,size_t block_size,size_t num_sections, size_t dram_size, int num_dsections, size_t flash_size);
    virtual ~ripq_shield();

    // Modifiers.
    size_t proc (const Request *r, bool warmup);
    void dram_add(const Request *r, int section_id);
    void dram_evict();    
    
//    void add(const Request *r, int section_id);
//    void add_virtual(item_ptr it, int section_id);
  
    // Accessors.
//  size_t get_bytes_cached() const;
//  size_t get_hits(); 
//  size_t get_accs();

//    void dump_stats();
    virtual void evict();
    void dbalance(int start=0);
//protected:
//    hash_map map; 
};

#endif
