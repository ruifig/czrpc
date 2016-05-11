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
		printf("Clear\n");
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
	int divide(int a, int b)
	{
		return a / b;
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

CZRPC_ALLOW_CONST_LVALUE_REFS;

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

struct Foo
{
	int val;
	explicit Foo(int val = 0) : val(val)
	{
		printf("%p: Foo::Foo(%d)\n", this, val);
	}
	Foo(Foo&& other) : val(other.val)
	{
		other.val = -1;
		printf("%p: Foo::Foo(&& %p %d)\n", this, &other, val);
	}

	Foo(const Foo& other) : val(other.val)
	{
		printf("%p: Foo::Foo(const& %p %d)\n", this, &other, val);
	}

};

template<>
struct cz::rpc::ParamTraits<Foo> : cz::rpc::DefaultParamTraits<Foo>
{
	template<typename S>
	static void write(S& s, const Foo& v) {
		s << v.val;
	}
	template<typename S>
	static void read(S&s, Foo& v) {
		s >> v.val;
	}
};

CZRPC_DEFINE_CONST_LVALUE_REF(Foo);
CZRPC_DEFINE_NON_CONST_LVALUE_REF(Foo);
CZRPC_DEFINE_RVALUE_REF(Foo);

template<>
struct cz::rpc::ParamTraits<int*>
{
	using store_type = int*;
	static constexpr bool valid = true;

	template<typename S>
	static void write(S& s, int* v) {
		s << (uint64_t)v;
	}
	template<typename S>
	static void read(S&s, store_type& v) {
		uint64_t tmp;
		s >> tmp;
		v = (int*)tmp;
	}
};

SUITE(RPCTraits)
{


struct Bar
{
	std::string name;

	int misc(int , float , const char* , const std::string& , Foo /*f1*/, Foo& f2, const Foo& /*f3*/, Foo&& f4)
	{
		f2.val = -f2.val;
		f4.val = -f4.val;
		return 0;
	}

	void valid1() {}
	void valid2(int) {}
	void valid3(const int&) {}
	int valid4() { return 0; }
	int valid5() { return 0; }
	float* invalid1() { return nullptr; }
	int invalid2(int, float*) { return 0; }
};

#if 0

TEST(1)
{
	using namespace cz;
	using namespace rpc;

	RPCHeader hdr;
	hdr.bits.size = 0x12345789;
	hdr.bits.rpcid = 0xAA;
	hdr.bits.counter = 0x3FFFFF;
	auto k = hdr.key();

	Stream s;
	
	// Write
	int someInt = 123;
	{
		s << &someInt;
		s << 100;
		s << "Hello";
		s << std::string("World!");
		std::vector<std::string> v;
		v.push_back("A");
		v.push_back("BB");
		s << v;
		Foo foo(200);
		s << foo;
	}

	// Read back the same values
	{
		int* someIntPtr;
		s >> someIntPtr;
		CHECK_EQUAL(someInt, *someIntPtr);
		int i;
		s >> i;
		CHECK_EQUAL(100, i);
		std::string str;
		s >> str;
		CHECK_EQUAL("Hello", str);
		s >> str;
		CHECK_EQUAL("World!", str);
		std::vector<std::string> v;
		s >> v;
		CHECK_EQUAL(2, v.size());
		CHECK_EQUAL("A", v[0]);
		CHECK_EQUAL("BB", v[1]);
		Foo foo;
		s >> foo;
		CHECK_EQUAL(200, foo.val);
		CHECK_EQUAL(0, s.readSize());
	}

	printf("\n");
}

TEST(FuncCheck)
{
	using namespace cz;
	using namespace rpc;

	bool p0 = ParamPack<>::valid;
	bool p1 = ParamPack<int>::valid;
	bool p2 = ParamPack<int,float>::valid;
	bool p3 = ParamPack<int,int*>::valid;
	bool p4 = ParamPack<int,double&>::valid;
	bool p5 = ParamPack<int,const double&>::valid;
	CHECK(p0 == true);
	CHECK(p1 == true);
	CHECK(p2 == true);
	CHECK(p3 == true);
	CHECK(p4 == false);
	CHECK(p5 == true);

	CHECK(FunctionTraits<decltype(&Bar::misc)>::valid == true);
	CHECK(FunctionTraits<decltype(&Bar::valid1)>::valid == true);
	CHECK(FunctionTraits<decltype(&Bar::valid2)>::valid == true);
	CHECK(FunctionTraits<decltype(&Bar::valid3)>::valid == true);
	CHECK(FunctionTraits<decltype(&Bar::valid4)>::valid == true);
	CHECK(FunctionTraits<decltype(&Bar::valid5)>::valid == true);

	CHECK(FunctionTraits<decltype(&Bar::invalid1)>::valid == false);
	CHECK(FunctionTraits<decltype(&Bar::invalid2)>::valid == false);

	printf("\n");
}


TEST(2)
{
	using namespace cz;
	using namespace rpc;

	Bar bar;
	bar.name = "Some Bar object";

	Stream s;

	Foo tmp(2);
	const Foo& f2 = tmp;
	serializeMethod<decltype(&Bar::misc)>(s, 1, 2.5f, "A", "B", Foo(1), f2, Foo(3), Foo(4));

	using Tuple = FunctionTraits<decltype(&Bar::misc)>::param_tuple;
	Tuple params;
	s >> params;

	callMethod(bar, &Bar::misc, std::move(params));

}

class TestTransport : public cz::rpc::Transport
{
public:

	void setPeer(TestTransport& peer)
	{
		m_peer = &peer;
	}

	virtual void send(std::vector<char> data)
	{
		m_peer->m_data.push(std::move(data));
		if (m_peer->m_receiveNotification)
			m_peer->m_receiveNotification();
	}

	virtual bool receive(std::vector<char>& dst) override
	{
		if (m_data.size())
		{
			dst = std::move(m_data.front());
			m_data.pop();
			return true;
		}
		else
		{
			dst.clear();
			return false;
		}
	}
private:
	std::queue<std::vector<char>> m_data;
	TestTransport* m_peer = nullptr;
};



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

#endif

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



