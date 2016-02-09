#include <cstddef>
#include <numeric>
#include <vector>
#include <cinttypes>
#include <iostream>
#include <exception>

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

  void dump_cdf(uint64_t prefix) const {
    if (distances.size() == 0)
      return;

    uint64_t total = std::accumulate(distances.begin(), distances.end(), 0);
    const uint64_t limit = 1000000;

    uint64_t accum = 0;
    for (size_t i = 0; i < distances.size(); ++i) {
      accum += distances[i];
      if (i >= limit)
        break;
      std::cout << prefix << " " <<
        i << " " << float(accum) / total << std::endl;
    }
  }

  std::vector<uint64_t> distances;
};

#endif
