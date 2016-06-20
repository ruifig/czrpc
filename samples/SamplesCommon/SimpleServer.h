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

	explicit SimpleServer(Local& obj, int port, std::string authToken="")
		: m_obj(obj)
		, m_objData(&m_obj)
	{
		m_th = std::thread([this]
		{
			ASIO::io_service::work w(m_io);
			m_io.run();
		});

		printf("Starting server on port %d, with token '%s'\n", port, authToken.c_str());
		m_objData.setAuthToken(std::move(authToken));

		m_acceptor = AsioTransportAcceptor<Local, Remote>::create(m_io, m_obj);
		m_acceptor->start(port, [&](std::shared_ptr<Connection<Local, Remote>> con)
		{
			auto trp = static_cast<BaseAsioTransport*>(con->transport.get());
			auto point = trp->getRemoteEndpoint();
			printf("Client %s:%d connected.\n", point.address().to_string().c_str(), point.port());
			trp->setOnClosed([point]
			{
				printf("Client %s:%d disconnected.\n", point.address().to_string().c_str(), point.port());
			});
			m_cons.push_back(std::move(con));
		});
	}

	~SimpleServer()
	{
		m_io.stop();
		m_th.join();
	}

	Local& obj() { return m_obj;   }
	ObjectData& objData() { return m_objData; };
private:
	ASIO::io_service m_io;
	std::thread m_th;
	Local& m_obj;
	ObjectData m_objData;
	std::shared_ptr<AsioTransportAcceptor<Local, Remote>> m_acceptor;
	std::vector<std::shared_ptr<Connection<Local, Remote>>> m_cons;
};

} // namespace rpc
} // namespace cz

