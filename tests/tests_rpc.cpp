#include "testsPCH.h"
#include "Semaphore.h"


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

	void testClientAddCall(int a, int b);

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
	REGISTERRPC(testClientAddCall) \
	REGISTERRPC(voidTestException) \
	REGISTERRPC(intTestException)

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
void Tester::testClientAddCall(int a, int b)
{
	auto client = cz::rpc::Connection<Tester, TesterClient>::getCurrent();
	CHECK(client != nullptr);
	CZRPC_CALL(*client, clientAdd, a, b).async(
		[r = a+b](int res)
	{
		CHECK_EQUAL(r, res);
	});
}

using namespace cz::rpc;

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
	// This will cause the future to get a broken_promise
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



