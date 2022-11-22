#include <iostream>

#include <add5.h>

int main()
{

    std::cout << "autob test program: deep_deps" << std::endl << "Add 5 to: ";
    int n;
    std::cin >> n;

    std::cout << "Result: " << add5(n) << std::endl;

    return 0;
}