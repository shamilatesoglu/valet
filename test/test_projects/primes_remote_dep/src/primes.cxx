#include "primes.h"

#include <iostream>
#include <string>
#include <cmath>

// External dependency
#include <eratosthenes.h>

namespace test_project::primes {

std::vector<uint64_t> naive_primes_up_to(uint64_t n) {
  std::vector<uint64_t> primes;
  for (int i = 2; i < n; i++)
    for (int j = 2; j * j <= i; j++) {
      if (i % j == 0)
        break;
      else if (j + 1 > std::sqrt(float(i))) {
        primes.push_back(i);
      }
    }

  return primes;
}

std::vector<uint64_t> eratosthenes_primes_up_to(uint64_t n) {
  return eratosthenes::compute_primes_up_to(n);
}

} // namespace test_project::primes