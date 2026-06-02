#include <mymath.h>

#include <cstdio>

int main()
{
	int failures = 0;
	if (mymath::add(2, 3) != 5) {
		std::printf("FAIL: add(2, 3) != 5\n");
		++failures;
	}
	if (mymath::mul(2, 3) != 6) {
		std::printf("FAIL: mul(2, 3) != 6\n");
		++failures;
	}
	if (failures == 0)
		std::printf("All assertions passed\n");
	return failures == 0 ? 0 : 1;
}
