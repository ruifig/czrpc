#pragma once

#include "../../../czspas/source/crazygaze/spas/spas.h"
#include "RPCTransport.h"
#include "RPCConnection.h"
#include "RPCUtils.h"

namespace cz
{
namespace rpc
{

struct SpasTransportSession
{
};

class SpasTransport : public Transport
{
private:
	// Private so it forces creation in the heap through "connect"
	SpasTransport(spas::Service& service) : m_sock(service)
	{
	}

	virtual ~SpasTransport()
	{
	}

	template<typename H, typename = cz::spas::detail::IsConnectHandler<H> >
	void asyncConnect(BaseConnection& rpccon, const char* ip, int port, H&& h)
	{
		m_sock.asyncConnect(ip, port,[this, h = std::forward<H>(h)](const spas::Error& ec)
		{
			// If no error, then setup whatever we need
			if (!ec)
			{
				m_con = con;
				startReadData();
			}

			h(ec);
		});
	}

public:

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
		// #TODO : What to do here?
	}
	//
	// Transport interface END
	//

private:
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
	BaseConnection* m_con = nullptr;

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
		// #TODO
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
			m_con->process();
		});
	}

};

}
}
