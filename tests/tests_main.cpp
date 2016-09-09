#include "testsPCH.h"

//
// Entry points to try samples used in the documentation
void RunDocTest_ASmallTaste();
void RunDocTest_ParamTraits();

int main()
{
	//RunDocTest_ParamTraits();
	int res;
	while (true)
	{
		res = UnitTest::RunAllTests();
		if (res != 0)
			break;
	}
	return res == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
