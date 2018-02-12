#include <iostream>
#include <string>
#include <stdexcept>
#include <sstream>

#include "common.h"
#include "request.h"

#include "openssl/sha.h"

Request::Request(const std::string& s)
  : time{}
  , key_sz{}
  , val_sz{}
  , frag_sz{}
  , kid{}
  , appid{}
  , type{}
  , hit{}
{
  parse(s);
}

// Populate a Request \a r from a single CSV string \a s.  Logs errors on lines
// that cannot be parsed.
void Request::parse(const std::string &s) 
{
  std::string::size_type sz;
  string_vec tokens;
  csv_tokenize(s, &tokens);

  try 
  {
    time   = std::stod(tokens.at(0), &sz);
    appid  = std::stoi(tokens.at(1), &sz);
    type   = req_type(std::stoi(tokens.at(2), &sz));
    key_sz = std::stoi(tokens.at(3), &sz);
    val_sz = std::stoi(tokens.at(4), &sz);
    kid    = std::stoi(tokens.at(5), &sz);
  } 
  catch (const std::out_of_range& e) 
  {
    std::cerr << "! Malformed line couldn't be parsed: " << e.what() << ". "
              << s << std::endl;
  }
}

// debug/check
void Request::dump() const 
{
  std::cerr << "*** Request ***"  << std::endl
            << "time: "           << time    << std::endl
            << "app id: "         << appid   << std::endl
            << "req type: "       << type    << std::endl
            << "key size: "       << key_sz  << std::endl
            << "val size: "       << val_sz  << std::endl
            << "kid: "            << kid     << std::endl
            << "hit:" << (hit == 1 ? "yes" : "no") << std::endl;
}

int32_t Request::size() const 
{
  return key_sz + val_sz;
}

int32_t Request::get_frag() const
{
  return frag_sz;
}

bool Request::operator<(const Request& other)
{
  return time < other.time;
}

size_t Request::hash_key(const size_t modulus) const
{
  union Int_to_unsigned_char 
  {
    uint32_t int_value;                         
    unsigned char char_value[sizeof(uint32_t)]; 
  };

  union Unsigned_char_to_size_t
  {
    unsigned char char_value[SHA_DIGEST_LENGTH];  // 20 bytes.
    size_t   size_t_value;                        // 8 bytes on x86.
  };
  
  Int_to_unsigned_char input;
  input.int_value = this->kid;
  Unsigned_char_to_size_t output;
  SHA_CTX context;
  SHA1_Init(&context);
  SHA1_Update(&context, input.char_value, sizeof(uint32_t));
  SHA1_Final(output.char_value, &context);
  return output.size_t_value % modulus;
}
