#pragma once

#include "crazygaze/rpc/RPCTCPSocket.h"

namespace cz
{
namespace rpc
{


class BaseTCPTransport : public Transport
{
public:
	BaseTCPTransport(TCPService& service)
		: m_sock(service)
	{
	}

	virtual ~BaseTCPTransport()
	{
	}

	virtual void send(std::vector<char> data) override
	{
		auto d = std::make_shared<std::vector<char>>(std::move(data));
		m_sock.asyncWrite(d->data(), d->size(), [d, this](const TCPError& ec, int bytesTransfered)
		{
			if (ec)
			{
				onClosed();
				return;
			}
			assert((size_t)bytesTransfered == d->size());
		});
	}

	virtual bool receive(std::vector<char>& dst) override
	{
		if (m_incoming.size() == 0)
			return false;
		dst = std::move(m_incoming);
		return true;
	}

	virtual void close() override
	{
		m_sock.asyncClose(nullptr);
	}

protected:
	template<typename, typename> friend class TCPTransportAcceptor;

	void onClosed()
	{
		m_rpcCon->process();
	}

	void startReadSize()
	{
		assert(m_incoming.size() == 0);
		const auto headerSize = sizeof(uint32_t);
		m_incoming.insert(m_incoming.end(), headerSize, 0);
		m_sock.asyncRead(m_incoming.data(), headerSize, [this](const TCPError& ec, int bytesTransfered)
		{
			if (ec)
			{
				onClosed();
				return;
			}

			assert((size_t)bytesTransfered == sizeof(uint32_t));
			startReadData();
		});
	}

	void startReadData()
	{
		const int rpcSize = *reinterpret_cast<uint32_t*>(m_incoming.data());
		// To received "rpcSize" is the total size of the data, so the remaining iss rpcSize - sizeof(uint32_t)
		const int remaining = rpcSize - sizeof(uint32_t);
		m_incoming.insert(m_incoming.end(), remaining, 0);
		m_sock.asyncRead(m_incoming.data() + sizeof(uint32_t), remaining,

			[this](const TCPError& ec, int bytesTransfered)
		{
			if (ec)
			{
				onClosed();
				return;
			}

			assert(bytesTransfered == m_incoming.size() - sizeof(uint32_t));
			m_rpcCon->process(BaseConnection::Direction::In);
			startReadSize();
		});
	}


	template<typename LOCAL, typename REMOTE>
	static std::future<std::shared_ptr<Connection<LOCAL,REMOTE>>>
		createImpl(TCPService& service, LOCAL* localObj, const char* ip, int port)
	{
		auto trp = std::make_shared<BaseTCPTransport>(service);
		auto pr = std::make_shared<std::promise<std::shared_ptr<Connection<LOCAL,REMOTE>>>>();
		trp->m_sock.asyncConnect(ip, port, [pr, trp, localObj](const TCPError& ec)
		{
			if (ec)
			{
				pr->set_value(nullptr);
				return;
			}

			auto con = std::make_shared<Connection<LOCAL,REMOTE>>(localObj, trp);
			trp->m_rpcCon = con.get();
			con->setOutSignal([con=con.get()]()
			{
				con->process(BaseConnection::Direction::Out);
			});
			trp->startReadSize();
			pr->set_value(std::move(con));
		});

		return pr->get_future();
	}

	TCPSocket m_sock;
	BaseConnection* m_rpcCon = nullptr;

	// Holds the next incoming RPC data
	std::vector<char> m_incoming;
};


template<typename LOCAL, typename REMOTE>
class TCPTransport : public BaseTCPTransport
{
public:
	static std::future<std::shared_ptr<Connection<LOCAL,REMOTE>>>
		create(TCPService& service, LOCAL& localObj, const char* ip, int port)
	{
		return createImpl<LOCAL,REMOTE>(service, &localObj, ip, port);
	}
};

template<typename REMOTE>
class TCPTransport<void,REMOTE> : public BaseTCPTransport
{
public:
	static std::future<std::shared_ptr<Connection<void, REMOTE>>>
		create(TCPService& service, const char* ip, int port)
	{
		return createImpl<void,REMOTE>(service, nullptr, ip, port);
	}
};


class BaseTCPTransportAcceptor
{
public:
	BaseTCPTransportAcceptor(TCPService& service)
		: m_acceptor(service)
	{}

	virtual ~BaseTCPTransportAcceptor()
	{
	}

protected:
	TCPAcceptor m_acceptor;
};

template<typename LOCAL, typename REMOTE>
class TCPTransportAcceptor : public BaseTCPTransportAcceptor
{
public:
	using LocalType = LOCAL;
	using RemoteType = REMOTE;
	using ConnectionType = Connection<LocalType, RemoteType>;

	TCPTransportAcceptor(TCPService& service, LocalType& localObj)
		: BaseTCPTransportAcceptor(service)
		, m_localObj(localObj)
	{
	}

	bool start(int port, std::function<void(std::shared_ptr<ConnectionType>)> newConnectionCallback)
	{
		auto ec = m_acceptor.listen(port, 200);
		if (ec)
			return false;
		m_newConnectionCallback = std::move(newConnectionCallback);
		setupAccept();
		return true;
	}

	void setupAccept()
	{
		auto trp = std::make_shared<BaseTCPTransport>(TCPService::getFrom(m_acceptor));
		m_acceptor.asyncAccept(trp->m_sock, [this, trp](const TCPError& ec)
		{
			doAccept(ec, std::move(trp));
		});
	}

private:

	void doAccept(const TCPError& ec, std::shared_ptr<BaseTCPTransport> trp)
	{
		if (ec)
			return;

		trp->startReadSize();
		auto con = std::make_shared<ConnectionType>(&m_localObj, trp);
		trp->m_rpcCon = con.get();
		con->setOutSignal([con=con.get()]
		{
			con->process(BaseConnection::Direction::Out);
		});

		if (m_newConnectionCallback)
			m_newConnectionCallback(std::move(con));
	}

	LocalType& m_localObj;
	std::function<void(std::shared_ptr<ConnectionType>)>  m_newConnectionCallback;
};


} // namespace rpc
} // namespace cz

