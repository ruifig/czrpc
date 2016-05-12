#include "testsPCH.h"


class Fake
{
public:
	void clear()
	{
		printf("Clear\n");
	}
	void voidTestException(bool doThrow)
	{
		if (doThrow)
			throw std::exception("Testing exception");
	}

	int intTestException(bool doThrow)
	{
		if (doThrow)
			throw std::exception("Testing exception");
		else
			return 10;
	}

	int addBase(int a, int b)
	{
		return a+b;
	}
	int multiply(int a, int b)
	{
		return a*b;
	}
};

class CalculatorClient;

class Calculator : public Fake
{
public:

	void clear()
	{
	}

	int add(int a, int b)
	{
		return a+b;
	}
	int multiply(int a, int b)
	{
		return a*b;
	}

	void testClientCall();
};

class AdvancedCalculator : public Calculator
{
public:
	int leftShift(int v, int bits)
	{
		return v << bits;
	}
};

class CalculatorClient
{
public:
	void clientClear()
	{
		printf("clientClear\n");
	}
	int clientAdd(int a, int b)
	{
		printf("clientAdd(%d, %d)\n", a, b);
		return a + b;
	}
};

#define RPCTABLE_CALCULATOR_CONTENTS \
	REGISTERRPC(clear) \
	REGISTERRPC(voidTestException) \
	REGISTERRPC(intTestException) \
	REGISTERRPC(add) \
	REGISTERRPC(multiply) \
	REGISTERRPC(testClientCall)

#define RPCTABLE_CLASS Calculator
	#define RPCTABLE_CONTENTS RPCTABLE_CALCULATOR_CONTENTS
#include "crazygaze/rpc/RPCGenerate.h"

#define RPCTABLE_CLASS AdvancedCalculator
	#define RPCTABLE_CONTENTS \
		RPCTABLE_CALCULATOR_CONTENTS \
		REGISTERRPC(divide)
#include "crazygaze/rpc/RPCGenerate.h"

#define RPCTABLE_CLASS CalculatorClient
#define RPCTABLE_CONTENTS \
	REGISTERRPC(clientClear) \
	REGISTERRPC(clientAdd)
#include "crazygaze/rpc/RPCGenerate.h"


void Calculator::testClientCall()
{
	auto cl = cz::rpc::Connection<Calculator, CalculatorClient>::getCurrent();
	if (!cl)
		return;

	CZRPC_CALL(*cl, clientAdd, 2, 3).async(
		[](int res)
	{
		printf("ClientAdd=%d\n", res);
	});
}





SUITE(RPCTraits)
{

/*
TEST(RPC1)
{
	using namespace cz;
	using namespace rpc;

	std::queue<BaseConnection*> rpcQueue;

	auto clientTrp = std::make_shared<TestTransport>();
	auto serverTrp = std::make_shared<TestTransport>();
	clientTrp->setPeer(*serverTrp);
	serverTrp->setPeer(*clientTrp);

	Connection<void, Calculator> client(nullptr, clientTrp);
	Calculator calc;
	Connection<Calculator, void> server(&calc, serverTrp);
	clientTrp->setReceiveNotification([&]()
	{
		rpcQueue.push(&client);
	});
	serverTrp->setReceiveNotification([&]()
	{
		rpcQueue.push(&server);
	});

	CZRPC_CALL(client, clear)
		.async([]()
	{
		printf("Clear\n");
	});

	CZRPC_CALL(client, add, 1, 2)
		.async([](auto res)
	{
		CHECK(res == 3);
		printf("Res = %d\n", res);
	});

	CZRPC_CALL(client, add, 1, 2)
		.asyncEx([](auto res)
	{
		CHECK(*res == 3);
		printf("Res = %d\n", *res);
	});

	CZRPC_CALL(client, voidTestException, false)
		.asyncEx([](Expected<void> res)
	{
		CHECK(res.valid());
		printf("voidTestException 1\n");
	});

	CZRPC_CALL(client, voidTestException, true)
		.asyncEx([](Expected<void> res)
	{
		CHECK(!res.valid());
		printf("voidTestException 2\n");
	});

	CZRPC_CALL(client, intTestException, false)
		.asyncEx([](Expected<int> res)
	{
		CHECK(res);
		printf("intTestException 1\n");
	});

	CZRPC_CALL(client, intTestException, true)
		.asyncEx([](Expected<int> res)
	{
		CHECK(!res);
		printf("intTestException 2\n");
	});

	CZRPC_CALL(client, intTestException, false)
		.async([](int res)
	{
		CHECK(res == 10);
		printf("intTestException 3\n");
	});

	CZRPC_CALL(client, intTestException, true)
		.async([](int res)
	{
		CHECK(res == 10);
		printf("intTestException 4\n");
	});

	auto ft1 = CZRPC_CALL(client, clear).ft();
	auto ft2 = CZRPC_CALL(client, multiply, 3, 2).ft();

	while (rpcQueue.size())
	{
		rpcQueue.front()->process();
		rpcQueue.pop();
	}

	ft1.get();
	printf("Res multiply=%d\n", ft2.get());
	printf("\n");
}

TEST(RPC2)
{
	using namespace cz;
	using namespace rpc;

	auto clientTrp = std::make_shared<TestTransport>();
	auto serverTrp = std::make_shared<TestTransport>();
	clientTrp->setPeer(*serverTrp);
	serverTrp->setPeer(*clientTrp);

	CalculatorClient calcClient;
	Connection<CalculatorClient, Calculator> client(&calcClient, clientTrp);
	Calculator calc;
	Connection<Calculator, CalculatorClient> server(&calc, serverTrp);


	CZRPC_CALL(client, add, 1, 2).async(
		[](int res)
	{
		printf("Add=%d\n", res);
	});

	CZRPC_CALL(client, testClientCall).async(
		[]()
	{
		printf("testClientCall reply received\n");
	});

	server.process();
	client.process();
	server.process();

	printf("\n");

}
*/

TEST(AsioTest)
{
	using namespace cz::rpc;

	ASIO::io_service io;
	std::thread iothread = std::thread([&io]
	{
		ASIO::io_service::work w(io);
		io.run();
	});

	Calculator calc;
	auto acceptor = std::make_shared<AsioTransportAcceptor<Calculator, void>>(io, calc);

	std::shared_ptr<Connection<Calculator, void>> serverCon;
	acceptor->start(9000, [&](std::shared_ptr<Connection<Calculator,void>> con)
	{
		serverCon = std::move(con);
	});

	auto clientCon = AsioTransport::create<void, Calculator>(io, "127.0.0.1", 9000).get();

	auto ft1 = CZRPC_CALL(*clientCon, add, 1, 2).ft();
	auto ft2 = CZRPC_CALL(*clientCon, add, 2, 2).ft();
	auto ft3 = CZRPC_CALL(*clientCon, add, 3, 2).ft();

	printf("Res = %d, %d, %d\n", ft1.get(), ft2.get(), ft3.get());

	io.stop();
	iothread.join();
}

TEST(AsioTest2)
{
	using namespace cz::rpc;

	ASIO::io_service io;
	std::thread iothread = std::thread([&io]
	{
		ASIO::io_service::work w(io);
		io.run();
	});

	Calculator calc;
	CalculatorClient calcClient;
	auto acceptor = std::make_shared<AsioTransportAcceptor<Calculator, CalculatorClient>>(io, calc);

	std::shared_ptr<Connection<Calculator, CalculatorClient>> serverCon;
	acceptor->start(9000, [&](std::shared_ptr<Connection<Calculator,CalculatorClient>> con)
	{
		serverCon = std::move(con);
	});

	auto clientCon = AsioTransport::create<CalculatorClient, Calculator>(io, calcClient, "127.0.0.1", 9000).get();

	auto ft1 = CZRPC_CALL(*clientCon, add, 1, 2).ft();
	auto ft2 = CZRPC_CALL(*clientCon, add, 2, 2).ft();
	auto ft3 = CZRPC_CALL(*clientCon, add, 3, 2).ft();
	auto ft4 = CZRPC_CALL(*clientCon, testClientCall).ft();

	printf("Res = %d, %d, %d\n", ft1.get(), ft2.get(), ft3.get());

	io.stop();
	iothread.join();
}

TEST(AsioTest3)
{
	using namespace cz::rpc;

	ASIO::io_service io;
	std::thread iothread = std::thread([&io]
	{
		ASIO::io_service::work w(io);
		io.run();
	});

	AdvancedCalculator calc;
	CalculatorClient calcClient;
	auto acceptor = std::make_shared<AsioTransportAcceptor<AdvancedCalculator, CalculatorClient>>(io, calc);

	std::shared_ptr<Connection<AdvancedCalculator, CalculatorClient>> serverCon;
	acceptor->start(9000, [&](std::shared_ptr<Connection<AdvancedCalculator,CalculatorClient>> con)
	{
		serverCon = std::move(con);
	});

	auto clientCon = AsioTransport::create<CalculatorClient, AdvancedCalculator>(io, calcClient, "127.0.0.1", 9000).get();

	auto ft1 = CZRPC_CALL(*clientCon, add, 1, 2).ft();
	auto ft2 = CZRPC_CALL(*clientCon, add, 2, 2).ft();
	auto ft3 = CZRPC_CALL(*clientCon, divide, 9, 3).ft();
	auto ft4 = CZRPC_CALL(*clientCon, testClientCall).ft();

	printf("Res = %d, %d, %d\n", ft1.get(), ft2.get(), ft3.get());

	io.stop();
	iothread.join();
}

}



