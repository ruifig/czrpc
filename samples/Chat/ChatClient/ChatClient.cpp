/*
ChatClient

This is a client that connects a ChatServer process.
It shows how to use bidirectional connections.

Check the ChatInterface.h file for an explanation of the interfaces
*/
#include "ChatClientPCH.h"

using namespace cz;
using namespace cz::rpc;

Parameters gParams;

class ChatClient : public ChatClientInterface
{
public:
	using ConType = Connection<ChatClientInterface, ChatServerInterface>;

	ChatClient(spas::Service& service)
		: m_trp(service)
	{
	}

	~ChatClient()
	{
	}

	bool init(const std::string& ip, int port)
	{
		printf("SYSTEM: Connecting to Chat Server at %s:%d\n", ip.c_str(), port);
		spas::Error ec = m_trp.connect(nullptr, m_con, *this, ip.c_str(), port);
		if (ec)
		{
			printf("Failed to connect to %s:%d: %s\n", ip.c_str(), port, ec.msg());
			return false;
		}
		printf("SYSTEM: Connected to server %s:%d\n", ip.c_str(), port);

		m_con.setOnDisconnect([this]
		{
			printf("Disconnected\n");
			m_finished = true;
		});

		return true;
	}

	bool run(const std::string& name, const std::string& pass)
	{
		auto res = CZRPC_CALL(m_con, login, name, pass).ft().get().get();
		if (res!="OK")
		{
			printf("LOGIN ERROR: %s\n", res.c_str());
			return false;
		}

		while (!m_finished)
		{
			std::string msg;
			std::getline(std::cin, msg);
			if (msg == "/exit")
			{
				printf("Exiting...\n");
				return true;
			}
			else if (strncmp(msg.c_str(), "/kick ", strlen("/kick ")) == 0)
			{
				CZRPC_CALL(m_con, kick, std::string(msg.begin() + strlen("/kick "), msg.end()));
			}
			else if (strncmp(msg.c_str(), "/userlist", strlen("/userlist")) == 0)
			{
				Result<std::vector<std::string>> res = CZRPC_CALL(m_con, getUserList).ft().get();
				if (res.isValid())
				{
					printf("%d users.\n", (int)res.get().size());
					for (auto&& u : res.get())
						printf("    %s\n", u.c_str());
				}
			}
			else if (msg.size())
			{
				CZRPC_CALL(m_con, sendMsg, msg);
			}
		}

		return true;
	}

	bool isFinished()
	{
		return m_finished;
	}


private:

	// This is is a client side RPC. The server calls this on the client
	virtual void onMsg(const std::string& name, const std::string& msg) override
	{
		printf("%s: %s\n", name=="" ? "SYSTEM" : name.c_str(), msg.c_str());
	}

	std::atomic<bool> m_finished {false};
	ConType m_con;
	SpasTransport m_trp;
};

int main(int argc, char *argv[])
{
	gParams.set(argc, argv);

	std::pair<std::string, int> addr("127.0.0.1", CHATSERVER_DEFAULT_PORT);
	if (gParams.has("addr"))
		addr = splitAddress(gParams.get("addr"));
	if (addr.first == "" || addr.second == 0)
	{
		printf("-addr parameter needs to be in the form \"ip:port\". E.g: 127.0.0.1:9000\n");
		return EXIT_FAILURE;
	}

	spas::Service service;
	std::thread ioth = std::thread([&service]
	{
		// Start the service, and use a dummy work item, so Service::run doesn't return right away
		spas::Service::Work keepAlive(service);
		service.run();
	});

	ChatClient client(service);
	if (!client.init(addr.first, addr.second))
	{
		service.stop();
		ioth.join();
		return EXIT_FAILURE;
	}

	std::string name;
	std::string pass;
	char buf[512];
	if (argc==3)
	{
		name = argv[1];
		pass = argv[2];
	}
	else
	{
		printf("Enter your name: ");
		scanf("\n%s", buf);
		name = buf;
		printf("Enter pass: ");
		scanf("%s", buf);
		pass = buf;
	}
	printf("Logging in as %s\n", name.c_str());
	auto res = client.run(name, pass);
	service.stop();
	ioth.join();
	return res ? EXIT_SUCCESS : EXIT_FAILURE;
}

