#pragma once

namespace cz
{
namespace rpc
{

class BaseConnection
{
public:
	BaseConnection() { }
	virtual ~BaseConnection() { }

	enum class Direction
	{
		In = 1 << 0,
		Out = 1 << 1,
		Both = In | Out
	};

	virtual void process(Direction what=Direction::Both) = 0;
	virtual bool isRunningInThread() = 0;
	virtual void close() = 0;

	const Transport* getTransport()
	{
		return m_transport;
	}

	std::shared_ptr<SessionData> getSession()
	{
		return m_weakSession.lock();
	}

protected:

	void initBase(Transport& transport, std::shared_ptr<SessionData> session)
	{
		m_transport = &transport;
		m_transport->m_con = this;
		m_weakSession = session;
	}

	Transport* m_transport = nullptr;
	// This keeps a weak reference, and it's from this czrpc gets any strong references to pass to async handlers.
	// NOTE: This also allows the transport to still get a valid session pointer during shutdown if it needs to queue
	// up and execute any more async handlers during shutdown
	std::weak_ptr<SessionData> m_weakSession;
};

}
}

