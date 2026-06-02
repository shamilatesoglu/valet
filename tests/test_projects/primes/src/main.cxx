#include <chrono>
#include <iostream>
#include <cstdint>
#include <vector>

#include "primes.h"

int main(int argc, char *args[]) {
  std::cout << "valet test program: primes" << std::endl;

  uint64_t n;
  std::cout << "Find primes up to: ";
  std::cin >> n;

  auto t1 = std::chrono::high_resolution_clock::now().time_since_epoch();
  auto primes_naive = test_project::primes::naive_primes_up_to(n);
  auto t2 = std::chrono::high_resolution_clock::now().time_since_epoch();
  auto primes_eratosthenes = test_project::primes::eratosthenes_primes_up_to(n);
  auto t3 = std::chrono::high_resolution_clock::now().time_since_epoch();

  std::cout << "Naive approach results:" << std::endl;
  for (auto p : primes_naive) {
    std::cout << p << " ";
  }
  std::cout
      << std::endl
      << "Duration (ms): "
      << std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() << std::endl;
  std::cout << "Sieve of eratosthenes:" << std::endl;
  for (auto p : primes_eratosthenes) {
    std::cout << p << " ";
  }
  std::cout
      << std::endl
      << "Duration (ms): "
      << std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count() << std::endl;

  return 0;
}