#pragma once

#include "crazygaze/rpc/RPC.h"
#include "crazygaze/rpc/RPCTCPSocket.h"

#ifdef _WIN32
	#define TRPLOG(fmt, ...) \
		printf(TRPLOG_CONCAT("TRPLOG th=%ld, %s:%d: ", fmt, "\n"), (long)GetCurrentThreadId(), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
	#include <sys/types.h>
	#include <sys/syscall.h>
	#define TRPLOG(fmt, ...) \
		printf(TRPLOG_CONCAT("TRPLOG th=%ld, %s:%d: ", fmt, "\n"), (long)syscall(SYS_gettid), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#define TRPLOG_CONCAT(a,b,c) a ## b ## c

#undef TRPLOG
#define TRPLOG(...) (void(0))

namespace cz
{
namespace rpc
{

struct TCPServiceThread
{
	TCPServiceThread()
	{
		th = std::thread([this]
		{
			io.run();
		});
	}

	~TCPServiceThread()
	{
		stop();
	}

	void stop()
	{
		if (th.joinable())
		{
			io.stop();
			th.join();
		}
	}

	cz::rpc::TCPService io;
	std::thread th;
};

namespace ThreadEnforcer
{
	class None
	{
	public:
		void lock() {}
		void unlock() {}
	};

	class Serialize
	{
	public:
		static void doBreak()
		{
#ifdef _WIN32
			__debugbreak();
#else
			__builtin_trap();
#endif
		}

		void lock()
		{
			if (!m_mtx.try_lock())
			{
				// If it breaks here, then there are multiple threads accessing this object at this moment
				doBreak();
			}
		}

		void unlock()
		{
			m_mtx.unlock();
		}
	private:
		std::recursive_mutex m_mtx;
	};

	class Affinity
	{
	public:
		void lock()
		{
			if (!m_mtx.try_lock())
			{
				// If it breaks here, then there are multiple threads accessing this object at this moment
				Serialize::doBreak();
			}

			if (
				m_lastThreadId != std::thread::id() &&
				std::this_thread::get_id() != m_lastThreadId)
			{
				// If if breaks here, then this object was access from different threads at some point, even if not
				// concurrently
				Serialize::doBreak();
			}

			m_lastThreadId = std::this_thread::get_id();
		}

		void unlock()
		{
			m_mtx.unlock();
		}

	private:
		std::recursive_mutex m_mtx;
		std::thread::id m_lastThreadId;
	};
}

template<typename T>
struct SingleThreadEnforcerLock
{
	SingleThreadEnforcerLock(T& outer) : outer(outer) { outer.lock(); }
	~SingleThreadEnforcerLock() { outer.unlock(); }
	T& outer;
};

#ifndef NDEBUG
	#define SINGLETHREAD_ENFORCE() \
		auto threadEnforcerLock_ = SingleThreadEnforcerLock<decltype(threadEnforcer_)>(threadEnforcer_)
	#define DECLARE_THREADENFORCER_NONE \
		cz::rpc::ThreadEnforcer::None threadEnforcer_
	#define DECLARE_THREADENFORCER_SERIALIZE \
		cz::rpc::ThreadEnforcer::Serialize threadEnforcer_
	#define DECLARE_THREADENFORCER_AFFINITY \
		cz::rpc::ThreadEnforcer::Affinity threadEnforcer_
#else
	#define SINGLETHREAD_ENFORCE() (void(0))
	#define DECLARE_THREADENFORCER_NONE
	#define DECLARE_THREADENFORCER_SERIALIZE
	#define DECLARE_THREADENFORCER_AFFINITY
#endif

class BaseTCPTransport : public Transport
{
public:
	BaseTCPTransport(TCPService& service, std::shared_ptr<TCPServiceThread> iothread=nullptr)
		: m_sock(service)
		, m_iothread(iothread)

	{
		CZRPC_ASSERT(m_iothread==nullptr || (&m_iothread->io == &service));
		TRPLOG("%p", this);
	}

	virtual ~BaseTCPTransport()
	{
		TRPLOG("%p", this);
	}

	virtual bool send(std::vector<char> data) override
	{
		// #TODO : This is not completely thread safe.
		// Send can be called from different threads, because of the way we handle RPCs that return std::future
		//SINGLETHREAD_ENFORCE();

		if (m_closing)
			return false;

		TRPLOG("%p", this);
		auto d = std::make_shared<std::vector<char>>(std::move(data));
		m_sock.asyncWrite(
			d->data(), static_cast<int>(d->size()),
			[d, this, con=m_rpcCon.lock()](const TCPError& ec, int bytesTransfered)
		{
			if (ec)
			{
				TRPLOG("%p: asyncWrite failed", this);
				close();
				return;
			}
			assert((size_t)bytesTransfered == d->size());
		});

		return true;
	}

	virtual bool receive(std::vector<char>& dst) override
	{
		SINGLETHREAD_ENFORCE();
		
		if (m_closing)
			return false;

		if (m_inQueue.size()==0)
		{
			TRPLOG("%p: Not ready", this);
			dst.clear();
			return true;
		}

		dst = std::move(m_inQueue.front());
		m_inQueue.pop();
		TRPLOG("%p: Ready. %d bytes", this, (int)dst.size());
		return true;
	}

	virtual void close() override
	{
		TCPService::getFrom(m_sock).dispatch([this, con=m_rpcCon.lock()]
		{
			doClose();
		});
	}

	const std::pair<std::string,int>& getPeerAddress() const
	{
		return m_sock.getPeerAddress();
	}

protected:

	void triggerConProcessing(bool allowDispatch)
	{
		// NOTE:
		// m_conProcessPending.clear() needs to be called BEFORE calling con->process()
		// Initially I was trying to be smart and call it after to make sure there were no unnecessary extra calls to
		// process().
		// But there is a small window inside process() where the processing done already, and failing to queue another
		// process() call will indefinetly block everything. 
		// By clearing the flag BEFORE calling process(), we make sure this doesn't happen, at the cost of having
		// sporadic extra calls to process() queued.

		// If the previous value is true, it means there is a pending processing already
		if (m_conProcessPending.test_and_set())
			return;

		auto& io = TCPService::getFrom(m_sock);
		if (allowDispatch && io.tickingInThisThread())
		{
			SINGLETHREAD_ENFORCE();
			m_conProcessPending.clear();
			m_rpcCon.lock()->process();
		}
		else
		{
			io.post([this, con = m_rpcCon.lock()]()
			{
				m_conProcessPending.clear();
				con->process();
			});
		}
	}

	void doClose()
	{
		SINGLETHREAD_ENFORCE();

		if (m_closing)
		{
			TRPLOG("%p : Close: Was already closing...", this);
			return;
		}
		TRPLOG("%p : Close: closing...", this);
		m_closing = true;
		m_sock.asyncClose([this, con = m_rpcCon.lock()]()
		{
			triggerConProcessing(true);
		});
	}


	DECLARE_THREADENFORCER_AFFINITY;

	template<typename, typename> friend class TCPTransportAcceptor;

	void startReadSize()
	{
		SINGLETHREAD_ENFORCE();

		assert(m_incoming.size() == 0);
		m_sock.asyncRead(
			reinterpret_cast<char*>(&m_incomingSize), sizeof(m_incomingSize),
			[this, con=m_rpcCon.lock()](const TCPError& ec, int bytesTransfered)
		{
			if (ec)
			{
				TRPLOG("%p: asyncRead (startReadSize) failed", this);
				close();
				return;
			}

			assert((size_t)bytesTransfered == sizeof(m_incomingSize));
			startReadData();
		});
	}

	void startReadData()
	{
		SINGLETHREAD_ENFORCE();

		assert(m_incomingSize > sizeof(uint32_t));
		m_incoming.insert(m_incoming.end(), m_incomingSize, 0);
		*reinterpret_cast<uint32_t*>(m_incoming.data()) = m_incomingSize;
		// To received "rpcSize" is the total size of the data, so the remaining iss rpcSize - sizeof(uint32_t)
		m_sock.asyncRead(
			m_incoming.data() + sizeof(uint32_t), m_incomingSize - sizeof(uint32_t),
			[this, con=m_rpcCon.lock()](const TCPError& ec, int bytesTransfered)
		{
			if (ec)
			{
				TRPLOG("%p: asyncRead (startReadData) failed", this);
				close();
				return;
			}

			assert(bytesTransfered == m_incoming.size() - sizeof(uint32_t));
			m_inQueue.push(std::move(m_incoming));
			startReadSize();
			triggerConProcessing(false);
		});
	}


	template<typename LOCAL, typename REMOTE>
	static std::future<std::shared_ptr<Connection<LOCAL,REMOTE>>>
		createImpl(TCPService& service, LOCAL* localObj, const char* ip, int port, std::shared_ptr<TCPServiceThread> iothread)
	{
		auto trp = std::make_shared<BaseTCPTransport>(service, iothread);
		auto pr = std::make_shared<std::promise<std::shared_ptr<Connection<LOCAL,REMOTE>>>>();
		trp->m_sock.asyncConnect(ip, port, [pr, trp, localObj](const TCPError& ec)
		{
			if (ec)
			{
				pr->set_value(nullptr);
				return;
			}

			auto con = std::make_shared<Connection<LOCAL,REMOTE>>(localObj, trp);
			trp->m_rpcCon = con;
			TRPLOG("Connected: trp<->con : %p <-> %p", trp.get(), con.get());
			con->setOutSignal([trp=trp.get()]()
			{
				trp->triggerConProcessing(false);
			});
			trp->startReadSize();
			pr->set_value(std::move(con));
		});

		return pr->get_future();
	}

	std::shared_ptr<TCPServiceThread> m_iothread; // this needs to be before m_sock
	TCPSocket m_sock;
	std::weak_ptr<BaseConnection> m_rpcCon;

	// Holds the currently incoming RPC data
	uint32_t m_incomingSize = 0;
	std::vector<char> m_incoming;
	std::queue<std::vector<char>> m_inQueue;
	bool m_closing = false;
	std::atomic_flag m_conProcessPending = ATOMIC_FLAG_INIT;
};


template<typename LOCAL, typename REMOTE>
class TCPTransport : public BaseTCPTransport
{
public:
	static std::future<std::shared_ptr<Connection<LOCAL,REMOTE>>>
		create(TCPService& service, LOCAL& localObj, const char* ip, int port)
	{
		return createImpl<LOCAL,REMOTE>(service, &localObj, ip, port, nullptr);
	}
	static std::future<std::shared_ptr<Connection<LOCAL,REMOTE>>>
		create(std::shared_ptr<TCPServiceThread> iothread, LOCAL& localObj, const char* ip, int port)
	{
		return createImpl<LOCAL,REMOTE>(iothread->io, &localObj, ip, port, iothread);
	}
};

template<typename REMOTE>
class TCPTransport<void,REMOTE> : public BaseTCPTransport
{
public:
	static std::future<std::shared_ptr<Connection<void, REMOTE>>>
		create(TCPService& service, const char* ip, int port)
	{
		return createImpl<void,REMOTE>(service, nullptr, ip, port, nullptr);
	}
	static std::future<std::shared_ptr<Connection<void, REMOTE>>>
		create(std::shared_ptr<TCPServiceThread> iothread, const char* ip, int port)
	{
		return createImpl<void,REMOTE>(iothread->io, nullptr, ip, port, iothread);
	}
};


class BaseTCPTransportAcceptor
{
public:
	BaseTCPTransportAcceptor(TCPService& service, std::shared_ptr<TCPServiceThread> iothread=nullptr)
		: m_acceptor(service)
		, m_iothread(iothread)
	{
		CZRPC_ASSERT(m_iothread==nullptr || (&m_iothread->io == &service));
	}

	virtual ~BaseTCPTransportAcceptor()
	{
	}

protected:
	std::shared_ptr<TCPServiceThread> m_iothread; // this needs to be before m_acceptor
	TCPAcceptor m_acceptor;
};

template<typename LOCAL, typename REMOTE>
class TCPTransportAcceptor : public BaseTCPTransportAcceptor
{
public:
	using LocalType = LOCAL;
	using RemoteType = REMOTE;
	using ConnectionType = Connection<LocalType, RemoteType>;
	using AcceptHandler = std::function<bool(std::shared_ptr<ConnectionType>)>;

	TCPTransportAcceptor(TCPService& service, LocalType& localObj)
		: BaseTCPTransportAcceptor(service, nullptr)
		, m_localObj(localObj)
	{
	}

	TCPTransportAcceptor(std::shared_ptr<TCPServiceThread> iothread, LocalType& localObj)
		: BaseTCPTransportAcceptor(iothread->io, iothread)
		, m_localObj(localObj)
	{
	}

	bool start(int port, AcceptHandler handler)
	{
		assert(!m_acceptor.isValid());
		assert(handler);
		auto ec = m_acceptor.listen(port, 200);
		if (ec)
			return false;
		setupAccept(std::move(handler));
		return true;
	}

private:

	void setupAccept(AcceptHandler handler)
	{
		auto trp = std::make_shared<BaseTCPTransport>(TCPService::getFrom(m_acceptor), m_iothread);
		m_acceptor.asyncAccept(trp->m_sock, [this, trp, handler=std::move(handler)](const TCPError& ec)
		{
			SINGLETHREAD_ENFORCE();

			if (ec)
				return;

			auto con = std::make_shared<ConnectionType>(&m_localObj, trp);
			trp->m_rpcCon = con;
			TRPLOG("Accepted: trp<->con : %p <-> %p", trp.get(), con.get());
			con->setOutSignal([con=con.get()]
			{
				con->process(BaseConnection::Direction::Out);
			});

			trp->startReadSize();
			if (handler(std::move(con)))
				setupAccept(std::move(handler));
		});
	}

	DECLARE_THREADENFORCER_AFFINITY;
	LocalType& m_localObj;
	std::function<void(std::shared_ptr<ConnectionType>)>  m_newConnectionCallback;
};


} // namespace rpc
} // namespace cz

