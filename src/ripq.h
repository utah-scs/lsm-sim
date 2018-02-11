#include <unordered_map>
#include <list>
#include <memory>
#include <boost/enable_shared_from_this.hpp>

#include "policy.h"
#include "common.h"

#ifndef RIPQ_H
#define RIPQ_H

static int global_bid; //Used for debugging

class ripq : public Policy {
    protected:
    class item; 
    class block;
    class section;

    typedef std::shared_ptr<block> block_ptr;
    typedef std::shared_ptr<item> item_ptr;
    typedef std::shared_ptr<section> section_ptr;

    typedef std::list<block_ptr> block_list;
    typedef std::list<item_ptr> item_list;
    typedef std::vector<section_ptr> section_vector;

    typedef std::unordered_map<uint32_t, item_ptr> hash_map;

    section_vector sections;
    size_t num_sections;
    size_t section_size;
    bool warmup;
    hash_map map;

    class item : public std::enable_shared_from_this<item> {
      public:
        item(const Request req, block_ptr blk)
          : req(req)
          , physical_block(blk) 
          , virtual_block(blk)
          , is_ghost(false)
          , in_dram(false)
        {}

        ~item() {
          physical_block = NULL;
          virtual_block = NULL;
        }

        item& operator=(const item &obj) {
          req = obj.req;
          physical_block = obj.physical_block;
          virtual_block = obj.virtual_block;
          is_ghost = obj.is_ghost;
          in_dram = obj.in_dram;
          return *shared_from_this();
        }

        item(const item &obj) : std::enable_shared_from_this<item>() 
          , req(obj.req)
          , physical_block(obj.physical_block)
          , virtual_block(obj.virtual_block)
          , is_ghost(obj.is_ghost)
          , in_dram(obj.in_dram)
          {}

        Request req;
        block_ptr physical_block;
        block_ptr virtual_block;
        bool is_ghost;
        bool in_dram;
    };

    class block : public std::enable_shared_from_this<block> {
      public:
        block(section_ptr s, bool is_virtual)
          : s(s)
          , items{}
          , filled_bytes(0)
          , is_virtual(is_virtual) 
          , active(true)
          , num_items(0)
          , id(global_bid)
          {global_bid++;}

        block& operator=(const block &obj) {
          s = obj.s;
          items = obj.items;
          filled_bytes = obj.filled_bytes;
          active = obj.active;
          num_items = obj.num_items;
          id = obj.id;
          return *shared_from_this();
        }

        block(const block &obj) : std::enable_shared_from_this<block>()
          , s(obj.s)
          , items(obj.items)
          , filled_bytes(obj.filled_bytes)
          , is_virtual(obj.is_virtual)
          , active(obj.active)
          , num_items(obj.num_items)
          , id(obj.id)
          {}

        ~block() {
          s = NULL;
        }

        section_ptr s;

        item_list items;
        size_t filled_bytes;
        bool const is_virtual;
        bool active;
        int num_items;
        int id; //Used for debugging

        item_ptr add(const Request *req);
        void remove(item_ptr victim);
    };

    class section : public std::enable_shared_from_this<section> {
      public:
        block_list blocks;
        block_ptr active_phy_block;
        block_ptr active_vir_block;
        int id;
        size_t filled_bytes;
        const stats stat;

        section(int id, stats &stat)
          : blocks{}
          , active_phy_block{}
          , active_vir_block{}
          , id(id)
          , filled_bytes(0)
          , stat(stat)
        {
        }

        int count_filled_bytes();

        section& operator=(const section &obj) {
          blocks = obj.blocks;
          active_phy_block = obj.active_phy_block;
          active_vir_block = obj.active_vir_block;
          id = obj.id;
          filled_bytes = obj.filled_bytes;

          return *shared_from_this();
        }
 
        section(const section &obj): std::enable_shared_from_this<section>()
          , blocks(obj.blocks)
          , active_phy_block(obj.active_phy_block)
          , active_vir_block(obj.active_vir_block)
          , id(obj.id)
          , filled_bytes(obj.filled_bytes)
          , stat(obj.stat)
        {}


        void seal_phy_block();

        item_ptr add(const Request *req);
        void seal_vir_block();
        void add_virtual(item_ptr it);
        void add_block(block_ptr new_block);
        block_ptr evict_block();
    };

  private:
    std::ofstream out;
  
  public:
    ripq(stats stat, size_t block_size, size_t dram_size, size_t flash_size);
    virtual ~ripq();

    virtual size_t process_request(const Request *r, bool warmup);
    void add(const Request *r, int section_id);
    void add_virtual(item_ptr it, int section_id);
  
    // Accessors.
    size_t get_bytes_cached() const;
    size_t get_hits(); 
    size_t get_accs();

    virtual void evict();
    void balance(int start=0);
    void dump_stats(void);
};

#endif
