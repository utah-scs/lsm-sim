#ifndef COMMON_H
#define COMMON_H

#include <cinttypes>
#include <string>
#include <vector>

typedef std::vector<std::string> string_vec;

// breaks a CSV string into a vector of tokens
int csv_tokenize(const std::string &s, string_vec *tokens);

#endif

