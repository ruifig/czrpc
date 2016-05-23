#include "testsPCH.h"
#include "Semaphore.h"
#include "Foo.h"


#define TEST_PORT 9000

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

	bool testFoo1(const Foo& f)
	{
		return true;
	}

	bool testFoo2(Foo f)
	{
		return true;
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
	REGISTERRPC(voidTestException) \
	REGISTERRPC(intTestException) \
	REGISTERRPC(testVector1) \
	REGISTERRPC(testVector2) \
	REGISTERRPC(testFoo1) \
	REGISTERRPC(testFoo2)

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
		[this, r = a+b](int res)
	{
		CHECK_EQUAL(r, res);
		clientCallRes = res;
	});

	return a + b;
}

using namespace cz::rpc;

CZRPC_DEFINE_CONST_LVALUE_REF(std::vector<typename>)

//
// To simulate a server process, serving one single object instance
//
template<typename LOCAL, typename REMOTE>
class ServerProcess
{
public:
	using Local = LOCAL;
	using Remote = REMOTE;

	ServerProcess(int port)
	{
		m_th = std::thread([this]
		{
			ASIO::io_service::work w(m_io);
			m_io.run();
		});

		m_acceptor = std::make_shared<AsioTransportAcceptor<Local, Remote>>(m_io, m_obj);
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
	std::shared_ptr<AsioTransportAcceptor<Local, Remote>> m_acceptor;
	std::vector<std::shared_ptr<Connection<Local, Remote>>> m_cons;
};

SUITE(RPCTraits)
{

// RPC without return value or parameters
TEST(Simple)
{
	using namespace cz::rpc;
	ServerProcess<Tester, void> server(TEST_PORT);

	ASIO::io_service io;
	std::thread iothread = std::thread([&io]
	{
		ASIO::io_service::work w(io);
		io.run();
	});

	auto clientCon = AsioTransport::create<void, Tester>(io, "127.0.0.1", TEST_PORT).get();

	ZeroSemaphore sem; // Used to make sure all rpcs were called
	sem.increment();

	// Test with async
	CZRPC_CALL(*clientCon, simple).async(
		[&]()
	{
		sem.decrement();
	});
	// Test with exception handling
	sem.increment();
	CZRPC_CALL(*clientCon, simple).asyncEx(
		[&](Expected<void> res)
	{
		sem.decrement();
		CHECK(res.valid());
	});

	// Test with future
	CZRPC_CALL(*clientCon, simple).ft().get();

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

	auto clientCon = AsioTransport::create<void, Tester>(io, "127.0.0.1", TEST_PORT).get();

	ZeroSemaphore sem; // Used to make sure all rpcs were called
	sem.increment();

	// Test with async
	CZRPC_CALL(*clientCon, noParams).async(
		[&](int res)
	{
		sem.decrement();
		CHECK_EQUAL(128, res);
	});

	// Test with exception handling
	sem.increment();
	CZRPC_CALL(*clientCon, noParams).asyncEx(
		[&](Expected<int> res)
	{
		sem.decrement();
		CHECK_EQUAL(128, *res);
	});

	// Test with future
	int res = CZRPC_CALL(*clientCon, noParams).ft().get();
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

	auto clientCon = AsioTransport::create<void, Tester>(io, "127.0.0.1", TEST_PORT).get();

	ZeroSemaphore sem; // Used to make sure all rpcs were called
	sem.increment();

	// Test with async
	CZRPC_CALL(*clientCon, add, 1,2).async(
		[&](int res)
	{
		sem.decrement();
		CHECK_EQUAL(3, res);
	});

	// Test with exception handling
	sem.increment();
	CZRPC_CALL(*clientCon, add, 1,2).asyncEx(
		[&](Expected<int> res)
	{
		sem.decrement();
		CHECK_EQUAL(3, *res);
	});

	// Test with future
	int res = CZRPC_CALL(*clientCon, add, 1, 2).ft().get();
	CHECK_EQUAL(3, res);

	// Test with vector
	std::vector<int> vec{ 1,2,3 };
	auto v = CZRPC_CALL(*clientCon, testVector1, vec).ft().get();
	CHECK_ARRAY_EQUAL(vec, v, 3);
	v = CZRPC_CALL(*clientCon, testVector2, vec).ft().get();
	CHECK_ARRAY_EQUAL(vec, v, 3);


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

	auto clientCon = AsioTransport::create<void, Tester>(io, "127.0.0.1", TEST_PORT).get();

	ZeroSemaphore sem; // Used to make sure all rpcs were called

	// Test with async
	expectedUnhandledExceptions.increment();
	CZRPC_CALL(*clientCon, voidTestException,  true).async(
		[&]()
	{
		// This is never called, because the RPC will throw an exception, and we are using .async instead of .asyncEx
	});

	sem.increment();
	CZRPC_CALL(*clientCon, voidTestException, true).asyncEx(
		[&](Expected<void> res)
	{
		sem.decrement();
		CHECK(!res.valid());
		CHECK_THROW(res.get(), Exception);
	});

	// RPC with exception and the client using a future
	// This will cause the future to get a broken_promise:vsplit
	expectedUnhandledExceptions.increment();
	bool brokenPromise = false;
	auto ft = CZRPC_CALL(*clientCon, voidTestException, true).ft();
	try
	{
		ft.get();
	}
	catch (std::future_error& e)
	{
		brokenPromise = true;
		CHECK_EQUAL(std::future_errc::broken_promise, e.code());
	}

	sem.wait();
	io.stop();
	iothread.join();
	expectedUnhandledExceptions.wait();
	CHECK(brokenPromise);
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
	auto clientCon = AsioTransport::create<TesterClient, Tester>(io, clientObj, "127.0.0.1", TEST_PORT).get();

	ZeroSemaphore pending;

	pending.increment();
	CZRPC_CALL(*clientCon, testClientAddCall, 1,2).async(
		[&](int res)
	{
		pending.decrement();
		CHECK_EQUAL(3, res);
	});

	pending.wait();
	// Make sure the server got the client reply
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
	auto clientCon = AsioTransport::create<void, Tester>(io, "127.0.0.1", TEST_PORT).get();

	ZeroSemaphore pending;
	pending.increment();
	CZRPC_CALL(*clientCon, virtualFunc).async(
		[&](const std::string& res)
	{
		pending.decrement();
		CHECK_EQUAL("TesterEx", res.c_str());
	});

	pending.wait();
	// Make sure the server got the client reply
	io.stop();
	iothread.join();
}

TEST(Constructors)
{
	using namespace cz::rpc;
	// #TODO : Create a unit test to check that I'm not creating more copies than necessary of an object when calling
	// RPCs

	ServerProcess<Tester, void> server(TEST_PORT);

	ASIO::io_service io;
	std::thread iothread = std::thread([&io]
	{
		ASIO::io_service::work w(io);
		io.run();
	});

	auto clientCon = AsioTransport::create<void, Tester>(io, "127.0.0.1", TEST_PORT).get();

	Foo foo(1);
	Foo::resetCounters();
	auto ft = CZRPC_CALL(*clientCon, testFoo1, foo).ft();
	CHECK(ft.get() == true);
	Foo::check(1, 0, 0);

	Foo::resetCounters();
	ft = CZRPC_CALL(*clientCon, testFoo2, foo).ft();
	CHECK(ft.get() == true);
	Foo::check(1, 1, 0);

	// Make sure the server got the client reply
	io.stop();
	iothread.join();

}

}


