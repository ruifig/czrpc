#pragma once

namespace cz
{
namespace rpc
{

template<typename LOCAL, typename REMOTE>
class SimpleServer
{
public:
	using Local = LOCAL;
	using Remote = REMOTE;

	explicit SimpleServer(
		Local& obj, int port, std::string authToken = "", std::shared_ptr<TCPServiceThread> iothread = getSharedData<TCPServiceThread>())
		: m_obj(obj)
		, m_objData(&m_obj)
		, m_acceptor(iothread, m_obj)
	{
		printf("Starting server on port %d, with token '%s'\n", port, authToken.c_str());
		m_objData.setAuthToken(std::move(authToken));

		auto res = m_acceptor.start(port, [&](std::shared_ptr<Connection<Local, Remote>> con)
		{
			auto addr = static_cast<BaseTCPTransport*>(con->getTransport().get())->getPeerAddress();
			printf("Client %s:%d connected.\n", addr.first.c_str(), addr.second);
			con->setDisconnectSignal([addr]
			{
				printf("Client %s:%d disconnected.\n", addr.first.c_str(), addr.second);
			});

			m_cons.push_back(std::move(con));
			return true;
		});

		if (!res)
		{
			throw std::runtime_error("Could not start listening on the specified port.");
		}
	}

	~SimpleServer()
	{
	}

	Local& obj() { return m_obj; }
	ObjectData& objData() { return m_objData; };
private:
	Local& m_obj;
	ObjectData m_objData;
	TCPTransportAcceptor<Local, Remote> m_acceptor;
	std::vector<std::shared_ptr<Connection<Local, Remote>>> m_cons;
};

} // namespace rpc
} // namespace cz

