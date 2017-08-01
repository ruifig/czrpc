#pragma once

#define CHECK_CZSPAS_EQUAL_IMPL(expected, ec)                                                                      \
	if ((ec.code) != (expected))                                                                 \
	{                                                                                                         \
		UnitTest::CheckEqual(*UnitTest::CurrentTest::Results(), Error(expected).msg(), ec.msg(), \
		                     UnitTest::TestDetails(*UnitTest::CurrentTest::Details(), __LINE__));             \
	}

#define CHECK_CZSPAS_EQUAL(expected, ec) CHECK_CZSPAS_EQUAL_IMPL(cz::spas::Error::Code::expected, ec)
#define CHECK_CZSPAS(ec) CHECK_CZSPAS_EQUAL(Success, ec)

//namespace {

// Just puts together a Connection and Transport
template<typename LOCAL, typename REMOTE>
struct Session : rpc::SessionData
{
	explicit Session(spas::Service& service)
		: trp(service)
	{
		//printf("Session: %p constructed\n", this);
	}
	~Session()
	{
		//printf("Session: %p destroyed\n", this);
	}
	Connection<LOCAL, REMOTE> con;
	SpasTransport trp;
};


// A wrapper around a Session, so it closes a connection when going out of scope
template<typename LOCAL, typename REMOTE>
struct SessionWrapper
{
	explicit SessionWrapper(spas::Service& service)
	{
		session = std::make_shared<Session<LOCAL, REMOTE>>(service);
	}

	explicit SessionWrapper(std::shared_ptr<Session<LOCAL,REMOTE>> session)
		: session(std::move(session))
	{
	}

	~SessionWrapper()
	{
		if (session)
			session->con.close();
	}

	Session<LOCAL, REMOTE>* operator->() const
	{
		return session.get();
	}
	std::shared_ptr<Session<LOCAL, REMOTE>> session;
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
	using ConType = cz::rpc::Connection<Local, Remote>;

	explicit TestRPCServer()
		: m_objData(&m_servedObj)
		, m_acceptor(m_service)
	{
	}

	~TestRPCServer()
	{
		finish();
	}

	std::shared_ptr<Session<Local, Remote>> getCurrent()
	{
		auto con = ConType::getCurrent();
		for (auto&& c : m_clients)
			if (con == &c->con)
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
		auto ec = m_acceptor.listen(port);
		CHECK_CZSPAS(ec);
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
			if (keepAlive)
				m_keepAliveWork = std::make_unique<spas::Service::Work>(m_service);
			m_service.run();
		});

		return *this;
	}

	void cancelAll()
	{
		m_service.post([this]()
		{
			m_acceptor.cancel();
			for (auto&& c : m_clients)
				c->con.close();
		});
	}

	void finish()
	{
		cancelAll();
		m_keepAliveWork = nullptr;
		if (m_autoStop)
			m_service.stop();
		if (m_th.joinable())
			m_th.join();
	}

	Local& getObj()
	{
		return m_servedObj;
	}

private:

	void setupAccept(int leftToAccept)
	{
		if (leftToAccept == 0)
			return;
		auto session = std::make_shared<Session<Local, Remote>>(m_service);
		m_acceptor.asyncAccept(session, session->trp, session->con, m_servedObj,
			[this, leftToAccept, session](const spas::Error& ec)
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
				m_clients.push_back(session);
				todo--;

				session->con.setOnDisconnect([this, session]()
				{
					onDisconnect(session);
				});
			}
			setupAccept(todo);
		});
	}

	void onDisconnect(std::shared_ptr<Session<Local, Remote>> session)
	{
		m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), session));
	}

	Local m_servedObj;
	ObjectData m_objData;
	spas::Service m_service;
	std::thread m_th;
	SpasTransportAcceptor m_acceptor;
	bool m_autoStop = false;
	std::unique_ptr<cz::spas::Service::Work> m_keepAliveWork;
	std::vector<std::shared_ptr<Session<Local, Remote>>> m_clients;
};

//! Helper class to run a Service in a separate thread.
struct ServiceThread
{
	spas::Service service;
	std::thread th;
	bool autoStop = false;
	std::unique_ptr<cz::spas::Service::Work> keepAliveWork;

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
			if (keepAlive)
				keepAliveWork = std::make_unique<spas::Service::Work>(service);
			service.run();
		});

		return *this;
	}

	void finish()
	{
		keepAliveWork = nullptr;
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
	std::promise<int> clientCallRes; // So the unit test can wait on the future to make sure the server got the reply from the client
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
	auto con = cz::rpc::Connection<Tester, TesterClient>::getCurrent();
	CHECK(con != nullptr);
	//auto session = std::static_pointer_cast<Session<Tester, TesterClient>>(con->getSession());
	//CHECK(session != nullptr);
	auto pr = std::make_shared<std::promise<std::string>>();
	CZRPC_CALL(*con, clientAdd, 1, 2).async(
		[this, pr](Result<int> res)
	{
		CHECK(res.isException());
		pr->set_value(res.getException());
	});

	return pr->get_future();
}

template<typename Local, typename Remote>
std::shared_ptr<Session<Local, Remote>> createClientSession(cz::spas::Service& service)
{
	auto session = std::make_shared<Session<Local, Remote>>(service);
	auto ec = session->trp.connect(session, session->con, "127.0.0.1", TEST_PORT);
	CHECK_CZSPAS(ec);
	return session;
}

template<typename Local, typename Remote>
SessionWrapper<Local, Remote> createClientSessionWrapper(cz::spas::Service& service)
{
	auto session = createClientSession<Local,Remote>(service);
	return SessionWrapper<Local,Remote>(session);
}



//} // anonymous namespace
