#include "eratosthenes.h"

namespace eratosthenes {

std::vector<uint64_t> compute_primes_up_to(uint64_t n) {
  std::vector<uint64_t> primes;
  std::vector<bool> is_prime(n, true);
  for (int i = 3; i * i < n; i += 2) {
    for (int j = 3; j * i < n; j += 2) {
      is_prime[j * i] = false;
    }
  }
  if (n >= 2) {
    primes.push_back(2);
  }
  for (int i = 3; i < n; i += 2) {
    if (is_prime[i] == true)
      primes.push_back(i);
  }
  return primes;
}

} // namespace eratosthenes