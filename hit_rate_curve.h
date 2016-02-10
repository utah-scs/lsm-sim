#include <cstddef>
#include <numeric>
#include <vector>
#include <cinttypes>
#include <iostream>
#include <exception>
#include <fstream>

#ifndef HIT_RATE_CURVE_H
#define HIT_RATE_CURVE_H

class hit_rate_curve {
 public:
  hit_rate_curve()
    : distances{}
  {
  }

  void hit(size_t distance) {
    if (distances.size() < distance + 1)
      distances.resize(distance + 1, 0);

    ++(distances.at(distance));
  }

  void dump() const {
    for (size_t i = 0; i < distances.size(); ++i)
      std::cout << distances[i] << " ";
  }

  void dump_readable() const {
    for (size_t i = 0; i < distances.size(); ++i)
      std::cout << "Distance " << i << " Count " << distances[i] << std::endl;
  }

  void dump_cdf(const std::string& filename) const {
    std::ofstream out{filename};

    out << "distance cumfrac" << std::endl;
    if (distances.size() == 0)
      return;

    uint64_t total = std::accumulate(distances.begin(), distances.end(), 0);

    uint64_t accum = 0;
    for (size_t i = 0; i < distances.size(); ++i) {
      uint64_t delta = distances[i];
      accum += delta;
      if (delta)
        out << i << " " << float(accum) / total << std::endl;
    }
  }

  void merge(const hit_rate_curve& other) {
    if (distances.size() < other.distances.size())
      distances.resize(other.distances.size());

    for (size_t i = 0; i < other.distances.size(); ++i)
      distances[i] += other.distances[i];
  }

  std::vector<uint64_t> distances;
};

#endif
