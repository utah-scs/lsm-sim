#include <experimental/optional>
#include <cinttypes>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cassert>

#ifndef COMMON_H
#define COMMON_H

template <class T>
using optional = std::experimental::optional<T>;

constexpr auto nullopt = std::experimental::nullopt;

typedef std::vector<std::string> string_vec;

// breaks a CSV string into a vector of tokens
int csv_tokenize(const std::string &s, string_vec *tokens);

template <typename T>
std::string to_string_with_precision(const T a_value, const int n = 2)
{
  std::ostringstream out{};
  out << std::setprecision(n) << a_value;
  return out.str();
}

#endif

