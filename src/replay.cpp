#ifndef NOREPLAY

#include <iostream>
#include <cassert>
#include <fstream>

#include "replay.h"

PRNG::PRNG()
  : x{123456789}
  , y{362436069}
  , z{521288629}
{}

PRNG::PRNG(uint64_t seed)
  : x{123456789lu * ~(seed << seed)}
  , y{362436069 * ~(seed << (seed + 1))}
  , z{521288629 * ~(seed << (seed + 2))}
{}

void
PRNG::reseed(uint64_t seed)
{
  new (this) PRNG{seed};
}

uint64_t
PRNG::operator()()
{
  uint64_t t;
  x ^= x << 16;
  x ^= x >> 5;
  x ^= x << 1;

  t = x;
  x = y;
  y = z;

  z = t ^ x ^ y;
  return z;
}

replay::replay(stats stat)
  : policy{stat}
  , random_chars{new char[NCHARS]{}}
  , client{}
  , prng{}
  , get_attempts{}
  , set_attempts{}
  , get_failures{}
  , set_failures{}
{
  for (size_t i = 0; i < NCHARS; ++i)
    random_chars[i] = '!' + (prng() % ('~' - '!' + 1));

  client = memcached_create(NULL);

  memcached_return rc =
    memcached_behavior_set(client, MEMCACHED_BEHAVIOR_NO_BLOCK, 1);
  if (rc != MEMCACHED_SUCCESS) {
    std::cerr << "failed to set non-blocking IO" << std::endl;
    exit(1);
  }

  memcached_server_st* servers =
    memcached_server_list_append(NULL, "127.0.0.1", port, &rc);
  if (servers == NULL) {
    std::cerr << "memcached_server_list_append failed: " << (int)rc
              << std::endl;
    exit(1);
  }
  rc = memcached_server_push(client, servers);
  if (rc != MEMCACHED_SUCCESS) {
    std::cerr << "memcached_server_push failed: " << (int)rc << " "
              <<  memcached_strerror(client, rc) << std::endl;
    exit(1);
  }
}

replay::~replay () {
  memcached_free(client);
  client = nullptr;
}

size_t replay::proc(const request *r, bool warmup) {
  assert(r->size() > 0);
  if (r->size() > int32_t(1024 * 1024)) {
    std::cerr << "Can't process large request of size " << r->size()
              << std::endl;
    return 1;
  }

   if (stat.apps->empty())
    stat.apps->insert(r->appid);
  assert(stat.apps->count(r->appid) == 1);

  if (!warmup)
    ++stat.accesses;

  std::ostringstream str{};
  str << r->appid << "-" << r->kid;
  const std::string key = str.str();

  issue_get(key.c_str(), r->val_sz);
 
  return 1;
}

size_t replay::get_bytes_cached() const {
  return 0;
}

void replay::issue_set(const char* key,
                       size_t value_len)
{
  if (value_len > NCHARS)
    std::cerr << "Value too big: " << value_len << std::endl;
  assert(value_len <= NCHARS);
  const char* value = &random_chars[prng() % (NCHARS - value_len)];
  set_attempts++;
  memcached_return rc =
    memcached_set(client, key, strlen(key), value, value_len,
                  (time_t)0, (uint32_t)0);
  if (rc != MEMCACHED_SUCCESS)
    set_failures++;
}

void replay::issue_get(const char* key,
                       size_t reinsert_value_len)
{
  memcached_return rc;
  uint32_t flags;
  size_t valueLength;

  get_attempts++;

  char* ret = memcached_get(client,
                            key, strlen(key), &valueLength, &flags, &rc);
  if (ret == NULL) {
    get_failures++;

    // should just be a cache miss. handle by adding it to the cache.
    if (rc == MEMCACHED_NOTFOUND) { 
      issue_set(key, reinsert_value_len);
    } else {
      std::cerr << "unexpected get error: " <<  memcached_strerror(client, rc)
                << std::endl;
      exit(1);
    }
  } else {
    if (UPDATE_CHANGED_VALUE_LENGTH && valueLength != reinsert_value_len) {
      get_failures++;
      issue_set(key, reinsert_value_len);
    }
    free(ret);
  }
}

#endif // NOREPLAY
