#pragma once

namespace cz
{
namespace rpc
{

template<typename LOCAL, typename REMOTE=void>
class SimpleServer
{
public:
	using Local = LOCAL;
	using Remote = REMOTE;

	struct Session : rpc::Session
	{
		Session(spas::Service& service)
			: trp(service)
		{
			printf("Session %p created\n", this);
		}
		~Session()
		{
			printf("Session %p destroyed\n", this);
		}
		Connection<Local, Remote> rpccon;
		SpasTransport trp;
	};

	explicit SimpleServer(
		Local& obj, int port, std::string authToken = "")
		: m_obj(obj)
		, m_objData(&m_obj)
		, m_acceptor(m_service)
	{
		printf("Starting server on port %d, with token '%s'\n", port, authToken.c_str());
		m_objData.setAuthToken(std::move(authToken));
		auto ec = m_acceptor.listen(port);
		if (ec)
		{
			throw std::runtime_error("Could not start listening on the specified port.");
		}

		setupAccept();

		// Starting the service thread after setting up the accept, so we don't need a dummy work item
		m_ioth = std::thread([this]
		{
			m_service.run();
		});
	}

	~SimpleServer()
	{
		finish();
	}

	Local& obj() { return m_obj; }
	ObjectData& objData() { return m_objData; };

	spas::Service& getService()
	{
		return m_service;
	}

private:

	void finish()
	{
		m_service.post([this]()
		{
			m_shuttingDown = true;
			m_acceptor.cancel();
			for (auto&& c : m_cons)
				c->rpccon.close();
		});

		if (m_ioth.joinable())
			m_ioth.join();
	}

	void setupAccept()
	{
		auto session = std::make_shared<Session>(m_service);

		m_acceptor.asyncAccept(session, session->trp, session->rpccon, m_obj, [this, session](const spas::Error& ec)
		{
			if (m_shuttingDown)
				return;
			if (ec)
			{
				if (ec.code == spas::Error::Code::Cancelled)
					return;
				printf("Error accepting connection: %s\n", ec.msg());
			}

			auto&& addr = session->trp.getPeerAddr();
			printf("Client %s:%d connected.\n", addr.first.c_str(), addr.second);

			session->rpccon.setOnDisconnect([this, session]
			{
				auto&& addr = session->trp.getPeerAddr();
				printf("Client %s:%d disconnected.\n", addr.first.c_str(), addr.second);
				m_cons.erase(std::remove(m_cons.begin(), m_cons.end(), session));
			});

			m_cons.push_back(std::move(session));
			setupAccept();
		});

	}

	Local& m_obj;
	std::thread m_ioth;
	spas::Service m_service;
	bool m_shuttingDown = false;
	ObjectData m_objData;
	SpasTransportAcceptor m_acceptor;
	std::vector<std::shared_ptr<Session>> m_cons;
};

} // namespace rpc
} // namespace cz

