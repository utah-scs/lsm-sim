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
