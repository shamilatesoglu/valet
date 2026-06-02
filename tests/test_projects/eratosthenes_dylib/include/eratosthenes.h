#pragma once

#include <cstdint>
#include <vector>

#ifdef _WIN32
#ifdef ERATOSTHENES_DYLIB_SHARED
#ifdef ERATOSTHENES_DYLIB_EXPORTS
#define ERATOSTHENES_DYLIB_API __declspec(dllexport)
#else
#define ERATOSTHENES_DYLIB_API __declspec(dllimport)
#endif
#else
#define ERATOSTHENES_DYLIB_API
#endif
#else
#define ERATOSTHENES_DYLIB_API
#endif

namespace eratosthenes
{

std::vector<uint64_t> ERATOSTHENES_DYLIB_API compute_primes_up_to(uint64_t n);

}