#include "testsPCH.h"

#define LOOP_TESTS 1

#if CZRPC_LOGGING
cz::Logger g_rpcLogger("czrpc.log");
#endif

namespace cz
{
	namespace rpc
	{
		bool MyTCPLog::ms_assertOnFatal = true;
		bool MyTCPLog::ms_logEnabled = true;
	}
}

//
// Entry points to try samples used in the documentation
void RunDocTest_ASmallTaste();
void RunDocTest_ParamTraits();

int main()
{
	//RunDocTest_ParamTraits();
#if LOOP_TESTS
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
