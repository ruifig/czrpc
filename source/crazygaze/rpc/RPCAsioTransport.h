/************************************************************************
RPC Transport based on Asio
************************************************************************/

#pragma once

#if CZRPC_HAS_BOOST
	#define CZRPC_ASIO_ERROR_CODE boost::system::error_code
	#include <boost/asio.hpp>
#else
	#define ASIO_STANDALONE
	#include "asio.hpp"
	#define CZRPC_ASIO_ERROR_CODE asio::error_code
#endif

namespace cz
{

namespace rpc
{

#if CZRPC_HAS_BOOST
	namespace ASIO = ::boost::asio;
#else
	namespace ASIO = ::asio;
#endif

class BaseAsioTransport : public Transport
{
private:
	// A dummy struct, to force the users to use the create functions, since the transport needs
	// to be created in the heap and tracked by std::shared_ptr
	struct ConstructorCookie { };

	std::shared_ptr<BaseAsioTransport> shared_from_this()
	{
		return std::static_pointer_cast<BaseAsioTransport>(Transport::shared_from_this());
	}
public:

	virtual ~BaseAsioTransport()
	{
	}

	BaseAsioTransport(ConstructorCookie, ASIO::io_service& io)
        : m_io(io)
        , m_conPendingProcess(false)
	{
	}

	ASIO::ip::tcp::endpoint getLocalEndpoint()
	{
		return m_s->local_endpoint();
	}

	ASIO::ip::tcp::endpoint getRemoteEndpoint()
	{
		return m_s->remote_endpoint();
	}

	ASIO::io_service& getIOService()
	{
		return m_io;
	}

	virtual void send(std::vector<char> data) override
	{
		if (m_closed)
			return;

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
			triggerSend();
	}

	virtual bool receive(std::vector<char>& dst) override
	{
		if (m_closed)
			return false;

		return m_in([&dst](In& in) -> bool
		{
			if (in.q.size() == 0)
			{
				dst.clear();
				return true;
			}
			else
			{
				dst = std::move(in.q.front());
				in.q.pop();
				return true;
			}
		});
	}

	// This only causes the asio sockets to close, which in turn will cause our asio handlers
	// to fail, therefore signaling our close cleanup code (to abort RPC replies)
	virtual void close() override
	{
		m_io.post([this, con=m_con.lock()]
		{
			if (m_s && m_s->is_open())
			{
				m_s->shutdown(ASIO::ip::tcp::socket::shutdown_both);
				m_s->close();
			}
		});
	}

	void connect(const char* ip, int port, std::function<void(bool)> callback)
	{
		//printf("Client side transport = this=%p, ms=%p\n", this, m_s.get());
		m_s = std::make_shared<ASIO::ip::tcp::socket>(m_io);
		ASIO::ip::tcp::endpoint point(ASIO::ip::address::from_string(ip), port);
		m_s->async_connect(
			point, [this_=shared_from_this(), callback=std::move(callback)](const CZRPC_ASIO_ERROR_CODE& ec)
		{
			callback(ec ? false : true);
			if (!ec)
				this_->startReadSize();
		});

	}

protected:

	template<typename LOCAL, typename REMOTE>
	static std::future<std::shared_ptr<Connection<LOCAL, REMOTE>>>
		createImpl(ASIO::io_service& io, LOCAL* localObj, const char* ip, int port)
	{
		auto trp = std::make_shared<BaseAsioTransport>(ConstructorCookie(), io);
		auto pr = std::make_shared<std::promise<std::shared_ptr<Connection<LOCAL, REMOTE>>>>();
		trp->connect(ip, port, [pr, trp, localObj](bool result)
		{
			if (result)
			{
				auto con = std::make_shared<Connection<LOCAL, REMOTE>>(localObj, trp);
				trp->m_con = con;
				con->setOutSignal([trp = trp.get()]()
				{
					trp->triggerConProcessing(false);
				});
				pr->set_value(std::move(con));
			}
			else
			{
				pr->set_value(nullptr);
			}
		});

		return pr->get_future();
	}

	template<typename, typename> friend class AsioTransportAcceptor;
	std::shared_ptr<ASIO::ip::tcp::socket> m_s;
	ASIO::io_service& m_io;

	bool m_closed = false;
	std::weak_ptr<BaseConnection> m_con;

	struct Out
	{
		bool ongoingWrite = false;
		std::queue<std::vector<char>> q;
	};
	Monitor<Out> m_out;

	struct In
	{
		std::queue<std::vector<char>> q;
	};
	Monitor<In> m_in;
	// Holds the next incoming RPC data
	std::vector<char> m_incoming;
	// Hold the currently outgoing RPC data
	std::vector<char> m_outgoing;

	std::atomic<bool> m_conPendingProcess;

	void onClosed()
	{
		if (m_closed)
			return;
		m_closed = true;
		// Trigger a call to the Connection processing, so the application can detect
		// the transport has been closed
		triggerConProcessing(false);
	}

	void startReadSize()
	{
		assert(m_incoming.size() == 0);
		m_incoming.insert(m_incoming.end(), 4, 0);
		ASIO::async_read(
			*m_s, ASIO::buffer(&m_incoming[0], sizeof(uint32_t)),
			[this, con = m_con.lock()](const CZRPC_ASIO_ERROR_CODE& ec, std::size_t bytesTransfered)
		{
			if (ec)
			{
				onClosed();
				return;
			}

			startReadData();
		});
	}

	void startReadData()
	{
		// reserve space for the rest of the rpc data
		auto rpcSize = *reinterpret_cast<uint32_t*>(&m_incoming[0]);
		m_incoming.insert(m_incoming.end(), rpcSize - 4, 0);
		ASIO::async_read(
			*m_s, ASIO::buffer(&m_incoming[4], rpcSize - 4),
			[this, con = m_con.lock()](const CZRPC_ASIO_ERROR_CODE& ec, std::size_t bytesTransfered)
		{
			if (ec)
			{
				onClosed();
				return;
			}
			assert(bytesTransfered == m_incoming.size() - 4);
			m_in([this](In& in)
			{
				in.q.push(std::move(m_incoming));
			});
			startReadSize();
			triggerConProcessing(false);
		});
	}

	void triggerSend()
	{
		ASIO::async_write(
			*m_s, ASIO::buffer(m_outgoing),
			[this, con = m_con.lock()](const CZRPC_ASIO_ERROR_CODE& ec, std::size_t bytesTransfered)
		{
			handleAsyncWrite(ec, bytesTransfered);
		});
	}

	void handleAsyncWrite(const CZRPC_ASIO_ERROR_CODE& ec, std::size_t bytesTransfered)
	{
		if (ec)
		{
			onClosed();
			return;
		}
		assert(bytesTransfered == m_outgoing.size());

		m_out([&](Out& out)
		{
			assert(out.ongoingWrite);
			if (out.q.size())
			{
				m_outgoing  = std::move(out.q.front());
				out.q.pop();
			}
			else
			{
				out.ongoingWrite = false;
				m_outgoing.clear();
			}
		});

		if (m_outgoing.size())
			triggerSend();
	}

	void triggerConProcessing(bool allowDispatch)
	{
		// Checking for a pending process, and triggering one is not atomic, but that's ok.
		// What can happen is that we can end up with unnecessary extra calls to m_con->process(),
		// which is not perfect.
		if (!m_conPendingProcess)
		{
			m_conPendingProcess = true;
			if (allowDispatch)
			{
				m_io.dispatch([this, con = m_con.lock()]
				{
					m_conPendingProcess = false;
					con->process();
				});
			}
			else
			{
				m_io.post([this, con = m_con.lock()]
				{
					m_conPendingProcess = false;
					con->process();
				});
			}
		}
	}
};

template<typename LOCAL, typename REMOTE>
class AsioTransport : public BaseAsioTransport
{
public:
	static std::future<std::shared_ptr<Connection<LOCAL, REMOTE>>>
		create(ASIO::io_service& io, LOCAL& localObj, const char* ip, int port)

	{
		return createImpl<LOCAL, REMOTE>(io, &localObj, ip, port);
	}
};

template<typename REMOTE>
class AsioTransport<void, REMOTE> : public BaseAsioTransport
{
public:
	static std::future<std::shared_ptr<Connection<void, REMOTE>>>
		create(ASIO::io_service& io, const char* ip, int port)
	{
		return createImpl<void, REMOTE>(io, nullptr, ip, port);
	}
};

class BaseAsioTransportAcceptor
{
public:
	BaseAsioTransportAcceptor(ASIO::io_service& io) : m_io(io)
	{
	}
	virtual ~BaseAsioTransportAcceptor() {}
protected:
	ASIO::io_service& m_io;
	std::shared_ptr<ASIO::ip::tcp::acceptor> m_acceptor;
};



// Forward declaration
//template<typename LOCAL, typename REMOTE> class AsioTransportAcceptor;

//template<typename LOCAL, typename REMOTE>
//std::shared_ptr<AsioTransportAcceptor<LOCAL, REMOTE>> make_AsioTransportAcceptor(ASIO::io_service& io, LOCAL& obj);

template<typename LOCAL, typename REMOTE>
class AsioTransportAcceptor : public BaseAsioTransportAcceptor, public std::enable_shared_from_this<AsioTransportAcceptor<LOCAL,REMOTE>>
{
private:
	// A dummy struct, to force the users to use the create functions, since the transport needs
	// to be created in the heap and tracked by std::shared_ptr
	struct ConstructorCookie { };
public:
	using LocalType = LOCAL;
	using RemoteType = REMOTE;
	using ConnectionType = Connection<LocalType, RemoteType>;

	virtual ~AsioTransportAcceptor() {}

	AsioTransportAcceptor(ConstructorCookie, ASIO::io_service& io, LocalType& localObj)
		: BaseAsioTransportAcceptor(io)
		, m_localObj(localObj)
	{
	}

	void start(int port, std::function<void(std::shared_ptr<ConnectionType>)> newConnectionCallback)
	{
		m_newConnectionCallback = std::move(newConnectionCallback);
		ASIO::ip::tcp::endpoint point(ASIO::ip::tcp::v4(), port);
		m_acceptor = std::make_shared<ASIO::ip::tcp::acceptor>(m_io, point);
		setupAccept();
	}

	// We have a create static method, to enforce creating it as std::shared_ptr,
	static std::shared_ptr<AsioTransportAcceptor<LOCAL,REMOTE>> create(ASIO::io_service& io, LocalType& localObj)
	{
		return std::make_shared<AsioTransportAcceptor>(ConstructorCookie(), io, localObj);
	}

private:

	void setupAccept()
	{
		auto socket = std::make_shared<ASIO::ip::tcp::socket>(m_io);
		m_acceptor->async_accept(
			*socket,
			[this_ = this->shared_from_this(), socket](const CZRPC_ASIO_ERROR_CODE& ec)
		{
			this_->doAccept(ec, std::move(socket));
		});
	}

	void doAccept(const CZRPC_ASIO_ERROR_CODE& ec, std::shared_ptr<ASIO::ip::tcp::socket> socket)
	{
		if (ec)
			return;

		auto trp = std::make_shared<BaseAsioTransport>(BaseAsioTransport::ConstructorCookie(), m_io);
		trp->m_s = std::move(socket);
		trp->startReadSize();

		auto con = std::make_shared<ConnectionType>(&m_localObj, trp);
		trp->m_con = con;
		con->setOutSignal([trp=trp.get()]()
		{
			trp->triggerConProcessing(false);
		});

		if (m_newConnectionCallback)
			m_newConnectionCallback(std::move(con));
		setupAccept();
	}

	LocalType& m_localObj;
	std::function<void(std::shared_ptr<ConnectionType>)> m_newConnectionCallback;
};

}
}
