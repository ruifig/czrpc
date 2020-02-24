// ChatServer.cpp : Defines the entry point for the console application.
//

#include "ChatServerPCH.h"

using namespace cz;
using namespace cz::rpc;

Parameters gParams;

#define LOG(fmt, ...) printf("LOG: " fmt "\n", ##__VA_ARGS__);

// Example on how to call an RPC on all clients
#define BROADCAST_RPC(excludedClient, func, ...)             \
	{                                                        \
		auto excluded = excludedClient;                      \
		for (auto&& c : m_clients)                           \
		{                                                    \
			if (c.second.get() == excluded)                  \
				continue;                                    \
			CZRPC_CALL(c.second->con, func, __VA_ARGS__); \
		}                                                    \
	}

// This puts together all the data for 1 client.
// By inherithing from rpc::SessionData, we can pass a shared_ptr to czrpc, to make sure its kept alive for as long
// as there are asynchronous operations using it.
struct ClientInfo : public rpc::SessionData
{
	explicit ClientInfo(spas::Service& io)
		: trp(io)
	{
		LOG("DEBUG: ClientInfo constructed (%p)", this);
	}

	~ClientInfo()
	{
		LOG("DEBUG: ClientInfo destroyed (%p, %s)", this, name.c_str());
	}

	Connection<ChatServerInterface, ChatClientInterface> con;
	SpasTransport trp;
	std::string name;
	bool admin = false;
	bool authenticated = false;
};

class ChatServer : public ChatServerInterface
{
public:
	using ConType = Connection<ChatServerInterface, ChatClientInterface>;

	ChatServer(int port)
		: m_objData(this)
		, m_acceptor(m_io)
	{
		m_objData.setProperty("name", Any("chat"));
		auto ec = m_acceptor.listen(port);
		if (ec)
			throw std::runtime_error(formatString("%s", ec.msg()));

		setupAccept();

		// NOTE: We start the IO thread AFTER calling setupAccept, so the Service has work to do, otherwise
		// Service::run return immediately
		// This makes it easier to deal with shutdown. Once whe cancel all asynchronous operations, Service::run will
		// exit and we are done.
		m_th = std::thread([this]
		{
			m_io.run();
		});
	}

	~ChatServer()
	{
		// Cancel everything from the service thread
		m_io.post([this]()
		{
			m_acceptor.cancel();
			for (auto&& info : m_clients)
				info.second->con.close();
		});

		m_th.join();
	}

private:

	void setupAccept()
	{
		auto clientInfo = std::make_shared<ClientInfo>(m_io);
		// We pass the clientInfo as "session" data, so czrpc keeps it alive for as long as there are asynchronous
		// operations that need it.
		m_acceptor.asyncAccept(clientInfo, clientInfo->trp, clientInfo->con, *this,
			[this, clientInfo](const spas::Error& ec)
		{
			if (ec)
			{
				// If we are shutting down, we get a Cancelled error, so nothing to log
				if (ec.code == spas::Error::Code::Cancelled)
					return;
				else
				{
					// Note that we are not returning if we get an error.
					// We simply setup another accept (see bellow), so we keep accepting other clients
					LOG("Failed to accept incoming connection: %s", ec.msg());
				}
			}
			else
			{
				LOG("Client connected.");
				// Add the client to our map
				m_clients.insert(std::make_pair(&clientInfo->con, clientInfo));

				// Setup a callback to remove the client from our map.
				// czrpc might still keep the session alive because the transport might be using it, but it will
				// be destroyed shortly after this.
				clientInfo->con.setOnDisconnect([this, clientInfo]()
				{
					onDisconnect(clientInfo);
				});
			}

			// setup another accept, so we can get more clients
			setupAccept();
		});
	}

	// Remove a client from our map, and broadcast to all other clients that the removed client disconnected
	void onDisconnect(std::shared_ptr<ClientInfo> info)
	{
		for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
		{
			if (it->second == info) // Found the client to remove
			{
				m_clients.erase(it);
				LOG("User %s disconnected", info->name.c_str());

				// If the client is not authenticated, it didn't login, and as such other clients don't care
				if (info->authenticated)
					BROADCAST_RPC(nullptr, onMsg, "", formatString("user %s disconnected", info->name.c_str()));
				return;
			}
		}
	}

	// At any point, czrpc allows us to check if we are serving an RPC call (on the thread we are checking)
	// This makes it possible for a server to know what client called the RPC
	ClientInfo* getCurrentUser()
	{
		if (ConType::getCurrent())
		{
			auto it = m_clients.find(ConType::getCurrent());
			return (it == m_clients.end()) ? nullptr : it->second.get();
		}
		else
		{
			return nullptr;
		}
	}

	std::shared_ptr<ClientInfo> getUser(const std::string& name)
	{
		for(auto it = m_clients.begin(); it!= m_clients.end(); ++it)
		{
			if (it->second->name==name)
			{
				return it->second;
			}
		}
		return nullptr;
	};


	//
	// ChatServerInterface - The actual RPC interface
	//
	virtual std::string login(const std::string& name, const std::string& pass) override
	{
		auto info = getUser(name);
		if (info)
		{
			return "A user with the same name is already logged in.\n";
		}

		LOG("RPC:login:%s,%s", name.c_str(), pass.c_str());
		if (pass != "pass")
			return "Wrong password";

		auto user = getCurrentUser();
		user->name = name;
		if (name == "Admin")
			user->admin = true;

		BROADCAST_RPC(nullptr, onMsg, "", formatString("%s joined the chat", name.c_str()));
		user->authenticated = true;
		return "OK";
	}

	virtual void sendMsg(const std::string& msg) override
	{
		LOG("RPC:sendMsg:%s", msg.c_str());
		auto user = getCurrentUser();
		if (!user || !user->authenticated)
			return;
		BROADCAST_RPC(nullptr, onMsg, user->name, msg);
	}

	virtual void kick(const std::string& name) override
	{
		LOG("RPC:kick:%s", name.c_str());
		auto user = getCurrentUser();
		if (!user->admin)
		{
			CZRPC_CALL(user->con, onMsg, "", "You do not have admin rights");
			return;
		}

		std::shared_ptr<ClientInfo> kicked = getUser(name);

		if (!kicked)
		{
			CZRPC_CALL(user->con, onMsg, "", formatString("User %s not found", name.c_str()));
			return;
		}

		// Inform the kicked player that he was kicked
		// #TODO : I can see a problem here. The "close" will only happen once we get the reply from the client,
		// which is "Result<void>". If the client doesn't answer and keeps the connection alive, the server will
		// in theory never disconnect this client.
		// This can be solved once czrpc has support for rpcs timeout
		CZRPC_CALL(kicked->con, onMsg, "", "You were kicked").async(
			[kicked,this] (Result<void> r)
		{
			kicked->con.close();
		});

	}

	virtual std::vector<std::string> getUserList() override
	{
		LOG("RPC:getUserList");
		auto user = getCurrentUser();
		if (!user || !user->authenticated)
			return std::vector<std::string>();

		std::vector<std::string> users;
		for (auto&& u : m_clients)
			users.push_back(u.second->name);
		return users;
	}

	spas::Service m_io;
	std::thread m_th;
	SpasTransportAcceptor m_acceptor;
	std::unordered_map<ConType*, std::shared_ptr<ClientInfo>> m_clients;
	ObjectData m_objData;
};

int main(int argc, char* argv[])
{
	gParams.set(argc, argv);

	try
	{
		int port = CHATSERVER_DEFAULT_PORT;
		if (gParams.has("port"))
			port = std::stoi(gParams.get("port"));

		printf("Running ChatServer on port %d.\n", port);
		printf("Type any key to quit.\n");
		ChatServer server(port);
		while (true)
		{
			if (my_getch())
				break;
		}
	}
	catch (const std::exception& e)
	{
		printf("Exception: %s\n", e.what());
	}
	return 0;
}

