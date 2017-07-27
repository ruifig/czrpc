#pragma once

#include "../../../czspas/source/crazygaze/spas/spas.h"
#include "RPCTransport.h"
#include "RPCConnection.h"
#include "RPCUtils.h"

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
	template<typename LOCAL, typename REMOTE, typename H,
		typename = cz::spas::detail::IsConnectHandler<H>,
		typename = std::enable_if<!std::is_void<LOCAL>::value>
		>
	void asyncConnect(Connection<LOCAL, REMOTE>& rpccon, LOCAL& localObj, const char* ip, int port, H&& h)
	{
		m_sock.asyncConnect(ip, port, 5000, [this, &rpccon, &localObj, h = std::forward<H>(h)](const spas::Error& ec)
		{
			// If no error, then setup whatever we need
			if (!ec)
			{
				rpccon.init(&localObj, *this);
				startReadSize();
			}

			h(ec);
		});
	}


	//! Version for Connection<void, REMOTE>
	template<typename LOCAL, typename REMOTE, typename H,
		typename = cz::spas::detail::IsConnectHandler<H>,
		typename = std::enable_if<std::is_void<LOCAL>::value>
		>
	void asyncConnect(Connection<LOCAL, REMOTE>& rpccon, const char* ip, int port, H&& h)
	{
		m_sock.asyncConnect(ip, port, 5000, [this, &rpccon, h = std::forward<H>(h)](const spas::Error& ec)
		{
			// If no error, then setup whatever we need
			if (!ec)
			{
				rpccon.init(nullptr, *this);
				startReadSize();
			}

			h(ec);
		});
	}

	//! Version for Connection<LOCAL, REMOTE>
	template<typename LOCAL, typename REMOTE, typename LOCAL_,
		typename = std::enable_if<!std::is_void<LOCAL>::value>
		>
	std::future<spas::Error> asyncConnect(Connection<LOCAL, REMOTE>& rpccon, LOCAL_& localObj, const char* ip, int port)
	{
		static_assert(std::is_base_of<LOCAL, LOCAL_>::value, "localObj doesn't implement the required LOCAL interface");
		LOCAL* obj = static_cast<LOCAL*>(&localObj);
		static_assert(!std::is_void<LOCAL>::value, "Specified RPC Connection doesn't have a local interface, so use the other asyncConnect function");
		// Create the std::promise as a shared_ptr, so we can pass it to the handler.
		auto pr = std::make_shared<std::promise<spas::Error>>();
		m_sock.asyncConnect(ip, port, 5000, [this, &rpccon, obj, pr](const spas::Error& ec)
		{
			// If no error, then setup whatever we need
			if (!ec)
			{
				rpccon.init(obj, *this);
				startReadSize();
			}

			pr->set_value(ec);
		});

		return pr->get_future();
	}

	//! Version for Connection<void, REMOTE>
	template<typename LOCAL, typename REMOTE,
		typename = std::enable_if<std::is_void<LOCAL>::value>
		>
	std::future<spas::Error> asyncConnect(Connection<LOCAL, REMOTE>& rpccon, const char* ip, int port)
	{
		static_assert(std::is_void<LOCAL>::value, "Specified RPC Connection has a local interface, so use the other asyncConnect to specify the local interface");
		// Create the std::promise as a shared_ptr, so we can pass it to the handler.
		auto pr = std::make_shared<std::promise<spas::Error>>();
		m_sock.asyncConnect(ip, port, 5000, [this, &rpccon, pr](const spas::Error& ec)
		{
			// If no error, then setup whatever we need
			if (!ec)
			{
				rpccon.init(nullptr, *this);
				startReadSize();
			}

			pr->set_value(ec);
		});

		return pr->get_future();
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
		// #TODO : Is this ok?
		m_sock.getService().post([this]()
		{
			m_sock.cancel();
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
			m_sock.getService().post([this]()
			{
				m_con->process(rpc::BaseConnection::Direction::Both);
				m_pendingConProcessCall.exchange(false);
			});
		}
	}

	void doSend()
	{
		spas::asyncSend(m_sock, m_outgoing.data(), m_outgoing.size(), [this](const spas::Error& ec, size_t transfered)
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
		if (m_closing)
			return;
		m_closing = true;
		checkConProcessCallTrigger();
		// #TODO ???
	}

	void startReadSize()
	{
		assert(m_incoming.size() == 0);
		m_incoming.insert(m_incoming.end(), 4, 0);
		spas::asyncReceive(m_sock, m_incoming.data(), 4, [&](const spas::Error& ec, size_t transfered)
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
		spas::asyncReceive(m_sock, &m_incoming[4], rpcSize - 4, [&](const spas::Error& ec, size_t transfered)
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
	template<typename LOCAL, typename REMOTE, typename H, typename = cz::spas::detail::IsConnectHandler<H>>
	void asyncAccept(SpasTransport& trp, Connection<LOCAL, REMOTE>& rpccon, LOCAL* localObj, H&& h)
	{
		m_acceptor.asyncAccept(trp.m_sock, [&trp, &rpccon, localObj, h](const spas::Error& ec)
		{
			// If no errors, then setup transport and rpc connection
			if (!ec)
			{
				rpccon.init(localObj, trp);
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
