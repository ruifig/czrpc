#pragma once

#include "../../../czspas/source/crazygaze/spas/spas.h"
#include "RPCTransport.h"
#include "RPCConnection.h"
#include "RPCUtils.h"

#define CHECK_CZSPAS_EQUAL(expected, ec)                                                                      \
	if ((ec.code) != (Error::Code::expected))                                                                 \
	{                                                                                                         \
		UnitTest::CheckEqual(*UnitTest::CurrentTest::Results(), Error(Error::Code::expected).msg(), ec.msg(), \
		                     UnitTest::TestDetails(*UnitTest::CurrentTest::Details(), __LINE__));             \
	}
#define CHECK_CZSPAS(ec) CHECK_CZSPAS_EQUAL(Success, ec)

namespace cz
{
namespace rpc
{

class SpasTransport : public Transport
{
public:
	SpasTransport(spas::Service& service)
		: m_sock(service)
		, m_pendingConProcessCall(false)
	{
	}

	virtual ~SpasTransport()
	{
	}

	//! Version for Connection<LOCAL, REMOTE>
	template<
		typename LOCAL, typename REMOTE, typename LOCAL_, typename H,
		typename = std::enable_if_t<std::is_base_of<LOCAL, LOCAL_>::value>,
		typename = cz::spas::detail::IsConnectHandler<H>
		>
	void asyncConnect(
		std::shared_ptr<SessionData> session,
		Connection<LOCAL, REMOTE>& rpccon, LOCAL_& localObj,
		const char* ip, int port, H&& h)
	{
		static_assert(!std::is_void<LOCAL>::value, "Specified RPC Connection doesn't have a local interface, so use the other asyncConnect function");
		static_assert(std::is_base_of<LOCAL, LOCAL_>::value, "localObj doesn't implement the required LOCAL interface");

		LOCAL* rpcObj = static_cast<LOCAL*>(&localObj);
		m_sock.asyncConnect(ip, port, 5000,
			[this, &rpccon, rpcObj, session=std::move(session), h = std::forward<H>(h)](const spas::Error& ec)
		{
			// If no error, then setup whatever we need
			if (!ec)
			{
				rpccon.init(rpcObj, *this, session);
				startReadSize();
			}

			h(ec);
		});
	}


	//! Version for Connection<void, REMOTE>
	template<
		typename LOCAL, typename REMOTE, typename H,
		typename = std::enable_if_t<std::is_void<LOCAL>::value>,
		typename = cz::spas::detail::IsConnectHandler<H>
		>
	void asyncConnect(std::shared_ptr<SessionData> session, Connection<LOCAL, REMOTE>& rpccon, const char* ip, int port, H&& h)
	{
		static_assert(std::is_void<LOCAL>::value, "Specified RPC Connection has a local interface, so use the other asyncConnect");

		// Create the std::promise as a shared_ptr, so we can pass it to the handler.
		m_sock.asyncConnect(ip, port, 5000, [this, &rpccon, session=std::move(session), h = std::forward<H>(h)](const spas::Error& ec)
		{
			// If no error, then setup whatever we need
			if (!ec)
			{
				rpccon.init(nullptr, *this, session);
				startReadSize();
			}

			h(ec);
		});
	}

	//! Version for Connection<LOCAL, REMOTE>
	template<
		typename LOCAL, typename REMOTE, typename LOCAL_,
		typename = std::enable_if_t<std::is_base_of<LOCAL, LOCAL_>::value>
		>
	std::future<spas::Error> asyncConnect(std::shared_ptr<SessionData> session, Connection<LOCAL, REMOTE>& rpccon, LOCAL_& localObj, const char* ip, int port)
	{
		static_assert(!std::is_void<LOCAL>::value, "Specified RPC Connection doesn't have a local interface, so use the other asyncConnect function");
		static_assert(std::is_base_of<LOCAL, LOCAL_>::value, "localObj doesn't implement the required LOCAL interface");

		// Create the std::promise as a shared_ptr, so we can copy it around
		auto pr = std::make_shared<std::promise<spas::Error>>();
		asyncConnect(std::move(session), rpccon, localObj, ip, port, [pr](const spas::Error& ec)
		{
			pr->set_value(ec);
		});
		return pr->get_future();
	}

	//! Version for Connection<void, REMOTE>
	template<
		typename LOCAL, typename REMOTE,
		typename = std::enable_if_t<std::is_void<LOCAL>::value>
		>
	std::future<spas::Error> asyncConnect(std::shared_ptr<SessionData> session, Connection<LOCAL, REMOTE>& rpccon, const char* ip, int port)
	{
		static_assert(std::is_void<LOCAL>::value, "Specified RPC Connection has a local interface, so use the other asyncConnect");

		// Create the std::promise as a shared_ptr, so we can copy it around
		auto pr = std::make_shared<std::promise<spas::Error>>();
		asyncConnect(std::move(session), rpccon, ip, port, [pr](const spas::Error& ec)
		{
			pr->set_value(ec);
		});
		return pr->get_future();
	}

	//! Synchronous connect.
	// Version for Connection<LOCAL, REMOTE>
	// This doesn't need to have the Service running to make the connection.
	template<
		typename LOCAL, typename REMOTE, typename LOCAL_,
		typename = std::enable_if_t<std::is_base_of<LOCAL, LOCAL_>::value>
		>
	spas::Error connect(std::shared_ptr<SessionData> session, Connection<LOCAL, REMOTE>& rpccon, LOCAL_& localObj, const char* ip, int port)
	{
		static_assert(!std::is_void<LOCAL>::value, "Specified RPC Connection doesn't have a local interface, so use the other asyncConnect function");
		static_assert(std::is_base_of<LOCAL, LOCAL_>::value, "localObj doesn't implement the required LOCAL interface");
		auto ec = m_sock.connect(ip, port);
		if (ec)
			return ec;

		LOCAL* rpcObj = static_cast<LOCAL*>(&localObj);
		rpccon.init(rpcObj, *this, std::move(session));
		startReadSize();
		return ec;
	}

	//! Synchronous connect.
	// Version for Connection<void, REMOTE>
	// This doesn't need to have the Service running to make the connection.
	template<
		typename LOCAL, typename REMOTE,
		typename = std::enable_if_t<std::is_void<LOCAL>::value>
		>
	spas::Error connect(std::shared_ptr<SessionData> session, Connection<LOCAL, REMOTE>& rpccon, const char* ip, int port)
	{
		static_assert(std::is_void<LOCAL>::value, "Specified RPC Connection has a local interface, so use the other asyncConnect");
		auto ec = m_sock.connect(ip, port);
		if (ec)
			return ec;

		rpccon.init(nullptr, *this, std::move(session));
		startReadSize();
		return ec;
	}

	const std::pair<std::string, int>& getLocalAddr() const
	{
		return m_sock.getLocalAddr();
	}

	const std::pair<std::string, int>& getPeerAddr() const
	{
		return m_sock.getPeerAddr();
	}

private:

	//
	// Transport interface BEGIN
	//
	virtual bool send(std::vector<char> data) override
	{
		if (m_closing)
			return false;

		auto trigger = m_out([&](Out& out)
		{
			if (out.ongoingWrite)
			{
				out.q.push(std::move(data));
				return false;
			}
			else
			{
				assert(out.q.size() == 0);
				out.ongoingWrite = true;
				m_outgoing = std::move(data);
				return true;
			}
		});

		if (trigger)
			doSend();
		return true;
	}

	virtual bool receive(std::vector<char>& dst) override
	{
		if (m_closing)
			return false;

		m_in([&](In& in)
		{
			if (in.q.size() == 0)
			{
				dst.clear();
			}
			else
			{
				dst = std::move(in.q.front());
				in.q.pop();
			}
		});

		return true;
	}

	virtual void close() override
	{
		// #TODO : Is this ok? Not sure m_closing should be outside, since the RPC connection might have calls pending,
		// which will be aborted. Need to think what should be the best design
		m_closing = true;
		m_sock.getService().post([this, session = m_con->getSession()]()
		{
			m_sock.close();
		});
	}

	//
	// Transport interface END
	//

private:
	friend class SpasTransportAcceptor;
	bool m_closing = false;
	spas::Socket m_sock;
	struct Out
	{
		bool ongoingWrite = false;
		std::queue<std::vector<char>> q;
	};
	struct In
	{
		std::queue<std::vector<char>> q;
	};
	Monitor<Out> m_out;
	Monitor<In> m_in;
	std::vector<char> m_outgoing;
	std::vector<char> m_incoming;
	std::atomic<bool> m_pendingConProcessCall;

	virtual void onSendReady() override
	{
		checkConProcessCallTrigger();
	}

	void checkConProcessCallTrigger()
	{
		// Check if we need to trigger a con->process call
		// #TODO : If I ever add functionality to spas to detect if Service::run is running on this thread, 
		// then add an optimization here to call con->process in-place, instead of using post.
		// Another option is, if I add that functionality to Service, then most certainly Service will have a
		// "dispatch" method (which executes right away is Service::run is running on this thread). I can simply use
		// that method
		if (m_pendingConProcessCall.exchange(true) == false)
		{
			m_sock.getService().post([this, session = m_con->getSession()]()
			{
				m_con->process(rpc::BaseConnection::Direction::Both);
				m_pendingConProcessCall.exchange(false);
			});
		}
	}

	void doSend()
	{
		spas::asyncSend(m_sock, m_outgoing.data(), m_outgoing.size(),
			[this, session = m_con->getSession()](const spas::Error& ec, size_t transfered)
		{
			handleSend(ec, transfered);
		});
	}

	void handleSend(const spas::Error& ec, size_t transfered)
	{
		if (ec)
		{
			onClosing();
			return;
		}

		assert(transfered == m_outgoing.size());
		m_out([&](Out& out)
		{
			assert(out.ongoingWrite);
			if (out.q.size())
			{
				m_outgoing = std::move(out.q.front());
				out.q.pop();
			}
			else
			{
				out.ongoingWrite = false;
				m_outgoing.clear();
			}
		});

		if (m_outgoing.size())
			doSend();
	}

	void onClosing()
	{
		m_closing = true;
		checkConProcessCallTrigger();
	}

	void startReadSize()
	{
		assert(m_incoming.size() == 0);
		m_incoming.insert(m_incoming.end(), 4, 0);
		spas::asyncReceive(m_sock, m_incoming.data(), 4,
			[this, session = m_con->getSession()](const spas::Error& ec, size_t transfered)
		{
			if (ec)
			{
				onClosing();
				return;
			}

			startReadData();
		});
	}

	void startReadData()
	{
		auto rpcSize = *reinterpret_cast<uint32_t*>(&m_incoming[0]);
		m_incoming.insert(m_incoming.end(), rpcSize - 4, 0);
		spas::asyncReceive(m_sock, &m_incoming[4], rpcSize - 4,
			[this, session = m_con->getSession()](const spas::Error& ec, size_t transfered)
		{
			if (ec)
			{
				onClosing();
				return;
			}

			assert(transfered == m_incoming.size() - 4);
			m_in([this](In& in)
			{
				in.q.push(std::move(m_incoming));
			});
			startReadSize();
			checkConProcessCallTrigger();
		});
	}

};


//template<typename LOCAL, typename REMOTE>
class SpasTransportAcceptor
{
public:
#if 0
	using LocalType = LOCAL;
	using RemoteType = REMOTE;
	using ConnectionType = Connection<LocalType, RemoteType>;
#endif

	SpasTransportAcceptor(spas::Service& io)
		: m_acceptor(io)
	{
	}

	virtual ~SpasTransportAcceptor()
	{
	}

	void cancel()
	{
		m_acceptor.cancel();
	}

	spas::Error listen(const char* bindIP, int port, int backlog, bool reuseAddr)
	{
		return m_acceptor.listen(bindIP, port, backlog, reuseAddr);
	}

	spas::Error listen(int port)
	{
		return m_acceptor.listen(port);
	}

#if 0
	template< typename H, typename = spas::detail::IsConnectHandler<H> >
	void asyncAccept(SpasTransport& trp, H&& h)
	{
		m_acceptor.asyncAccept(trp.m_sock, std::forward<H>(h));
	}
#endif

	//! Version for Connection<LOCAL, REMOTE>
	template<typename LOCAL, typename REMOTE, typename LOCAL_, typename H,
		typename = cz::spas::detail::IsConnectHandler<H>>
	void asyncAccept(std::shared_ptr<SessionData> session, SpasTransport& trp, Connection<LOCAL, REMOTE>& rpccon, LOCAL_& localObj, H&& h)
	{
		static_assert(std::is_base_of<LOCAL, LOCAL_>::value, "localObj doesn't implement the required LOCAL interface");

		LOCAL* rpcObj = static_cast<LOCAL*>(&localObj);
		m_acceptor.asyncAccept(trp.m_sock,
			[&trp, &rpccon, rpcObj, session = std::move(session), h=std::move(h)](const spas::Error& ec)
		{
			// If no errors, then setup transport and rpc connection
			if (!ec)
			{
				rpccon.init(rpcObj, trp, session);
				trp.startReadSize();
			}
			h(ec);
		});
	}

private:

	spas::Acceptor m_acceptor;
};

}
}
