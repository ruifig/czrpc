#include "testsPCH.h"
#include "crazygaze/rpc/RPCTCPSocket.h"

//
// Entry points to try samples used in the documentation
void RunDocTest_ASmallTaste();
void RunDocTest_ParamTraits();

int main()
{
	//RunDocTest_ParamTraits();
#if 1
	int res;
	while (true)
	{
		res = UnitTest::RunAllTests();
		if (res != 0)
			break;
	}
#else
	auto res = UnitTest::RunAllTests();
#endif

	return res == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
