#include <iostream>

#include <add2.h>
#include <add3.h>

int main()
{

    std::cout << "autob test program: deep_deps" << std::endl << "Add 5 to: ";
    int n;
    std::cin >> n;

    std::cout << "Result: " << add3(add2(n)) << std::endl;

    return 0;
}