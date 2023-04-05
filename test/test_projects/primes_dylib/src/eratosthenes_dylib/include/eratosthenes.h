#pragma once

#include <cstdint>
#include <vector>

namespace eratosthenes {

std::vector<uint64_t> __declspec(dllexport) compute_primes_up_to(uint64_t n);

}