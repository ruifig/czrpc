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
	REGISTERRPC(testTuple) \
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
		[this, r = a+b](Reply<int> res)
	{
		CHECK_EQUAL(r, res.get());
		clientCallRes = res.get();
	});

	return a + b;
}


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

	auto clientCon = AsioTransport<void,Tester>::create(io, "127.0.0.1", TEST_PORT).get();

	ZeroSemaphore sem; // Used to make sure all rpcs were called
	sem.increment();

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
		[&](Reply<int> res)
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
		[&](Reply<int> res)
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
		[&](Reply<int> res)
	{
		pending.decrement();
		CHECK_EQUAL(3, res.get());
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
	auto clientCon = AsioTransport<void, Tester>::create(io, "127.0.0.1", TEST_PORT).get();

	ZeroSemaphore pending;
	pending.increment();
	CZRPC_CALL(*clientCon, virtualFunc).async(
		[&](const Reply<std::string>& res)
	{
		pending.decrement();
		CHECK_EQUAL("TesterEx", res.get().c_str());
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

	// Make sure the server got the client reply
	io.stop();
	iothread.join();

}

/*
// Sample shown http://www.crazygaze.com/blog/2016/06/06/modern-c-lightweight-binary-rpc-framework-without-code-generation/

//////////////////////////////////////////////////////////////////////////
// Useless RPC-agnostic class that performs calculations.
class Calculator
{
public:
  double add(double a, double b) { return a + b; }
};

// Define the RPC table for the Calculator class
#define RPCTABLE_CLASS Calculator
#define RPCTABLE_CONTENTS \
	REGISTERRPC(add)
#include "crazygaze/rpc/RPCGenerate.h"


void RunServer()
{

}

void RunClient()
{
}

TEST(BlogArticleSample)
{
	// SERVER
	using namespace ArticleSample;
	auto serverThread = std::thread([]
	{
		RunServer();
	});


	// CLIENT
}

*/

}


//////////////////////////////////////////////////////////////////////////
// Useless RPC-agnostic class that performs calculations.
//////////////////////////////////////////////////////////////////////////
class Calculator
{
public:
	double add(double a, double b) { return a + b; }
};

//////////////////////////////////////////////////////////////////////////
// Define the RPC table for the Calculator class
// This needs to be seen by both the server and client code
//////////////////////////////////////////////////////////////////////////
#define RPCTABLE_CLASS Calculator
#define RPCTABLE_CONTENTS \
	REGISTERRPC(add)
#include "crazygaze/rpc/RPCGenerate.h"

//////////////////////////////////////////////////////////////////////////
// A Server that that only accepts 1 client, then shuts down
// when the client disconnects
//////////////////////////////////////////////////////////////////////////
void RunServer()
{
	asio::io_service io;
	// Start thread to run Asio's the io_service
	// we will be using for the server
	std::thread th = std::thread([&io]
	{
		asio::io_service::work w(io);
		io.run();
	});

	// Instance we will be using to serve RPC calls.
	// Note that it's an object that knows nothing about RPCs
	Calculator calc;

	// start listening for a client connection.
	// We specify what Calculator instance clients will use,
	auto acceptor = AsioTransportAcceptor<Calculator,void>::create(io, calc);
	// Start listening on port 9000.
	// For simplicity, we are only expecting 1 client
	using ConType = Connection<Calculator, void>;
	std::shared_ptr<ConType> con;
	acceptor->start(9000, [&io,&con](std::shared_ptr<ConType> con_)
	{
		con = con_;
		// Since this is just a sample, close the server once the first client disconnects
		reinterpret_cast<BaseAsioTransport*>(con->transport.get())->setOnClosed([&io]
		{
			io.stop();
		});
	});

	th.join();
}

//////////////////////////////////////////////////////////////////////////
// A client that connects to the server, calls 1 RPC
// then disconnects, causing everything to shut down
//////////////////////////////////////////////////////////////////////////
void RunClient()
{
	// Start a thread to run our Asio io_service
	asio::io_service io;
	std::thread th = std::thread([&io]
	{
		asio::io_service::work w(io);
		io.run();
	});

	// Connect to the server (localhost, port 9000)
	auto con = AsioTransport<void, Calculator>::create(io, "127.0.0.1", 9000).get();

	// Call one RPC (the add method), specifying an asynchronous handler for when the result arrives
	CZRPC_CALL(*con, add, 1, 2).async([&io](Reply<double> res)
	{
		printf("Result=%f\n", res.get()); // Prints 3.0
		// Since this is a just a sample, stop the io_service after we get the result,
		// so everything shuts down
		io.stop();
	});

	th.join();
}

// For testing simplicity, run both the server and client on the same machine,
void RunServerAndClient()
{
	auto a = std::thread([] { RunServer(); });
	auto b = std::thread([] { RunClient(); });
	a.join();
	b.join();
}

SUITE(ArticleSamples)
{

TEST(ArticleSample)
{
	RunServerAndClient();
}
}