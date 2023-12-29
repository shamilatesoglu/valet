#include <iostream>

#include <is_even/is_even.hpp>

int main()
{
	while (true) {
		std::cout << "Enter a number: ";
		int number = 0;
		std::cin >> number;
		std::cout << (is_even(number) ? "Even" : "Odd") << std::endl;
	}
	return 0;
}