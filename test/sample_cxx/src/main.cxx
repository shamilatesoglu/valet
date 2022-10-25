#include <iostream>

#include "fizzbuzz.h"

int main(int argc, char* args[])
{
    std::cout << "autob test program: fizzbuzz" << std::endl;
    autob::test::fizzbuzz::print_fizzbuzz(1, 100);
    return 0;
}