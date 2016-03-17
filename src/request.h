#ifndef REQUEST_H
#define REQUEST_H

#include <cinttypes>
#include <string>

struct request {
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
  
  

  void dump() const;
  void parse(const std::string& s);
  int32_t size() const { return key_sz + val_sz; }
  int32_t get_frag() const { return frag_sz; }

  request(const std::string& s)
    : time{}
    , key_sz{}
    , val_sz{}
    , frag_sz{}
    , kid{}
    , appid{}
    , type{}
    , hit{}
  { parse(s); }

  bool operator<(const request& other) {
    return time < other.time;
  }
};



#endif

