#include "testsPCH.h"
#include "Semaphore.h"
#include "Foo.h"


#define TEST_PORT 9000

using namespace cz::rpc;

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
			throw std::exception("Testing exception");
	}

	int intTestException(bool doThrow)
	{
		if (doThrow)
			throw std::exception("Testing exception");
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

	int clientCallRes = 0;
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
		clientCallRes = res.get();
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

	explicit ServerProcess(int port, std::string authToken="")
		: m_objData(&m_obj)
	{
		m_th = std::thread([this]
		{
			ASIO::io_service::work w(m_io);
			m_io.run();
		});

		m_objData.setAuthToken(std::move(authToken));

		m_acceptor = AsioTransportAcceptor<Local, Remote>::create(m_io, m_obj);
		m_acceptor->start(port, [&](std::shared_ptr<Connection<Local, Remote>> con)
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
	ASIO::io_service m_io;
	std::thread m_th;
	LOCAL m_obj;
	ObjectData m_objData;
	std::shared_ptr<AsioTransportAcceptor<Local, Remote>> m_acceptor;
	std::vector<std::shared_ptr<Connection<Local, Remote>>> m_cons;
};

SUITE(RPCTraits)
{

TEST(NotAuth)
{
	using namespace cz::rpc;
	ServerProcess<Tester, void> server(TEST_PORT, "meow");

	ASIO::io_service io;
	std::thread iothread = std::thread([&io]
	{
		ASIO::io_service::work w(io);
		io.run();
	});

	auto clientCon = AsioTransport<void,Tester>::create(io, "127.0.0.1", TEST_PORT).get();

	//
	// Calling an RPC without authenticating first (if authentication is required),
	// will cause the transport to close;
	bool asyncAborted = false;
	// Test with async
	CZRPC_CALL(*clientCon, simple).async(
		[&](Result<void> res)
	{
		asyncAborted = res.isAborted();
	});

	// Test with future
	auto ft = CZRPC_CALL(*clientCon, simple).ft();
	auto ftRes = ft.get();

	CHECK(asyncAborted);
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

	ASIO::io_service io;
	std::thread iothread = std::thread([&io]
	{
		ASIO::io_service::work w(io);
		io.run();
	});

	auto clientCon = AsioTransport<void,Tester>::create(io, "127.0.0.1", TEST_PORT).get();

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

	ASIO::io_service io;
	std::thread iothread = std::thread([&io]
	{
		ASIO::io_service::work w(io);
		io.run();
	});

	auto clientCon = AsioTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

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

	ASIO::io_service io;
	std::thread iothread = std::thread([&io]
	{
		ASIO::io_service::work w(io);
		io.run();
	});

	auto clientCon = AsioTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

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

// Test RPCs throwing exceptions
TEST(ExceptionThrowing)
{
	using namespace cz::rpc;
	ServerProcess<Tester, void> server(TEST_PORT);

	ASIO::io_service io;
	ZeroSemaphore expectedUnhandledExceptions;
	std::thread iothread = std::thread([&io, &expectedUnhandledExceptions]
	{
		while(true)
		{
			try
			{
				ASIO::io_service::work w(io);
				io.run();
			}
			catch (const Exception&)
			{
				expectedUnhandledExceptions.decrement();
				io.reset();
				continue;
			}
			break;
		}
	});

	auto clientCon = AsioTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

	ZeroSemaphore sem; // Used to make sure all rpcs were called

	// Test with async
	expectedUnhandledExceptions.increment();
	CZRPC_CALL(*clientCon, voidTestException,  true).async(
		[&](auto res)
	{
		res.get(); // this will throw an exception, because the RPC returned an exception
	});

	// RPC with exception and the client using a future
	// This will cause the future to get a broken_promise:vsplit
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

	ASIO::io_service io;
	std::thread iothread = std::thread([&io]
	{
		ASIO::io_service::work w(io);
		io.run();
	});

	TesterClient clientObj;
	auto clientCon = AsioTransport<TesterClient, Tester>::create(io, clientObj, "127.0.0.1", TEST_PORT).get();

	ZeroSemaphore pending;

	pending.increment();
	CZRPC_CALL(*clientCon, testClientAddCall, 1,2).async(
		[&](Result<int> res)
	{
		pending.decrement();
		CHECK_EQUAL(3, res.get());
	});

	// Make sure the server got the client reply
	pending.wait();
	CHECK_EQUAL(3, server.obj().clientCallRes);

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

	ASIO::io_service io;
	std::thread iothread = std::thread([&io]
	{
		ASIO::io_service::work w(io);
		io.run();
	});

	// The client connects as using Tester
	auto clientCon = AsioTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

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

	ASIO::io_service io;
	std::thread iothread = std::thread([&io]
	{
		ASIO::io_service::work w(io);
		io.run();
	});

	auto clientCon = AsioTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

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

	ASIO::io_service io;
	std::thread iothread = std::thread([&io]
	{
		ASIO::io_service::work w(io);
		io.run();
	});

	auto clientCon = AsioTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

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

	ASIO::io_service io;
	std::thread iothread = std::thread([&io]
	{
		ASIO::io_service::work w(io);
		io.run();
	});

	// Note that since we only want to use generic RPCs from this client we don't need to know
	// the server type. We can use "GenericServer"
	auto clientCon = AsioTransport<void, GenericServer>::create(io, "127.0.0.1", TEST_PORT).get();

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

	ASIO::io_service io;
	std::thread iothread = std::thread([&io]
	{
		ASIO::io_service::work w(io);
		io.run();
	});

	// Instead of having a LOCAL of TesterClient, like the server expects, we
	// use void, to test the behavior
	auto clientCon = AsioTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

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

	ASIO::io_service io;
	std::thread iothread = std::thread([&io]
	{
		ASIO::io_service::work w(io);
		io.run();
	});

	// Specifying GenericServer as REMOTE type, since we only need to call generic RPCs
	auto clientCon = AsioTransport<void, GenericServer>::create(io, "127.0.0.1", TEST_PORT).get();

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

}
