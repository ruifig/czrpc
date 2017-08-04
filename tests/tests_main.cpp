#include "testsPCH.h"
#include "tests_rpc_spas_helper.h"

#define LOOP_TESTS 0

#if CZRPC_LOGGING
cz::Logger g_rpcLogger("czrpc.log");
#endif

UnitTest::Timer gTimer;

namespace UnitTest
{
	class czrpcTestReporter : public TestReporter
	{
		virtual void ReportFailure(TestDetails const& details, char const* failure) override
		{
			using namespace std;
#if defined(__APPLE__) || defined(__GNUG__)
			char const* const errorFormat = "%s:%d:%d: error: Failure in %s: %s\n";
			fprintf(stderr, errorFormat, details.filename, details.lineNumber, 1, details.testName, failure);
#else
			char const* const errorFormat = "%s(%d): error: Failure in %s: %s\n";
			fprintf(stderr, errorFormat, details.filename, details.lineNumber, details.testName, failure);
#endif
			CZSPAS_DEBUG_BREAK();
		}

		virtual void ReportTestStart(TestDetails const& test) override
		{
			printf("TEST START: %s\n", test.testName);
			gSessionLeakDetector.clear();
		}

		virtual void ReportTestFinish(TestDetails const& test, float) override
		{
			CHECK_EQUAL(0, gSessionLeakDetector.count());
			printf("TEST FINISH: %s\n", test.testName);
		}

		virtual void ReportSummary(int totalTestCount, int failedTestCount, int failureCount, float secondsElapsed) override
		{
			using namespace std;

			if (failureCount > 0)
				printf("FAILURE: %d out of %d tests failed (%d failures).\n", failedTestCount, totalTestCount, failureCount);
			else
				printf("Success: %d tests passed.\n", totalTestCount);

			printf("Test time: %.2f seconds.\n", secondsElapsed);
		}
	};

	int czrpcRunAllTests()
	{
		czrpcTestReporter reporter;
		TestRunner runner(reporter);
		return runner.RunTestsIf(Test::GetTestList(), NULL, True(), 0);
	}
}
namespace cz
{
	namespace rpc
	{
		bool MyTCPLog::ms_assertOnFatal = true;
		bool MyTCPLog::ms_logEnabled = true;
	}
}

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

#if LOOP_TESTS
	int res;
	int counter = 0;
	while (true)
	{
		counter++;
		printf("Run %d\n", counter);
		res = UnitTest::czrpcRunAllTests();
		//return res;
		if (res != 0)
			break;
	}
#else
	auto res = UnitTest::czrpcRunAllTests();
#endif

	//while(true) {}
	return res == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
