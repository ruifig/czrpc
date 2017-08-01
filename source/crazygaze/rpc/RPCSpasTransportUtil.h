#pragma once

#include "RPCSpasTransport.h"

namespace cz
{
namespace rpc
{

template<typename LOCAL, typename REMOTE>
class SpasSession : public SessionData, public std::enable_shared_from_this<SpasSession>
{
public:
	using Local = LOCAL;
	using Remote = REMOTE;

	explicit SpasSession(spas::Service& service)
		: m_rpctrp(service)
	{
		//printf("Session: %p constructed\n", this);
	}
	~SpasSession()
	{
		//printf("Session: %p destroyed\n", this);
	}

	Connection<Local, Remote>& getCon()
	{
		return m_rpccon;
	}

	SpasTransport& getTrp()
	{
		return m_rpctrp;
	}

protected:
	Connection<LOCAL, REMOTE> m_rpccon;
	SpasTransport m_rpctrp;
};

template<typename SessionType, typename... Args>
std::shared_ptr<SessionType> createSpasClientSession(spas::Service& service, const char* ip, int port, Args&&... args)
{
	auto session = std::make_shared<SessionType>(service, std::forward<Args>(args)...);
	auto ec = session->getTrp().connect(session, session->getCon(), ip, port);
	if ()
}

template<typename LOCAL, typename REMOTE = void>
class SpasServer
{
};


} // namespace rpc
} // namespace cz


