#include "fizzbuzz.h"

#include <string>
#include <iostream>

namespace autob::test::fizzbuzz {

std::string next(int n) {
  if (n % 3 == 0) {
    return "fizz";
  } else if (n % 5 == 0) {
    return "buzz";
  }
  return std::to_string(n);
}

void print_fizzbuzz(int start, int end){
    for (int i = start; i < end; ++i)
    {
        std::cout << next(i) << " ";
    }
    std::cout << std::endl;
}

} // namespace autob::test::fizzbuzz