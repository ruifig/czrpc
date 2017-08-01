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

	void close()
	{
		m_transport->close();
	}

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
		m_strongSession = std::move(session);
		m_weakSession = m_strongSession;
	}

	Transport* m_transport = nullptr;
	// The strong reference is kept until the transport disconnects, or the user explicitly closes the connection
	// This keeps a session alive even if there are no pending operations.
	std::shared_ptr<SessionData> m_strongSession;
	// This keeps a weak reference, and it's the one from where everything gets a session pointers.
	// This allows the transport to still get a valid session pointer during shutdown, since at the point the strong
	// reference might not be valid anymore.
	std::weak_ptr<SessionData> m_weakSession;
};

}
}

