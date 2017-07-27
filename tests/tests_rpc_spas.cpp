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

	template<typename LOCAL, typename REMOTE>
	class ServerSession
	{
	public:
		using Local = LOCAL;
		using Remote = REMOTE;

		explicit ServerSession(int port)
			: m_objData(&m_servedObj)
			, m_acceptor(m_io)
		{
			// #TODO : Set auth token on m_objData
			m_th = std::thread([this]
			{
				m_io.run();
				printf("");
			});
		}

		~ServerSession()
		{
			m_io.stop();
			m_th.join();
		}

		void start(int port)
		{
			m_acceptor.listen(port);
			setupAccept();
		}

	private:

		void setupAccept()
		{
			auto contrp = std::make_shared<ConTrp<Local, Remote>>(m_io);
			m_acceptor.asyncAccept(contrp->trp, contrp->con, m_servedObj, [this, contrp](const spas::Error& ec)
			{
				if (ec)
				{
					CZRPC_LOG(Log, "Failed to accept connection: %s", ec.msg());
					return;
				}
				m_cons.push_back(contrp);
				setupAccept();
			});
		}

		Local m_servedObj;
		ObjectData m_objData;
		spas::Service m_io;
		std::thread m_th;
		SpasTransportAcceptor m_acceptor;
		std::vector<std::shared_ptr<ConTrp<Local, Remote>>> m_cons;
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

TEST(1)
{
	ServerSession<CalcTest, void> serverSession(TEST_PORT);
	serverSession.start(TEST_PORT);

	spas::Service io;
	std::thread iothread = std::thread([&io]
	{
		io.run();
		printf("");
	});

	ConTrp<void, CalcTest> clientSession(io);
	//bool res = doConnect(clientSession, "127.0.0.1", TEST_PORT);
	auto ec = clientSession.trp.asyncConnect(clientSession.con, "127.0.0.1", TEST_PORT).get();
	CHECK(!ec);

	Semaphore sem;
	CZRPC_CALL(clientSession.con, add, 1, 2).async([&sem](Result<int> res)
	{
		CHECK_EQUAL(3, res.get());
		sem.notify();
	});
	sem.wait();

	io.stop();
	iothread.join();
}

}
