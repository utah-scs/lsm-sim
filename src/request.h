#ifndef Request_H
#define Request_H

#include <cinttypes>
#include <string>

struct Request {

  Request(const std::string& s);
  void parse(const std::string& s);
  void dump() const;
  int32_t size() const;
  int32_t get_frag() const;
  bool operator<(const Request& other);

  // Given a modulus, computes the SHA hash of the key_id 'kid' that belongs to
  // this 'Request' and returns the hash mod the provided modules. This is
  // intended for uniformly distributing requests across buckets, partitions,
  // etc. 
  //
  // @param modulus - number of buckets or partitions to mod the hash by.
  size_t hash_key(const size_t modulus) const;

  enum req_type { GET = 1
                , SET = 2
                , DEL = 3
                , ADD = 4
                , INC = 5
                , STAT = 6
                , OTHER = 7 };

  double    time;
  int32_t   key_sz;
  int32_t   val_sz;
  int32_t   frag_sz;
  uint32_t  kid;
  uint32_t  appid;
  req_type  type;
  uint8_t   hit;
};

#endif
