// ChatServer.cpp : Defines the entry point for the console application.
//

#include "ChatServerPCH.h"
#include "../ChatCommon/Utils.inl"

using namespace cz::rpc;

#define LOG(fmt, ...) printf("LOG: " fmt "\n", __VA_ARGS__)

#define BROADCAST_RPC(excludedClient, func, ...)             \
	{                                                        \
		auto excluded = excludedClient;                      \
		for (auto&& c : m_clients)                           \
		{                                                    \
			if (c.second.get() == excluded)                  \
				continue;                                    \
			CZRPC_CALL(*(c.second->con), func, __VA_ARGS__); \
		}                                                    \
	}

struct ClientInfo
{
	std::shared_ptr<Connection<ChatServerInterface, ChatClientInterface>> con;
	std::string name;
	bool admin = false;
};

class ChatServer : public ChatServerInterface
{
public:
	using ConType = Connection<ChatServerInterface, ChatClientInterface>;

	ChatServer(int port)
	{
		m_th = std::thread([this]
		{
			ASIO::io_service::work w(m_io);
			m_io.run();
		});

		m_acceptor = std::make_shared<AsioTransportAcceptor<ChatServerInterface, ChatClientInterface>>(m_io, *this);
		m_acceptor->start(port, [&](std::shared_ptr<ConType> con)
		{
			LOG("Client connected.");
			auto info = std::make_shared<ClientInfo>();
			info->con = con;
			m_clients.insert(std::make_pair(con.get(), info));
		});
	}

	~ChatServer()
	{
		m_io.stop();
		m_th.join();
	}

private:
	//

	// ChatServerInterface
	//
	virtual std::string login(const std::string& name, const std::string& pass) override
	{
		LOG("RPC:login:%s,%s", name.c_str(), pass.c_str());
		if (pass != "pass")
			return "Wrong password";
		// #TODO

		auto user = getCurrentUser();
		user->name = name;
		if (name == "Admin")
			user->admin = true;

		BROADCAST_RPC(nullptr, onMsg, "", formatString("%s joined the chat", name.c_str()));
		return "OK";
	}

	virtual void sendMsg(const std::string& msg) override
	{
		LOG("RPC:sendMsg:%s", msg.c_str());
		// #TODO
		auto user = getCurrentUser();
		if (!user)
			return;
		BROADCAST_RPC(nullptr, onMsg, user->name, msg);
	}

	virtual void kick(const std::string& name) override
	{
		LOG("RPC:kick:%s", name.c_str());

		auto user = getCurrentUser();
		if (!user->admin)
		{
			CZRPC_CALL(*user->con, onMsg, "", "You do not have admin rights");
			return;
		}

		// Remove the kicked player if it exists
		std::shared_ptr<ClientInfo> kicked;
		for(auto it = m_clients.begin(); it!= m_clients.end(); ++it)
		{
			if (it->second->name==name)
			{
				kicked = std::move(it->second);
				m_clients.erase(it);
				break;
			}
		}

		if (!kicked)
		{
			CZRPC_CALL(*user->con, onMsg, "", formatString("User %s not found", name.c_str()));
			return;
		}

		// Inform the kicked player that he was kicked
		CZRPC_CALL(*kicked->con, onMsg, "", "You were kicked").async(
			[kicked,this] ()
		{
			// "kicked" is captured, to keep it alive while we need it
			printf("\n");
		});

	}

	ClientInfo* getCurrentUser()
	{
		if (ConType::getCurrent())
		{
			auto it = m_clients.find(ConType::getCurrent());
			return (it == m_clients.end()) ? nullptr : it->second.get();
			if (it == m_clients.end())
				return nullptr;
		}
		else
			return nullptr;
	}

	ASIO::io_service m_io;
	std::thread m_th;
	std::shared_ptr<AsioTransportAcceptor<ChatServerInterface, ChatClientInterface>> m_acceptor;
	std::unordered_map<ConType*, std::shared_ptr<ClientInfo>> m_clients;
};

int main()
{
	try
	{
		printf("Running ChatServer on port %d.\n", CHATSERVER_DEFAULT_PORT);
		printf("Type any key to quit.\n");
		ChatServer server(CHATSERVER_DEFAULT_PORT);
		while (true)
		{
			if (getch())
				break;
		}
	}
	catch (const std::exception& e)
	{
		printf("Exception: %s\n", e.what());

	}
	return 0;
}

