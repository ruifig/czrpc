#include "testsPCH.h"

#define TEST_PORT 9000

using namespace cz;
using namespace cz::rpc;


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

	//
	// Utility class just to make the unit tests shorter, by allowing tweaks on how to handle spas::Service, and
	// SpasTransport/SpasTransportAcceptor
	//
	template<typename LOCAL, typename REMOTE>
	class ServerSession
	{
	public:
		using Local = LOCAL;
		using Remote = REMOTE;

		explicit ServerSession(int port, bool autoRun, bool autoAccept, bool keepAlive, bool doStop)
			: m_objData(&m_servedObj)
			, m_acceptor(m_io)
			, m_port(port)
			, m_keepAlive(keepAlive)
			, m_doStop(doStop)
		{
			if (autoAccept)
				startAccept();

			if (autoRun)
				run();
		}

		~ServerSession()
		{
			finish();
		}

		void run()
		{
			// #TODO : Set auth token on m_objData
			m_th = std::thread([this]
			{
				// If required, add the dummy Work to the service, so that Service::run doesn't exit immediately
				std::unique_ptr<spas::Service::Work> work;
				if (m_keepAlive)
					work = std::make_unique<spas::Service::Work>(m_io);
				m_io.run();
			});

		}

		void startAccept()
		{
			m_acceptor.listen(m_port);
			setupAccept();
		}

		void finish()
		{
			if (m_doStop)
				m_io.stop();
			if (m_th.joinable())
				m_th.join();
		}

	private:

		void setupAccept()
		{
			auto contrp = std::make_shared<ConTrp<Local, Remote>>(m_io);
			m_acceptor.asyncAccept(contrp->trp, contrp->con, m_servedObj, [this, contrp](const spas::Error& ec)
			{
				if (ec)
				{
					if (ec.code == spas::Error::Code::Cancelled)
						return;
					CZRPC_LOG(Log, "Failed to accept connection: %s", ec.msg());
				}
				else
				{
					m_cons.push_back(contrp);
				}
				setupAccept();
			});
		}

		Local m_servedObj;
		ObjectData m_objData;
		spas::Service m_io;
		std::thread m_th;
		SpasTransportAcceptor m_acceptor;
		int m_port;
		bool m_keepAlive;
		bool m_doStop;
		std::vector<std::shared_ptr<ConTrp<Local, Remote>>> m_cons;
	};
	
	//! Helper class to run a Service in a separate thread.
	struct ServiceThread
	{
		spas::Service service;
		std::thread th;
		bool doStop = false;
		bool keepAlive = false;
		explicit ServiceThread(bool autoRun, bool keepAlive, bool doStop)
			: doStop(doStop)
			, keepAlive(keepAlive)
		{
			if (autoRun)
				run();
		}

		~ServiceThread()
		{
			finish();
		}

		void run()
		{
			CHECK(th.joinable() == false);
			th = std::thread([this]()
			{
				//UnitTest::TimeHelpers::SleepMs(500);
				std::unique_ptr<spas::Service::Work> work;
				if (keepAlive)
					work = std::make_unique<spas::Service::Work>(service);
				service.run();
			});
		}

		void finish()
		{
			if (doStop)
				service.stop();
			if (th.joinable())
				th.join();
		}

};

}


#define RPCTABLE_CLASS CalcTest
#define RPCTABLE_CONTENTS \
	REGISTERRPC(add)
#include "crazygaze/rpc/RPCGenerate.h"


template<typename LOCAL, typename REMOTE>
bool doConnect(ConTrp<LOCAL, REMOTE>& contrp, const char* ip, int port)
{
	Semaphore sem;
	spas::Error ec;
	contrp.trp.asyncConnect(contrp.con, ip, port, [&sem, &ec](const spas::Error& ec_)
	{
		ec = ec_;
		sem.notify();
	});

	sem.wait();
	if (ec)
		return false;
	else
		return true;
}

SUITE(RPC_SPAS)
{

// Does nothing. Just to check if there is anything blocking/crashing in this case
TEST(SpasTransport_Nothing)
{
	spas::Service ioservice;
	SpasTransportAcceptor acceptor(ioservice);
	SpasTransport trp(ioservice);
}

TEST(SpasTransport_Accept_cancel)
{
	spas::Service ioservice;
}


#if 1

TEST(1)
{
	ServerSession<CalcTest, void> serverSession(TEST_PORT, true, true, true, true);

	ServiceThread ioth(true, true, true);

	ConTrp<void, CalcTest> clientSession(ioth.service);
	auto ec = clientSession.trp.asyncConnect(clientSession.con, "127.0.0.1", TEST_PORT).get();
	CHECK(!ec);

	Semaphore sem;
	CZRPC_CALL(clientSession.con, add, 1, 2).async([&sem](Result<int> res)
	{
		CHECK_EQUAL(3, res.get());
		sem.notify();
	});
	sem.wait();
}

#endif

}

