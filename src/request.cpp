#include <iostream>
#include <string>
#include <stdexcept>
#include <sstream>

#include "common.h"
#include "request.h"

/**
 * Populate a request \a r from a single CSV string \a s.
 * Logs errors on lines that cannot be parsed.
 */
void request::parse(const std::string &s) {
  std::string::size_type sz;
  string_vec tokens;
  csv_tokenize(s, &tokens);

  try {
    time   = std::stod(tokens.at(0), &sz);
    appid  = std::stoi(tokens.at(1), &sz);
    type   = req_type(std::stoi(tokens.at(2), &sz));
    key_sz = std::stoi(tokens.at(3), &sz);
    val_sz = std::stoi(tokens.at(4), &sz);
    kid    = std::stoi(tokens.at(5), &sz);
    // r->hit    = stoi(tokens.at(6), &sz) == 1 ? true : false;
  } catch (const std::out_of_range& e) {
    std::cerr << "! Malformed line couldn't be parsed: " << e.what() << ". "
              << s << std::endl;
  }
}

// debug/check
void request::dump() const {
  std::cerr << "*** request ***"  << std::endl
            << "time: "           << time    << std::endl
            << "app id: "         << appid   << std::endl
            << "req type: "       << type    << std::endl
            << "key size: "       << key_sz  << std::endl
            << "val size: "       << val_sz  << std::endl
            << "kid: "            << kid     << std::endl
            << "hit:" << (hit == 1 ? "yes" : "no") << std::endl;
}


