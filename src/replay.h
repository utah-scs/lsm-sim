#ifndef NOREPLAY
#ifndef REPLAY_H
#define REPLAY_H

#include <memory>

#include <libmemcached/memcached.h>

#include "policy.h"

class PRNG {
 public:
  PRNG();
  PRNG(uint64_t seed);
  void reseed(uint64_t seed);
  uint64_t operator()();

 private:
  uint64_t x, y, z;
};

constexpr bool UPDATE_CHANGED_VALUE_LENGTH = true;

class replay : public policy {
  public:
    replay(stats stat);
   ~replay();
    size_t proc(const request *r, bool warmup); 
    size_t get_bytes_cached() const;

  private:
    void issue_set(const char* key, size_t value_len);
    void issue_get(const char* key, size_t reinsert_value_len);

    static constexpr int port = 12000;

    static constexpr size_t NCHARS = 1u << 20;
    std::unique_ptr<char[]> random_chars;

    memcached_st* client;
    PRNG prng;

    size_t get_attempts;
    size_t set_attempts;

    size_t get_failures;
    size_t set_failures;

    replay(const replay&) = delete;
    replay(replay&&) = delete;
    replay& operator=(const replay&) = delete;
    replay& operator=(replay&&) = delete;
};

#endif
#endif // NOREPLAY
