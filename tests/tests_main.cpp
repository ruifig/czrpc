#include "testsPCH.h"

int main()
{
	auto res = UnitTest::RunAllTests();
	return res == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
