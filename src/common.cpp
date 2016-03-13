#include <sstream>

#include "common.h"

int csv_tokenize(const std::string &s, string_vec *tokens) {
  std::istringstream ss(s);
  std::string token;
  while (std::getline(ss, token, ',')) {
    tokens->push_back(token);
  }
  return 0;
}
