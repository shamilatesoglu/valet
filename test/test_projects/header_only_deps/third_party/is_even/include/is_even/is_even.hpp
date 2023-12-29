#pragma once

#include <type_traits>

template <typename T>
requires std::is_integral_v<T>
bool is_even(T number)
{
	return (number & 1) == 0;
}