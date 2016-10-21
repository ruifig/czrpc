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

/*
class SingleThreadEnforcerTest
{
public:
	void func1()
	{
		SINGLETHREAD_ENFORCE();
	}

	void func2()
	{
		SINGLETHREAD_ENFORCE();
		Sleep(100);
	}

	void func3()
	{
		Sleep(210);
		SINGLETHREAD_ENFORCE();
	}
private:
	DECLARE_SINGLETHREAD_ENFORCER_STRICT;
};
*/

SUITE(RPCTCP)
{

/*
TEST(SingleThreadEnforcer)
{
	SingleThreadEnforcerTest test;
	auto res1 = std::async(std::launch::async, [&]()
	{
		test.func1();
		test.func2();
	});

	auto res2 = std::async(std::launch::async, [&]()
	{
		test.func3();
	});
	//auto res3 = std::async(std::launch::async, &SingleThreadEnforcerTest::func3, &test);

	res1.get();
	res2.get();
	printf("\n");
}
*/

TEST(1)
{
	while(true)
	{
	printf("-------------------START------------------\n");
	TCPService io;
	auto th = std::thread([&io]
	{
		while (io.tick()) {}
	});

	CalcTest calc;

	TCPTransportAcceptor<CalcTest, void> acceptor(io, calc);
	std::shared_ptr<Connection<CalcTest, void>> serverCon;
	acceptor.start(TEST_PORT, [&serverCon](std::shared_ptr<Connection<CalcTest, void>> con)
	{
		//con->close();
		printf("Accepted\n");
		serverCon = con;
	});

	auto conFt = TCPTransport<void, CalcTest>::create(io, "127.0.0.1", TEST_PORT);
	auto con = conFt.get();
	printf("Connected\n");

	Semaphore sem;
	/*
	CZRPC_CALL(*con, add, 1, 2).async([&sem](Result<int> res)
	{
		CHECK(res.isAborted());
		//printf("%d\n", res.get());
		if (res.isAborted())
			sem.notify();
		else
		{
			assert(false);
		}
	});
	*/
	CZRPC_CALL(*con, add, 1, 2).async([&sem](Result<int> res)
	{
		CHECK_EQUAL(3, res.get());
		sem.notify();
	});

	sem.wait();
	printf("\n");

	serverCon->close();
	con->close();
	//Sleep(10); // #TODO: Removing this, it asserting
	io.stop();
	th.join();
	}
}

}

#endif
