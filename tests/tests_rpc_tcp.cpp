#include "testsPCH.h"
#include "Semaphore.h"
#include "Foo.h"

#if 1

#define TEST_PORT 9000

#define LONGTEST 0

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

#if 1

SUITE(RPCTCP)
{

void test_closeTiming(bool accept, int a, bool doclose)
{
	TCPService io;
	auto th = std::thread([&io]
	{
		io.run();
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

// Forward declaration, so the server side can use it
class TesterClient;

class Tester
{
public:

	void simple()
	{
	}

	int noParams()
	{
		return 128;
	}

	int add(int a, int b)
	{
		return a+b;
	}

	virtual const char* virtualFunc()
	{
		return "Tester";
	}

	int testClientAddCall(int a, int b);

	std::future<std::string> testClientVoid();

	void voidTestException(bool doThrow)
	{
		if (doThrow)
			throw std::runtime_error("Testing exception");
	}

	int intTestException(bool doThrow)
	{
		if (doThrow)
			throw std::runtime_error("Testing exception");
		else
			return 128;
	}


	std::vector<int> testVector1(std::vector<int> v)
	{
		return v;
	}

	std::vector<int> testVector2(const std::vector<int>& v)
	{
		return v;
	}

	Semaphore m_throughputSem;
	int testThroughput1(std::string v, int id)
	{
		m_throughputSem.notify();
		return id;
	}
	int testThroughput2(std::vector<char> v, int id)
	{
		m_throughputSem.notify();
		return id;
	}
	std::tuple<int, std::string> testTuple(std::tuple<int, std::string> v)
	{
		return v;
	}

	bool testFoo1(const Foo& f)
	{
		return true;
	}

	bool testFoo2(Foo f)
	{
		return true;
	}

	std::future<std::string> testFuture(const char* str)
	{
		return std::async(std::launch::async, [s=std::string(str)]
		{
			UnitTest::TimeHelpers::SleepMs(100);
			return s;
		});
	}

	Any testAny(Any v)
	{
		return v;
	}



	std::promise<int> clientCallRes; // So the unit test can wait on the future to make sure the server got the reply from the server
};

class TesterEx : public Tester
{
public:
	int leftShift(int v, int bits)
	{
		return v << bits;
	}

	virtual const char* virtualFunc()
	{
		return "TesterEx";
	}
};

class TesterClient
{
public:
	int clientAdd(int a, int b)
	{
		return a + b;
	}
};

// Gather all the RPCs for Tester, but not define the Table right now, since we need to reuse the define
#define RPCTABLE_TESTER_CONTENTS \
	REGISTERRPC(simple) \
	REGISTERRPC(noParams) \
	REGISTERRPC(add) \
	REGISTERRPC(virtualFunc) \
	REGISTERRPC(testClientAddCall) \
	REGISTERRPC(testClientVoid) \
	REGISTERRPC(voidTestException) \
	REGISTERRPC(intTestException) \
	REGISTERRPC(testVector1) \
	REGISTERRPC(testVector2) \
	REGISTERRPC(testThroughput1) \
	REGISTERRPC(testThroughput2) \
	REGISTERRPC(testTuple) \
	REGISTERRPC(testFoo1) \
	REGISTERRPC(testFoo2) \
	REGISTERRPC(testFuture) \
	REGISTERRPC(testAny)

#define RPCTABLE_CLASS Tester
	#define RPCTABLE_CONTENTS RPCTABLE_TESTER_CONTENTS
#include "crazygaze/rpc/RPCGenerate.h"

// Inheritance is done by reusing defines, therefore putting together all the RPCs needed
#define RPCTABLE_CLASS TesterEx
	#define RPCTABLE_CONTENTS \
		RPCTABLE_TESTER_CONTENTS \
		REGISTERRPC(leftShift)
#include "crazygaze/rpc/RPCGenerate.h"

#define RPCTABLE_CLASS TesterClient
#define RPCTABLE_CONTENTS \
	REGISTERRPC(clientAdd)
#include "crazygaze/rpc/RPCGenerate.h"


// Server side calls a RPC on the client...
int Tester::testClientAddCall(int a, int b)
{
	auto client = cz::rpc::Connection<Tester, TesterClient>::getCurrent();
	CHECK(client != nullptr);
	CZRPC_CALL(*client, clientAdd, a, b).async(
		[this, r = a+b](Result<int> res)
	{
		CHECK_EQUAL(r, res.get());
		clientCallRes.set_value(res.get());
	});

	return a + b;
}

// This tests what happens when the server tries to call an RPC on a client which doesn't have a local object.
// In other words, the client's connection is in the form Connection<void,SomeRemoteInterface>
// In this case, the client having a InProcessor<void>, should send back an error for all RPC calls it receives.
// In turn, the server sends back this error as a reply to the original RPC call from the client, so the unit test
// can check if everything is still working and not blocked somewhere.
std::future<std::string> Tester::testClientVoid()
{
	auto client = cz::rpc::Connection<Tester, TesterClient>::getCurrent();
	CHECK(client != nullptr);

	auto pr = std::make_shared<std::promise<std::string>>();
	CZRPC_CALL(*client, clientAdd, 1, 2).async(
		[this, pr](Result<int> res)
	{
		CHECK(res.isException());
		pr->set_value(res.getException());
	});

	return pr->get_future();
}

CZRPC_DEFINE_CONST_LVALUE_REF(std::vector<int>)

// Alternatively, enable support for all "const T&" with
// CZRPC_ALLOW_CONST_LVALUE_REFS;

//
// To simulate a server process, serving one single object instance
//
template<typename LOCAL, typename REMOTE>
class ServerProcess
{
public:
	using Local = LOCAL;
	using Remote = REMOTE;

	explicit ServerProcess(int port, std::string authToken = "")
		: m_objData(&m_obj)
		, m_acceptor(m_io, m_obj)
	{
		m_th = std::thread([this]
		{
			m_io.run();
		});

		m_objData.setAuthToken(std::move(authToken));

		m_acceptor.start(TEST_PORT, [this](std::shared_ptr<Connection<Local, Remote>> con)
		{
			m_cons.push_back(std::move(con));
		});
	}

	~ServerProcess()
	{
		m_io.stop();
		m_th.join();
	}

	LOCAL& obj() { return m_obj;   }
private:
	TCPService m_io;
	std::thread m_th;
	LOCAL m_obj;
	ObjectData m_objData;
	TCPTransportAcceptor<Local, Remote> m_acceptor;
	std::vector<std::shared_ptr<Connection<Local, Remote>>> m_cons;
};

SUITE(RPCTraits)
{

#if 1
TEST(NotAuth)
{
	using namespace cz::rpc;
	ServerProcess<Tester, void> server(TEST_PORT, "meow");

	TCPService io;
	std::thread iothread = std::thread([&io]
	{
		io.run();
	});

	auto clientCon = TCPTransport<void,Tester>::create(io, "127.0.0.1", TEST_PORT).get();

	//
	// Calling an RPC without authenticating first (if authentication is required),
	// will cause the transport to close;
	int asyncAborted = 0;
	// Test with async
	Semaphore sem;
	CZRPC_CALL(*clientCon, simple).async(
		[&](Result<void> res)
	{
		CHECK(res.isAborted());
		sem.notify();
	});
	sem.wait();

	// Test with future
	auto ft = CZRPC_CALL(*clientCon, simple).ft();
	auto ftRes = ft.get();
	CHECK(ftRes.isAborted());

	io.stop();
	iothread.join();
}

// RPC without return value or parameters
TEST(Simple)
{
	using namespace cz::rpc;
	// Also test using authentication
	ServerProcess<Tester, void> server(TEST_PORT, "meow");

	TCPService io;
	std::thread iothread = std::thread([&io]
	{
		io.run();
	});

	auto clientCon = TCPTransport<void,Tester>::create(io, "127.0.0.1", TEST_PORT).get();

	ZeroSemaphore sem; // Used to make sure all rpcs were called
	sem.increment();

	// Authenticate first
	bool authRes = false;
	CZRPC_CALLGENERIC(*clientCon, "__auth", std::vector<Any>{ Any("meow") }).ft().get().get().getAs(authRes);
	CHECK(authRes == true);

	// Test with async
	CZRPC_CALL(*clientCon, simple).async(
		[&](auto)
	{
		sem.decrement();
	});

	// Test with future
	CZRPC_CALL(*clientCon, simple).ft().get().get();

	sem.wait();
	io.stop();
	iothread.join();
}

TEST(NoParams)
{
	using namespace cz::rpc;
	ServerProcess<Tester, void> server(TEST_PORT);

	TCPService io;
	std::thread iothread = std::thread([&io]
	{
		io.run();
	});

	auto clientCon = TCPTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

	ZeroSemaphore sem; // Used to make sure all rpcs were called
	sem.increment();

	// Test with async
	CZRPC_CALL(*clientCon, noParams).async(
		[&](Result<int> res)
	{
		sem.decrement();
		CHECK_EQUAL(128, res.get());
	});

	// Test with future
	int res = CZRPC_CALL(*clientCon, noParams).ft().get().get();
	CHECK_EQUAL(128, res);

	sem.wait();
	io.stop();
	iothread.join();
}

// Test with simple parameters and return value
TEST(WithParams)
{
	using namespace cz::rpc;
	ServerProcess<Tester, void> server(TEST_PORT);

	TCPService io;
	std::thread iothread = std::thread([&io]
	{
		io.run();
	});

	auto clientCon = TCPTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

	ZeroSemaphore sem; // Used to make sure all rpcs were called

	// Test with async
	sem.increment();
	CZRPC_CALL(*clientCon, add, 1,2).async(
		[&](Result<int> res)
	{
		sem.decrement();
		CHECK_EQUAL(3, res.get());
	});

	// Test with future
	int res = CZRPC_CALL(*clientCon, add, 1, 2).ft().get().get();
	CHECK_EQUAL(3, res);

	// Test with vector
	std::vector<int> vec{ 1,2,3 };
	auto v = CZRPC_CALL(*clientCon, testVector1, vec).ft().get().get();
	CHECK_ARRAY_EQUAL(vec, v, 3);
	v = CZRPC_CALL(*clientCon, testVector2, vec).ft().get().get();
	CHECK_ARRAY_EQUAL(vec, v, 3);

	// Test with tuple
	auto tp = std::make_tuple(1, std::string("Test"));
	tp = CZRPC_CALL(*clientCon, testTuple, tp).ft().get().get();
	CHECK(std::get<0>(tp) == 1 && std::get<1>(tp) == "Test");

	sem.wait();
	io.stop();
	iothread.join();
}

TEST(Future)
{
	using namespace cz::rpc;
	ServerProcess<Tester, void> server(TEST_PORT);

	TCPService io;
	std::thread iothread = std::thread([&io]
	{
		io.run();
	});

	auto clientCon = TCPTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

	const int count = 1;
	ZeroSemaphore sem;
	for (int i = 0;i < count; i++)
	{
		sem.increment();
		auto p = std::to_string(i);
		CZRPC_CALL(*clientCon, testFuture, p.c_str()).async(
			[&sem, p](Result<std::string> res)
		{
			CHECK_EQUAL(p, res.get());
			sem.decrement();
		});
	}

	sem.wait();
	io.stop();
	iothread.join();
}

// Test RPCs throwing exceptions
TEST(ExceptionThrowing)
{
	using namespace cz::rpc;
	ServerProcess<Tester, void> server(TEST_PORT);

	TCPService io;
	ZeroSemaphore expectedUnhandledExceptions;
	std::thread iothread = std::thread([&io, &expectedUnhandledExceptions]
	{
		while(true)
		{
			try
			{
				io.run();
				return;
			}
			catch (const Exception&)
			{
				expectedUnhandledExceptions.decrement();
				continue;
			}
			break;
		}
	});

	auto clientCon = TCPTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

	ZeroSemaphore sem; // Used to make sure all rpcs were called

	// Test with async
	expectedUnhandledExceptions.increment();
	CZRPC_CALL(*clientCon, voidTestException,  true).async(
		[&](auto res)
	{
		res.get(); // this will throw an exception, because the RPC returned an exception
	});

	// RPC with exception and the client using a future
	// This will cause the future to get a broken_promise
	expectedUnhandledExceptions.increment();
	bool brokenPromise = false;
	auto ft = CZRPC_CALL(*clientCon, voidTestException, true).ft();
	try
	{
		ft.get().get(); // This will throw, because the RPC returned an exception
	}
	catch (Exception&)
	{
		expectedUnhandledExceptions.decrement();
	}

	sem.wait();
	io.stop();
	iothread.join();
	expectedUnhandledExceptions.wait();
}

// Having the server call a function on the client
TEST(ClientCall)
{
	using namespace cz::rpc;
	ServerProcess<Tester, TesterClient> server(TEST_PORT);
	TCPService io;
	std::thread iothread = std::thread([&io]
	{
		io.run();
	});

	TesterClient clientObj;
	auto clientCon = TCPTransport<TesterClient, Tester>::create(io, clientObj, "127.0.0.1", TEST_PORT).get();

	ZeroSemaphore pending;

	std::promise<int> res1;
	CZRPC_CALL(*clientCon, testClientAddCall, 1,2).async(
		[&](Result<int> res)
	{
		res1.set_value(res.get());
	});

	CHECK_EQUAL(3, res1.get_future().get());
	CHECK_EQUAL(3, server.obj().clientCallRes.get_future().get());

	io.stop();
	iothread.join();
}

// The server is running a specialize TesterEx that overrides some virtuals,
// and the client connects asking for the Tester interface
TEST(Inheritance)
{
	using namespace cz::rpc;

	// The server is running a TesterEx
	ServerProcess<TesterEx, void> server(TEST_PORT);

	TCPService io;
	std::thread iothread = std::thread([&io]
	{
		io.run();
	});

	// The client connects as using Tester
	auto clientCon = TCPTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

	ZeroSemaphore pending;
	pending.increment();
	CZRPC_CALL(*clientCon, virtualFunc).async(
		[&](const Result<std::string>& res)
	{
		pending.decrement();
		CHECK_EQUAL("TesterEx", res.get().c_str());
	});

	pending.wait();

	io.stop();
	iothread.join();
}

TEST(Constructors)
{
	using namespace cz::rpc;

	ServerProcess<Tester, void> server(TEST_PORT);

	TCPService io;
	std::thread iothread = std::thread([&io]
	{
		io.run();
	});

	auto clientCon = TCPTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

	Foo foo(1);
	Foo::resetCounters();
	auto ft = CZRPC_CALL(*clientCon, testFoo1, foo).ft();
	CHECK(ft.get().get() == true);
	Foo::check(1, 0, 0);

	Foo::resetCounters();
	ft = CZRPC_CALL(*clientCon, testFoo2, foo).ft();
	CHECK(ft.get().get() == true);
	Foo::check(1, 1, 0);

	io.stop();
	iothread.join();
}

#define ANY_CHECK(a_, type, str)            \
{ \
	Any& a = a_; \
	CHECK(a.getType() == Any::Type::type); \
	CHECK(std::string(a.toString()) == str); \
}

TEST(Any)
{
	using namespace cz::rpc;

	ServerProcess<Tester, void> server(TEST_PORT);

	TCPService io;
	std::thread iothread = std::thread([&io]
	{
		io.run();
	});

	auto clientCon = TCPTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

	{
		auto res = CZRPC_CALL(*clientCon, testAny, Any()).ft().get();
		ANY_CHECK(res.get(), None, "")
	}
	{
		auto res = CZRPC_CALL(*clientCon, testAny, Any(true)).ft().get();
		ANY_CHECK(res.get(), Bool, "true")
	}
	{
		auto res = CZRPC_CALL(*clientCon, testAny, Any(int(1234))).ft().get();
		ANY_CHECK(res.get(), Integer, "1234")
	}
	{
		auto res = CZRPC_CALL(*clientCon, testAny, Any(int(-1234))).ft().get();
		ANY_CHECK(res.get(), Integer, "-1234")
	}
	{
		auto res = CZRPC_CALL(*clientCon, testAny, Any(unsigned(1234))).ft().get();
		ANY_CHECK(res.get(), UnsignedInteger, "1234")
	}
	{
		auto res = CZRPC_CALL(*clientCon, testAny, Any(float(1234.5))).ft().get();
		ANY_CHECK(res.get(), Float, "1234.5000")
	}
	{
		auto res = CZRPC_CALL(*clientCon, testAny, Any("hello")).ft().get();
		ANY_CHECK(res.get(), String, "hello")
	}
	{
		auto res = CZRPC_CALL(*clientCon, testAny, Any(std::vector<unsigned char>{0,1,2,3})).ft().get();
		ANY_CHECK(res.get(), Blob, "BLOB{4}");
		std::vector<unsigned char> v;
		CHECK(res.get().getAs(v) == true);
		CHECK_ARRAY_EQUAL(v, std::vector<unsigned char>({0, 1, 2, 3}), 4);
	}

	io.stop();
	iothread.join();
}

TEST(Generic)
{
	using namespace cz::rpc;

	ServerProcess<Tester, void> server(TEST_PORT);

	TCPService io;
	std::thread iothread = std::thread([&io]
	{
		io.run();
	});

	// Note that since we only want to use generic RPCs from this client we don't need to know
	// the server type. We can use "GenericServer"
	auto clientCon = TCPTransport<void, GenericServer>::create(io, "127.0.0.1", TEST_PORT).get();

	// Calling a non existent generic function
	{
		auto res = CZRPC_CALLGENERIC(*clientCon, "nonexistent").ft().get();
		CHECK(res.isException());
		CHECK(res.getException() == "Generic RPC not found");
	}
	{
		auto res = CZRPC_CALLGENERIC(*clientCon, "simple", std::vector<Any>{Any(true)}).ft().get();
		CHECK(res.isException());
		CHECK(res.getException() == "Invalid parameters for generic RPC");
	}

	{
		auto res = CZRPC_CALLGENERIC(*clientCon, "simple").ft().get().get();
		CHECK(res.getType() == Any::Type::None);
	}
	{
		auto res = CZRPC_CALLGENERIC(*clientCon, "noParams").ft().get().get();
		CHECK(res.getType() == Any::Type::Integer);
		CHECK(std::string(res.toString()) == "128");
	}
	{
		auto res = CZRPC_CALLGENERIC(*clientCon, "add", std::vector<Any>{Any(1), Any(2)}).ft().get().get();
		CHECK(res.getType() == Any::Type::Integer);
		CHECK(std::string(res.toString()) == "3");
	}

	io.stop();
	iothread.join();
}

// Tests the case when the server wants to call a client side RPC, but the client
// processor is actually InProcessor<void>
TEST(VoidPeer)
{
	using namespace cz::rpc;

	// The server expects the client to have a TesterClient API,
	// but if the client is using InProcessor<void>, it should still get a reply with an error
	ServerProcess<Tester, TesterClient> server(TEST_PORT);

	TCPService io;
	std::thread iothread = std::thread([&io]
	{
		io.run();
	});

	// Instead of having a LOCAL of TesterClient, like the server expects, we
	// use void, to test the behavior
	auto clientCon = TCPTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

	// Call an RPC on the server, that in turn will try to call one on the client-side.
	// Since the client is using InProcessor<void>, it cannot reply. It will just send back
	// an error. The server will in turn send us back that exception for us to check
	auto res = CZRPC_CALL(*clientCon, testClientVoid).ft().get();
	CHECK(res.get() == "Peer doesn't have an object to process RPC calls");

	io.stop();
	iothread.join();
}

TEST(ControlRPCs)
{
	using namespace cz::rpc;

	ServerProcess<Tester, void> server(TEST_PORT);
	{
		// Add some properties to the server
		ObjectData(&server.obj()).setProperty("name", Any("Tester1"));
	}

	TCPService io;
	std::thread iothread = std::thread([&io]
	{
		io.run();
	});

	// Specifying GenericServer as REMOTE type, since we only need to call generic RPCs
	auto clientCon = TCPTransport<void, GenericServer>::create(io, "127.0.0.1", TEST_PORT).get();

	{
		auto res = CZRPC_CALLGENERIC(*clientCon, "__getProperty", std::vector<Any>{Any("prop1")}).ft().get();
		CHECK(res.get().getType() == Any::Type::None);
	}

	{
		auto res = CZRPC_CALLGENERIC(*clientCon, "__setProperty", std::vector<Any>{Any("prop1"), Any(false)}).ft().get();
		CHECK(res.get().getType() == Any::Type::Bool);
		CHECK(std::string(res.get().toString()) == "true");
		res = CZRPC_CALLGENERIC(*clientCon, "__getProperty", std::vector<Any>{Any("prop1")}).ft().get();
		CHECK(res.get().getType() == Any::Type::Bool);
		CHECK(std::string(res.get().toString()) == "false");
	}

	{
		auto res = CZRPC_CALLGENERIC(*clientCon, "__setProperty", std::vector<Any>{Any("prop1"), Any("Hello")}).ft().get();
		CHECK(res.get().getType() == Any::Type::Bool);
		CHECK(std::string(res.get().toString()) == "true");
		res = CZRPC_CALLGENERIC(*clientCon, "__getProperty", std::vector<Any>{Any("prop1")}).ft().get();
		CHECK(res.get().getType() == Any::Type::String);
		CHECK(std::string(res.get().toString()) == "Hello");
	}

	{
		auto res = CZRPC_CALLGENERIC(*clientCon, "__getProperty", std::vector<Any>{Any("name")}).ft().get();
		CHECK(res.get().getType() == Any::Type::String);
		CHECK(std::string(res.get().toString()) == "Tester1");
	}

	io.stop();
	iothread.join();
}


TEST(Latency)
{
	using namespace cz::rpc;

	ServerProcess<Tester, void> server(TEST_PORT);

	TCPService io;
	std::thread iothread = std::thread([&io]
	{
		io.run();
	});

	UnitTest::Timer timer;
	auto clientCon = TCPTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

	const int count = (LONGTEST) ? 200 : 20;
	std::vector<double> times(count, 0.0f);
	for(int i=0; i<count; i++)
	{
		auto start = timer.GetTimeInMs();
		auto res = CZRPC_CALL(*clientCon, noParams).ft().get();
		times[i] = timer.GetTimeInMs() - start;
		CHECK_EQUAL(128, res.get());
	}

	double low = std::numeric_limits<double>::max();
	double high = std::numeric_limits<double>::min();
	double total = 0;
	for (auto&& t : times)
	{
		low = std::min(low, t);
		high = std::max(high, t);
		total += t;
	}
	printf("RPC latency (int func(), %d calls)\n", count);
	printf("        min=%0.4fms\n", low);
	printf("        max=%0.4fms\n", high);
	printf("        avg=%0.4fms\n", total/count);

	io.stop();
	iothread.join();
}

#endif

TEST(Throughput)
{
	using namespace cz::rpc;

	ServerProcess<Tester, void> server(TEST_PORT);

	TCPService io;
	std::thread iothread = std::thread([&io]
	{
		io.run();
	});

	UnitTest::Timer timer;
	auto clientCon = TCPTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

	std::atomic<bool> finish(false);
	double start, end;
	auto test = std::async(std::launch::async,
		[&timer, &finish, &clientCon, &start, &end, &server]
	{
		const int size = 1024 * 1024 / 4;

		//std::vector<char> data(size, 'a');
		std::string data(size, 'a');

		start = timer.GetTimeInMs();
		int id = 0;
		ZeroSemaphore sem;
		uint64_t totalBytes = 0;
		std::atomic<int> flying(0);
		while(!finish)
		{
			sem.increment();
			if (flying.load() > 5)
				server.obj().m_throughputSem.wait();

			++flying;
			CZRPC_CALL(*clientCon, testThroughput1, data, id).async(
				[&sem, &flying, id, &totalBytes, s=data.size()](Result<int> res)
			{
				totalBytes += s;
				CHECK_EQUAL(id, res.get());
				--flying;
				sem.decrement();
			});
			id++;
		}
		sem.wait();
		end = timer.GetTimeInMs();
		return std::make_pair(
			(end - start) / 1000, // seconds
			totalBytes // data sent
		);
	});

	UnitTest::TimeHelpers::SleepMs((LONGTEST) ? 30000 : 4000);
	finish = true;
	auto res = test.get();
	io.stop();
	iothread.join();

	auto seconds = res.first;
	auto mb = (double)res.second/(1000*1000);
	printf("RPC throughput: %0.2f Mbit/s (%0.2f MB/s)\n", (mb*8)/seconds, mb/seconds);
}

}



#endif
