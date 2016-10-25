#include "testsPCH.h"
#include "Semaphore.h"
#include "Foo.h"

#if 1

#define TEST_PORT 9000

using namespace cz;
using namespace cz::rpc;

class CalcTest
{
public:
	int add(int a, int b)
	{
		return a + b;
	}
	int sub(int a, int b)
	{
		return a - b;
	}
};

// Gather all the RPCs for Tester, but not define the Table right now, since we need to reuse the define
#define RPCTABLE_CLASS CalcTest
#define RPCTABLE_CONTENTS \
	REGISTERRPC(add) \
	REGISTERRPC(sub)
#include "crazygaze/rpc/RPCGenerate.h"

SUITE(RPCTCP)
{

void test_closeTiming(bool accept, int a, bool doclose)
{
	TCPService io;
	auto th = std::thread([&io]
	{
		while (io.tick()) {}
	});

	CalcTest calc;

	TCPTransportAcceptor<CalcTest, void> acceptor(io, calc);
	std::shared_ptr<Connection<CalcTest, void>> serverCon;
	acceptor.start(TEST_PORT, [&serverCon, accept](std::shared_ptr<Connection<CalcTest, void>> con)
	{
		if (accept)
			serverCon = con;
		else
			con->close();
	});

	auto conFt = TCPTransport<void, CalcTest>::create(io, "127.0.0.1", TEST_PORT);
	auto con = conFt.get();
	if (a)
		UnitTest::TimeHelpers::SleepMs(a);

	Semaphore sem;
	CZRPC_CALL(*con, add, 1, 2).async([&sem, accept](Result<int> res)
	{
		if (accept)
		{
			CHECK_EQUAL(3, res.get());
		}
		else
		{
			CHECK(res.isAborted());
		}
		sem.notify();
	});
	sem.wait();

	if (doclose)
	{
		if (accept)
			serverCon->close();
		con->close();
		if (a)
			UnitTest::TimeHelpers::SleepMs(a);
	}
	io.stop();
	th.join();
}

TEST(1)
{
	test_closeTiming(false, 0, false);
	test_closeTiming(false, 0, true);
	test_closeTiming(false, 10, false);
	test_closeTiming(false, 10, true);
	test_closeTiming(true, 0, false);
	test_closeTiming(true, 0, true);
	test_closeTiming(true, 10, false);
	test_closeTiming(true, 10, true);
}

}

#endif
