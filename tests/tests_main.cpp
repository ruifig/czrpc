#include "testsPCH.h"

#define LOOP_TESTS 0

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
#if defined(_WIN32) && !defined(NDEBUG) && ENABLE_MEM_DEBUG
	_CrtSetDbgFlag(
		_CRTDBG_ALLOC_MEM_DF
		//| _CRTDBG_DELAY_FREE_MEM_DF
		//| _CRTDBG_CHECK_ALWAYS_DF
		| _CRTDBG_CHECK_EVERY_128_DF
	);
#endif

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

	//while(true) {}
	return res == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
