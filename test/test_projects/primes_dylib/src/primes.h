#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace test_project::primes {

std::vector<uint64_t> naive_primes_up_to(uint64_t n);
std::vector<uint64_t> eratosthenes_primes_up_to(uint64_t n);

}