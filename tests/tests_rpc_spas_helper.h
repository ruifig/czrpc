#pragma once

//namespace {

// Just puts together a Connection and Transport
template<typename LOCAL, typename REMOTE>
struct ConTrp
{
	explicit ConTrp(spas::Service& io)
		: trp(io)
	{
	}
	~ConTrp() {}
	Connection<LOCAL, REMOTE> con;
	SpasTransport trp;
};

#define TEST_PORT 9000

//
// Utility class just to make the unit tests shorter, by allowing tweaks on how to handle spas::Service, and
// SpasTransport/SpasTransportAcceptor
//
template<typename LOCAL, typename REMOTE=void>
class TestRPCServer
{
public:
	using Local = LOCAL;
	using Remote = REMOTE;

	explicit TestRPCServer()
		: m_objData(&m_servedObj)
		, m_acceptor(m_io)
	{
		m_clients = getSharedData<Clients>();
	}

	~TestRPCServer()
	{
		finish();
	}

	static std::shared_ptr<ConTrp<Local, Remote>> getCurrent()
	{
		auto client = cz::rpc::Connection<Local, Remote>::getCurrent();
		auto clients = getShared<Clients>();
		if (!clients)
			return nullptr;

		for (auto&& c : clients)
			if (c->con.get() == clients)
				return c;
		return nullptr;
	}

	//! Starts the listen and accept for new connections
	// \param port
	//    Port to use
	// \param maxConnections
	//    How many successful connections to accept. Once this is reached, the acceptor is closed
	TestRPCServer& startAccept(int port = TEST_PORT, int maxConnections = INT_MAX)
	{
		m_acceptor.listen(port);
		setupAccept(maxConnections);
		return *this;
	}

	//! Runs the Service::run on a separate thread.
	// \param keepAlive
	//    Will add a Service::Work item to the Service, so Service::run doesn't return until Service::stop is used
	// \param autoStop
	//    Will call Service::stop from destructor
	TestRPCServer& run(bool keepAlive, bool autoStop, std::string authToken = "")
	{
		CHECK(m_th.joinable() == false);
		m_autoStop = autoStop;

		m_objData.setAuthToken(authToken);

		m_th = std::thread([this, keepAlive]
		{
			// If required, add the dummy Work to the service, so that Service::run doesn't exit immediately
			std::unique_ptr<spas::Service::Work> work;
			if (keepAlive)
				work = std::make_unique<spas::Service::Work>(m_io);
			m_io.run();
		});

		return *this;
	}

	void finish()
	{
		if (m_autoStop)
			m_io.stop();
		if (m_th.joinable())
			m_th.join();
	}

private:

	void setupAccept(int leftToAccept)
	{
		if (leftToAccept == 0)
			return;
		auto contrp = std::make_shared<ConTrp<Local, Remote>>(m_io);
		m_acceptor.asyncAccept(contrp->trp, contrp->con, m_servedObj,
			[this, leftToAccept, contrp](const spas::Error& ec)
		{
			int todo = leftToAccept;
			if (ec)
			{
				if (ec.code == spas::Error::Code::Cancelled)
					return;
				CZRPC_LOG(Log, "Failed to accept connection: %s", ec.msg());
			}
			else
			{
				m_clients.push_back(contrp);
				todo--;
			}
			setupAccept(todo);
		});
	}

	Local m_servedObj;
	ObjectData m_objData;
	spas::Service m_io;
	std::thread m_th;
	SpasTransportAcceptor m_acceptor;
	bool m_autoStop = false;

	using Clients = std::vector<std::shared_ptr<ConTrp<Local, Remote>>>;
	std::shared_ptr<Clients> m_clients;
};

//! Helper class to run a Service in a separate thread.
struct ServiceThread
{
	spas::Service service;
	std::thread th;
	bool autoStop = false;

	explicit ServiceThread() { }
	~ServiceThread()
	{
		finish();
	}

	ServiceThread& run(bool keepAlive, bool autoStop)
	{
		this->autoStop = autoStop;
		CHECK(th.joinable() == false);
		th = std::thread([this, keepAlive]()
		{
			std::unique_ptr<spas::Service::Work> work;
			if (keepAlive)
				work = std::make_unique<spas::Service::Work>(service);
			service.run();
		});

		return *this;
	}

	void finish()
	{
		if (autoStop)
			service.stop();
		if (th.joinable())
			th.join();
	}
};

// Class to be used as server side RPC interface, to test as much stuff as we can
class Tester
{
public:
	
	~Tester()
	{
	}

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

	Semaphore throughputSem;
	int testThroughput1(std::string v, int id)
	{
		throughputSem.notify();
		return id;
	}
	int testThroughput2(std::vector<char> v, int id)
	{
		throughputSem.notify();
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

	std::future<std::string> testFutureFailure(const char* str)
	{
		testFuturePending.decrement();
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

	ZeroSemaphore testFuturePending;
	std::promise<int> clientCallRes; // So the unit test can wait on the future to make sure the server got the reply from the server
};

//! To test inheritance
class TesterEx : public Tester
{
public:

	int leftShift(int v, int bits)
	{
		return v << bits;
	}

	virtual const char* virtualFunc() override
	{
		return "TesterEx";
	}
};

//! Client side rpc interface
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
	REGISTERRPC(testFutureFailure) \
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
	auto client = TestRPCServer<Tester, TesterClient>::getCurrent();
	CHECK(client != nullptr);

	auto pr = std::make_shared<std::promise<std::string>>();
	CZRPC_CALL(client->con, clientAdd, 1, 2).async(
		[this, client->con, pr](Result<int> res)
	{
		CHECK(res.isException());
		pr->set_value(res.getException());
	});

	return pr->get_future();
}


//} // anonymous namespace
