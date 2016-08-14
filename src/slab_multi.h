#ifndef SLABMULTI_H
#define SLABMULTI_H

#include "policy.h"
#include "lru.h"

#include <vector>
#include <memory>
#include <unordered_map>

class slab_multi : public policy {
  public:
    static constexpr uint8_t  MIN_CHUNK=48;       // Memcached min chunk bytes
    static constexpr double   DEF_GFACT=1.25;     // Memcached growth factor
    static constexpr uint32_t PAGE_SIZE=1000000;  // Memcached page bytes
    static constexpr uint16_t MAX_CLASSES=256;    // Memcached max no of slabs   
    static constexpr size_t   MAX_SIZE=1024 * 1024;   // Largest KV pair allowed 

    class application {
      public:
        application(size_t appid, size_t min_mem, size_t target_mem);
        ~application();

        size_t bytes_limit() const {
          return target_mem + credit_bytes;
        }

        bool try_steal_from(application& other, size_t bytes) {
          if (other.bytes_limit() < bytes)
            return false;

          if (&other == this)
            return false;

          const size_t would_become = other.bytes_limit() - bytes;
          if (would_become < min_mem)
            return false;

          other.credit_bytes -= bytes;
          credit_bytes += bytes;

          return true;
        }

        static void dump_stats_header() {
          std::cout << "time "
                    << "app "
                    << "subpolicy "
                    << "target_mem "
                    << "credit_bytes "
                    << "share "
                    << "min_mem "
                    << "min_mem_pct "
                    << "steal_size "
                    << "bytes_in_use "
                    << "need "
                    << "hits "
                    << "accesses "
                    << "shadow_q_hits "
                    << "survivor_items "
                    << "survivor_bytes "
                    << "evicted_items "
                    << "evicted_bytes "
                    << "hit_rate "
                    << std::endl;
        }

        void dump_stats(double time) {
          std::cout << int64_t(time) << " "
                    << appid << " "
                    << "multislab "
                    << target_mem << " "
                    << credit_bytes << " "
                    << target_mem + credit_bytes << " "
                    << min_mem << " "
                    << min_mem_pct << " "
                    << 0 << " "
                    << bytes_in_use << " "
                    << need() << " "
                    << hits << " "
                    << accesses << " "
                    << shadow_q_hits << " "
                    << survivor_items << " "
                    << survivor_bytes << " "
                    << evicted_items << " "
                    << evicted_bytes << " "
                    << double(hits) / accesses << " "
                    << std::endl;
        }

        double need() {
          return double(target_mem + credit_bytes) / bytes_in_use;
        }

        const size_t appid;
        const size_t min_mem_pct;
        const size_t target_mem;
        const size_t min_mem;

        ssize_t credit_bytes;

        size_t bytes_in_use;

        size_t accesses;
        size_t hits;
        size_t shadow_q_hits;
        size_t survivor_items;
        size_t survivor_bytes;
        size_t evicted_items;
        size_t evicted_bytes;
    };

    slab_multi(stats stat);
   ~slab_multi();

    void add_app(size_t appid, size_t min_mem_pct, size_t target_memory);
    void dump_app_stats(double time);

    size_t proc(const request *r, bool warmup); 
    size_t get_bytes_cached() const;


  private:
    std::pair<uint64_t, uint64_t> get_slab_class(uint32_t size);

    static constexpr size_t SLABSIZE = 1024 * 1024;

    double last_dump;
    std::unordered_map<size_t, application> apps;

    std::vector<lru> slabs; 

    // Simple mapping of existing keys to their respective slab.
    std::unordered_map<uint32_t, uint32_t> slab_for_key;

    uint32_t slab_count;

    uint64_t mem_in_use;

};

#endif
