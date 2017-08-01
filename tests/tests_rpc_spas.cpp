#include "testsPCH.h"
#include "Foo.h"

using namespace cz;
using namespace cz::rpc;
using namespace cz::spas;

#include "tests_rpc_spas_helper.h"

#define LONGTEST 0

extern UnitTest::Timer gTimer;

CZRPC_DEFINE_CONST_LVALUE_REF(std::vector<int>)

// Alternatively, enable support for all "const T&" with
// CZRPC_ALLOW_CONST_LVALUE_REFS;

namespace
{
	class CalcTest
	{
	public:
		int add(int a, int b)
		{
			return a + b;
		}
	};
}

#define RPCTABLE_CLASS CalcTest
#define RPCTABLE_CONTENTS \
	REGISTERRPC(add)
#include "crazygaze/rpc/RPCGenerate.h"

SUITE(Acceptor)
{
// Does nothing. Just to check if there is anything blocking/crashing in this case
TEST(SpasTransport_Nothing)
{
	spas::Service ioservice;
	SpasTransportAcceptor acceptor(ioservice);
	SpasTransport trp(ioservice);
}

//
// Tests canceling the transport accept, with minimal helper classes
//
TEST(SpasTransport_Accept_cancel)
{
	ServiceThread ioth;
	SpasTransportAcceptor acceptor(ioth.service);

	Session<CalcTest, void> session(ioth.service);
	Semaphore done;
	CalcTest calc;
	acceptor.listen(TEST_PORT);
	acceptor.asyncAccept(nullptr, session.trp, session.con, calc, [&done](const spas::Error& ec)
	{
		CHECK_CZSPAS_EQUAL(Cancelled, ec);
		done.notify();
	});

	// Running after setting up the accept, so we don't need a dummy work item
	ioth.run(false, false);

	// cancel the accept
	ioth.service.post([&acceptor]()
	{
		acceptor.cancel();
	});

	// Service::run should return once there is no more work to do
	ioth.finish();
	CHECK_EQUAL(1, done.getCount());
}

TEST(SpasTransport_Accept_ok)
{
	ServiceThread ioth;
	SpasTransportAcceptor acceptor(ioth.service);

	Session<CalcTest, void> session(ioth.service);
	Semaphore done;
	CalcTest calc;
	acceptor.listen(TEST_PORT);
	acceptor.asyncAccept(nullptr, session.trp, session.con, calc, [&done](const spas::Error& ec)
	{
		CHECK_CZSPAS(ec);
		done.notify();
	});

	// Running after setting up the accept, so we don't need a dummy work item
	ioth.run(false, false);

	Session<void, CalcTest> clientSession(ioth.service);
	auto ec = clientSession.trp.connect(nullptr, clientSession.con, "127.0.0.1", TEST_PORT);
	CHECK_CZSPAS(ec);
	clientSession.con.close(); // Close this connection, so the service doesn't have anything else to do and finishes

	// Service::run should return once there is no more work to do
	ioth.finish();
	CHECK_EQUAL(1, done.getCount());
}

void test_asyncConnect_lambda(spas::Error::Code expected)
{
	TestRPCServer<Tester> server;

	if (expected != spas::Error::Code::Timeout)
		server.startAccept();
	server.run(false, false, "");

	ServiceThread ioth;
	Session<void, Tester> client(ioth.service);
	Semaphore done;
	client.trp.asyncConnect(nullptr, client.con, "127.0.0.1", TEST_PORT, [&](const spas::Error& ec)
	{
		CHECK_CZSPAS_EQUAL_IMPL(expected, ec);
		done.notify();
		if (expected == cz::spas::Error::Code::Success)
			client.con.close();
	});

	if (expected == spas::Error::Code::Cancelled)
	{
		// Close the connection, to cause async handlers to be called with the Cancelled error
		client.trp._getSocket().close();
	}

	ioth.run(false, false);
	ioth.finish();
	server.cancelAll();
	CHECK_EQUAL(1, done.getCount());
}

TEST(asyncConnect_lambda_cancel)
{
	test_asyncConnect_lambda(cz::spas::Error::Code::Cancelled);
}

TEST(asyncConnect_lambda_error)
{
	test_asyncConnect_lambda(cz::spas::Error::Code::Timeout);
}

TEST(asyncConnect_lambda_ok)
{
	test_asyncConnect_lambda(cz::spas::Error::Code::Success);
}


void test_asyncConnect_future(cz::spas::Error::Code expected)
{
	TestRPCServer<Tester> server;
	if (expected != spas::Error::Code::Timeout)
		server.startAccept();
	server.run(false, false, "");

	ServiceThread ioth;
	Session<void, Tester> client(ioth.service);
	auto ft = client.trp.asyncConnect(nullptr, client.con, "127.0.0.1", TEST_PORT);

	if (expected == spas::Error::Code::Cancelled)
	{
		// Close the connection, to cause async handlers to be called with the Cancelled error
		client.trp._getSocket().close();
	}

	ioth.run(false, false);
	auto ec = ft.get();
	CHECK_CZSPAS_EQUAL_IMPL(expected, ec);
	if (expected == cz::spas::Error::Code::Success)
		client.con.close();

	ioth.finish();
	server.cancelAll();
}

TEST(asyncConnect_future_cancel)
{
	test_asyncConnect_future(cz::spas::Error::Code::Cancelled);
}

TEST(asyncConnect_future_error)
{
	test_asyncConnect_future(cz::spas::Error::Code::Timeout);
}

TEST(asyncConnect_future_ok)
{
	test_asyncConnect_future(cz::spas::Error::Code::Success);
}

} // SUITE

SUITE(RPC)
{

TEST(NotAuth)
{
	TestRPCServer<Tester> server;
	server.startAccept(TEST_PORT, 1).run(true, true, "meow");

	ServiceThread ioth;
	ioth.run(true, true);

	auto session = createClientSession<void, Tester>(ioth.service);

	//
	// Calling an RPC without authenticating first (if authentication is required), will cause the transport to close;
	Semaphore sem;
	CZRPC_CALL(session->con, simple).async(
		[&](Result<void> res)
	{
		CHECK(res.isAborted());
		sem.notify();
	});
	sem.wait();

	// Test with future
	// NOTE: The connection is already close because of the previous RPC, but it should still abort any RPC calls
	// made after that
	auto ft = CZRPC_CALL(session->con, simple).ft();
	auto ftRes = ft.get();
	CHECK(ftRes.isAborted());
}

// Simple RPC without parameters or return value
TEST(Simple_and_auth)
{
	using namespace cz::rpc;
	// Also test using authentication
	TestRPCServer<Tester, void> server;
	server.startAccept(TEST_PORT, 1).run(false, false, "meow");

	ServiceThread ioth;
	ioth.run(true, false);

	auto client = createClientSessionWrapper<void, Tester>(ioth.service);

	Semaphore sem; 
	// Authenticate first
	bool authRes = false;
	CZRPC_CALLGENERIC(client->con, "__auth", std::vector<Any>{ Any("meow") }).ft().get().get().getAs(authRes);
	CHECK(authRes == true);

	// Test with async
	CZRPC_CALL(client->con, simple).async(
		[&](Result<void> res)
	{
		CHECK(res.isValid());
		sem.notify();
	});

	// Test with future
	Result<void> res = CZRPC_CALL(client->con, simple).ft().get();
	CHECK(res.isValid());

	CHECK_EQUAL(1, sem.getCount());
}

TEST(RetVal_NoParams)
{
	using namespace cz::rpc;
	TestRPCServer<Tester, void> server;
	server.startAccept().run(false, false);

	ServiceThread ioth;
	ioth.run(true, false);

	auto client = createClientSessionWrapper<void, Tester>(ioth.service);

	Semaphore sem;

	// Test with async
	CZRPC_CALL(client->con, noParams).async(
		[&](Result<int> res)
	{
		CHECK_EQUAL(128, res.get());
		sem.notify();
	});

	// Test with future
	int res = CZRPC_CALL(client->con, noParams).ft().get().get();
	CHECK_EQUAL(128, res);

	CHECK_EQUAL(1, sem.getCount());
}

TEST(WithParams)
{
	using namespace cz::rpc;
	TestRPCServer<Tester, void> server;
	server.startAccept().run(true, false);

	ServiceThread ioth;
	ioth.run(true, false);

	auto client = createClientSessionWrapper<void, Tester>(ioth.service);

	Semaphore sem;

	// Test with async
	CZRPC_CALL(client->con, add, 1,2).async(
		[&](Result<int> res)
	{
		sem.notify();
		CHECK_EQUAL(3, res.get());
	});

	// Test with future
	int res = CZRPC_CALL(client->con, add, 1, 2).ft().get().get();
	CHECK_EQUAL(3, res);

	// Test with vector
	std::vector<int> vec{ 1,2,3 };
	auto v = CZRPC_CALL(client->con, testVector1, vec).ft().get().get();
	CHECK_ARRAY_EQUAL(vec, v, 3);
	v = CZRPC_CALL(client->con, testVector2, vec).ft().get().get();
	CHECK_ARRAY_EQUAL(vec, v, 3);

	// Test with tuple
	auto tp = std::make_tuple(1, std::string("Test"));
	tp = CZRPC_CALL(client->con, testTuple, tp).ft().get().get();
	CHECK(std::get<0>(tp) == 1 && std::get<1>(tp) == "Test");

	CHECK_EQUAL(1, sem.getCount());
}

// Tests with simple parameters and return value
// - Tests returning a successful result with a future
// - Tests detecting a broken promise if the connection is closed before setting the future
TEST(Future)
{
	using namespace cz::rpc;

	TestRPCServer<Tester, void> server;
	server.startAccept().run(true, false);

	ServiceThread ioth;
	ioth.run(true, false);

	auto client = createClientSessionWrapper<void, Tester>(ioth.service);

	const int count = 2;
	ZeroSemaphore sem;
	for (int i = 0;i < count; i++)
	{
		auto p = std::to_string(i);
		sem.increment();
		CZRPC_CALL(client->con, testFuture, p.c_str()).async(
			[&sem, p](Result<std::string> res)
		{
			CHECK_EQUAL(p, res.get());
			sem.decrement();
		});
	}
	sem.wait();

	// Test destroying the server while there is still a future pending
	// To test this, we need to wait for the server to get the RPC call, then kill it before it sets the future as ready
	server.getObj().testFuturePending.increment();
	sem.increment();
	CZRPC_CALL(client->con, testFutureFailure, "test").async(
		[&sem](Result<std::string> res)
	{
		CHECK(res.isAborted());
		sem.decrement();
	});
	server.getObj().testFuturePending.wait(); // wait for the server to get the call
	server.finish(); // Stop the server before it sets the future as ready
	sem.wait(); // wait for the rpc call result (aborted)
}

// #TODO : Enable this test once we add exceptions handling to czspas
#if 0
// Test RPCs throwing exceptions
TEST(ExceptionThrowing)
{
	using namespace cz::rpc;
	TestRPCServer<Tester, void> server;
	server.startAccept().run(true, false);

	spas::Service service;

	auto client = createClientSessionWrapper<void, Tester>(service);

	ZeroSemaphore expectedUnhandledExceptions;
	std::thread ioth([&service, &expectedUnhandledExceptions]
	{
		while(true)
		{
			try
			{
				service.run();
				return;
			}
			catch (const Exception& exc)
			{
				expectedUnhandledExceptions.decrement();
				CHECK_EQUAL("Testing exception", exc.what());
				continue;
			}
			break;
		}
	});

	// Test with async
	expectedUnhandledExceptions.increment();
	CZRPC_CALL(client->con, voidTestException,  true).async(
		[&](auto res)
	{
		res.get(); // because RPC returned an exception, calling .get() will throw that exception
	});

	// RPC with exception and the client using a future
	// This will cause the future to get a broken_promise
	expectedUnhandledExceptions.increment();
	bool brokenPromise = false;
	auto ft = CZRPC_CALL(client->con, voidTestException, true).ft();
	try
	{
		ft.get().get(); // This will throw, because the RPC returned an exception
	}
	catch (const Exception& exc)
	{
		expectedUnhandledExceptions.decrement();
		CHECK_EQUAL("Testing exception", exc.what());
	}

	expectedUnhandledExceptions.wait();
}
#endif

// Having the server call a function on the client
TEST(ClientCall)
{
	using namespace cz::rpc;
	TestRPCServer<Tester, TesterClient> server;
	server.startAccept().run(true, false);

	ServiceThread ioth;
	ioth.run(true, false);
	TesterClient clientObj;
	auto client = createClientSessionWrapper<TesterClient, Tester>(ioth.service, clientObj);

	Semaphore sem;

	// We call "testClientAddCall" on the server, which in turn calls "clientAdd" on the client
	CZRPC_CALL(client->con, testClientAddCall, 1,2).async(
		[&](Result<int> res)
	{
		sem.notify();
		CHECK_EQUAL(3, res.get());
	});
	sem.wait();

	// Check if the server got the reply from the client's "testClientAddCall"
	CHECK_EQUAL(3, server.getObj().clientCallRes.get_future().get());
}

// The server is running a specialize TesterEx that overrides some virtuals,
// and the client connects asking for the Tester interface
TEST(Inheritance)
{
	using namespace cz::rpc;

	// The server is running a TesterEx, which inherits from Tester
	TestRPCServer<TesterEx, void> server;
	server.startAccept().run(true, false);

	ServiceThread ioth;
	ioth.run(true, false);

	// The client connects as using Tester
	auto client = createClientSessionWrapper<void, Tester>(ioth.service);

	Semaphore sem;
	// Calling "virtualFunc" should call TesterEx::virtualFuc does, not Tester::virtualFunc, even though the client
	// only knows about the "Tester" interface
	CZRPC_CALL(client->con, virtualFunc).async(
		[&](const Result<std::string>& res)
	{
		sem.notify();
		CHECK_EQUAL("TesterEx", res.get().c_str());
	});
	sem.wait();
}

//
// Tests how many times a parameter of complex type is copied/moved
// This doesn't really show any bugs, but makes sure we are getting the optimal behavior, since we should prefer
// moving instead of copying
TEST(Constructors)
{
	using namespace cz::rpc;

	TestRPCServer<Tester, void> server;
	server.startAccept().run(true, false);

	ServiceThread ioth;
	ioth.run(true, false);

	auto client = createClientSessionWrapper<void, Tester>(ioth.service);

	Foo foo(1);
	Foo::resetCounters();
	auto ft = CZRPC_CALL(client->con, testFoo1, foo).ft();
	CHECK(ft.get().get() == true);
	Foo::check(1, 0, 0);

	Foo::resetCounters();
	ft = CZRPC_CALL(client->con, testFoo2, foo).ft();
	CHECK(ft.get().get() == true);
	Foo::check(1, 1, 0);
}

#define ANY_CHECK(a_, type, str)            \
	{ \
		Any& a = a_; \
		CHECK(a.getType() == Any::Type::type); \
		CHECK(std::string(a.toString()) == str); \
	}

// Tests using "Any"
// This calls an RPC on the server with a parameter "Any" of a specific type, and expects the server to an exact copy
// back
TEST(Any)
{
	using namespace cz::rpc;

	TestRPCServer<Tester, void> server;
	server.startAccept().run(true, false);

	ServiceThread ioth;
	ioth.run(true, false);
	auto client = createClientSessionWrapper<void, Tester>(ioth.service);

	{
		auto res = CZRPC_CALL(client->con, testAny, Any()).ft().get();
		ANY_CHECK(res.get(), None, "")
	}
	{
		auto res = CZRPC_CALL(client->con, testAny, Any(true)).ft().get();
		ANY_CHECK(res.get(), Bool, "true")
	}
	{
		auto res = CZRPC_CALL(client->con, testAny, Any(int(1234))).ft().get();
		ANY_CHECK(res.get(), Integer, "1234")
	}
	{
		auto res = CZRPC_CALL(client->con, testAny, Any(int(-1234))).ft().get();
		ANY_CHECK(res.get(), Integer, "-1234")
	}
	{
		auto res = CZRPC_CALL(client->con, testAny, Any(unsigned(1234))).ft().get();
		ANY_CHECK(res.get(), UnsignedInteger, "1234")
	}
	{
		auto res = CZRPC_CALL(client->con, testAny, Any(float(1234.5))).ft().get();
		ANY_CHECK(res.get(), Float, "1234.5000")
	}
	{
		auto res = CZRPC_CALL(client->con, testAny, Any("hello")).ft().get();
		ANY_CHECK(res.get(), String, "hello")
	}
	{
		auto res = CZRPC_CALL(client->con, testAny, Any(std::vector<unsigned char>{0,1,2,3})).ft().get();
		ANY_CHECK(res.get(), Blob, "BLOB{4}");
		std::vector<unsigned char> v;
		CHECK(res.get().getAs(v) == true);
		CHECK_ARRAY_EQUAL(v, std::vector<unsigned char>({0, 1, 2, 3}), 4);
	}
}

TEST(Generic)
{
	TestRPCServer<Tester, void> server;
	server.startAccept().run(true, false);

	ServiceThread ioth;
	ioth.run(true, false);

	// Note that since we only want to use generic RPCs from this client we don't need to know
	// the server type. We can use "GenericServer"
	auto client = createClientSessionWrapper<void, GenericServer>(ioth.service);

	// Calling a non existent generic function
	{
		auto res = CZRPC_CALLGENERIC(client->con, "nonexistent").ft().get();
		CHECK(res.isException());
		CHECK(res.getException() == "Generic RPC not found");
	}
	{
		auto res = CZRPC_CALLGENERIC(client->con, "simple", std::vector<Any>{Any(true)}).ft().get();
		CHECK(res.isException());
		CHECK(res.getException() == "Invalid parameters for generic RPC");
	}

	{
		auto res = CZRPC_CALLGENERIC(client->con, "simple").ft().get().get();
		CHECK(res.getType() == Any::Type::None);
	}
	{
		auto res = CZRPC_CALLGENERIC(client->con, "noParams").ft().get().get();
		CHECK(res.getType() == Any::Type::Integer);
		CHECK(std::string(res.toString()) == "128");
	}
	{
		auto res = CZRPC_CALLGENERIC(client->con, "add", std::vector<Any>{Any(1), Any(2)}).ft().get().get();
		CHECK(res.getType() == Any::Type::Integer);
		CHECK(std::string(res.toString()) == "3");
	}

}

// Tests the case when the server wants to call a client side RPC, but the client
// processor is actually InProcessor<void>
TEST(VoidPeer)
{
	using namespace cz::rpc;

	// The server expects the client to have a TesterClient API,
	// but if the client is using InProcessor<void>, it should still get a reply with an error
	TestRPCServer<Tester, TesterClient> server;
	server.startAccept().run(true, false);

	ServiceThread ioth;
	ioth.run(true, false);

	// Instead of having a LOCAL of TesterClient, like the server expects, we
	// use void, to test the behavior
	auto client = createClientSessionWrapper<void, Tester>(ioth.service);

	// Call an RPC on the server, that in turn will try to call one on the client-side.
	// Since the client is using InProcessor<void>, it cannot reply. It will just send back
	// an error. The server will in turn send us back that exception for us to check here
	auto res = CZRPC_CALL(client->con, testClientVoid).ft().get();
	CHECK_EQUAL("Peer doesn't have an object to process RPC calls", res.get());
}

// Tests using control RPCS, such as __getProperty/__setProperty
TEST(ControlRPCs)
{
	using namespace cz::rpc;

	TestRPCServer<Tester, void> server;
	server.startAccept().run(true, false);
	{
		// Add some properties to the server
		ObjectData(&server.getObj()).setProperty("name", Any("Tester1"));
	}

	ServiceThread ioth;
	ioth.run(true, false);
	// Specifying GenericServer as REMOTE type, since we only need to call generic RPCs
	auto client = createClientSessionWrapper<void, GenericServer>(ioth.service);

	{
		auto res = CZRPC_CALLGENERIC(client->con, "__getProperty", std::vector<Any>{Any("prop1")}).ft().get();
		CHECK(res.get().getType() == Any::Type::None);
	}

	{
		auto res = CZRPC_CALLGENERIC(client->con, "__setProperty", std::vector<Any>{Any("prop1"), Any(false)}).ft().get();
		CHECK(res.get().getType() == Any::Type::Bool);
		CHECK(std::string(res.get().toString()) == "true");
		res = CZRPC_CALLGENERIC(client->con, "__getProperty", std::vector<Any>{Any("prop1")}).ft().get();
		CHECK(res.get().getType() == Any::Type::Bool);
		CHECK(std::string(res.get().toString()) == "false");
	}

	{
		auto res = CZRPC_CALLGENERIC(client->con, "__setProperty", std::vector<Any>{Any("prop1"), Any("Hello")}).ft().get();
		CHECK(res.get().getType() == Any::Type::Bool);
		CHECK(std::string(res.get().toString()) == "true");
		res = CZRPC_CALLGENERIC(client->con, "__getProperty", std::vector<Any>{Any("prop1")}).ft().get();
		CHECK(res.get().getType() == Any::Type::String);
		CHECK(std::string(res.get().toString()) == "Hello");
	}

	{
		auto res = CZRPC_CALLGENERIC(client->con, "__getProperty", std::vector<Any>{Any("name")}).ft().get();
		CHECK(res.get().getType() == Any::Type::String);
		CHECK(std::string(res.get().toString()) == "Tester1");
	}

}

TEST(Latency)
{
	using namespace cz::rpc;

	TestRPCServer<Tester, void> server;
	server.startAccept().run(true, false);

	ServiceThread ioth;
	ioth.run(true, false);

	auto client = createClientSessionWrapper<void, Tester>(ioth.service);

	const int count = (LONGTEST) ? 200 : 20;
	std::vector<double> times(count, 0.0f);
	for(int i=0; i<count; i++)
	{
		auto start = gTimer.GetTimeInMs();
		auto res = CZRPC_CALL(client->con, noParams).ft().get();
		times[i] = gTimer.GetTimeInMs() - start;
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
}

TEST(Throughput)
{
	using namespace cz::rpc;

	TestRPCServer<Tester, void> server;
	server.startAccept().run(true, false);

	ServiceThread ioth;
	ioth.run(true, false);

	auto client = createClientSessionWrapper<void, Tester>(ioth.service);

	std::atomic<bool> finish(false);
	double start, end;
	auto test = std::async(std::launch::async,
		[&finish, &client, &start, &end, &server]
	{
		int size = 1024 * 1024 / 4;
		if (SHORT_TESTS)
			size /= 4;

		//std::vector<char> data(size, 'a');
		std::string data(size, 'a');

		start = gTimer.GetTimeInMs();
		int id = 0;
		ZeroSemaphore sem;
		uint64_t totalBytes = 0;
		std::atomic<int> flying(0);
		while(!finish)
		{
			sem.increment();
			if (flying.load() > 5)
				server.getObj().throughputSem.wait();

			++flying;
			CZRPC_CALL(client->con, testThroughput1, data, id).async(
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
		end = gTimer.GetTimeInMs();
		return std::make_pair(
			(end - start) / 1000, // seconds
			totalBytes // data sent
		);
	});

	unsigned sleepms = LONGTEST ? 300000 : 4000;
	if (SHORT_TESTS)
		sleepms /= 8;
	UnitTest::TimeHelpers::SleepMs(sleepms);

	finish = true;
	auto res = test.get();

	auto seconds = res.first;
	auto mb = (double)res.second/(1000*1000);
	printf("RPC throughput: %0.2f Mbit/s (%0.2f MB/s)\n", (mb*8)/seconds, mb/seconds);
}

}
