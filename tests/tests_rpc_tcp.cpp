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

TEST(1)
{
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
		con->close();
		printf("Derp\n");
		//serverCon = con;
	});

	auto conFt = TCPTransport<void, CalcTest>::create(io, "127.0.0.1", TEST_PORT);
	auto con = conFt.get();
	printf("Derp\n");

	Semaphore sem;
	CZRPC_CALL(*con, add, 1, 2).async([&sem](Result<int> res)
	{
		printf("%d\n", res.get());
		sem.notify();
	});

	sem.wait();
	printf("\n");

	io.stop();
	th.join();
}

}

#endif
