#include "testsPCH.h"
//#include "crazygaze/rpc/RPCTCPSocket.h"

//
// Entry points to try samples used in the documentation
void RunDocTest_ASmallTaste();
void RunDocTest_ParamTraits();

int main()
{
	//RunDocTest_ParamTraits();
#if 1
	int res;
	int counter = 0;
	while (true)
	{
		counter++;
		printf("Run %d\n", counter);
		res = UnitTest::RunAllTests();
		//return res;
		if (res != 0)
			break;
	}
#else
	auto res = UnitTest::RunAllTests();
#endif

	while(true) {}
	return res == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
