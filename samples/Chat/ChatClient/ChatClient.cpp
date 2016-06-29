// ChatClient.cpp : Defines the entry point for the console application.
//

#include "ChatClientPCH.h"

using namespace cz;
using namespace cz::rpc;

Parameters gParams;

class ChatClient : public ChatClientInterface
{
public:

	using ConType = Connection<ChatClientInterface, ChatServerInterface>;
	ChatClient(const std::string& ip, int port)
	{
		m_th = std::thread([this]
		{
			ASIO::io_service::work w(m_io);
			m_io.run();
		});

		printf("SYSTEM: Connecting to Chat Server at %s:%d\n", ip.c_str(), port);
		m_con = AsioTransport<ChatClientInterface, ChatServerInterface>::create(m_io, *this, ip.c_str(), port).get();
		if (!m_con)
		{
			printf("Failed to connect to %s:%d\n", ip.c_str(), port);
			exit(0);
		}
		m_con->setDisconnectSignal([]
		{
			printf("Disconnected\n");
			system("pause");
			exit(0);
		});

		printf("SYSTEM: Connected to server %s:%d\n", ip.c_str(), port);
	}

	~ChatClient()
	{
		m_io.stop();
		m_th.join();
	}

	int run(const std::string& name, const std::string& pass)
	{
		auto res = CZRPC_CALL(*m_con, login, name, pass).ft().get().get();
		if (res!="OK")
		{
			printf("LOGIN ERROR: %s\n", res.c_str());
			return EXIT_FAILURE;
		}

		while (true)
		{
			std::string msg;
			std::getline(std::cin, msg);
			if (msg == "/exit")
			{
				printf("Exiting...\n");
				return EXIT_SUCCESS;
			}
			else if (strncmp(msg.c_str(), "/kick ", strlen("/kick ")) == 0)
			{
				CZRPC_CALL(*m_con, kick, std::string(msg.begin() + strlen("/kick "), msg.end()));
			}
			else if (strncmp(msg.c_str(), "/userlist", strlen("/userlist")) == 0)
			{
				Result<std::vector<std::string>> res = CZRPC_CALL(*m_con, getUserList).ft().get();
				if (res.isValid())
				{
					printf("%d users.\n", (int)res.get().size());
					for (auto&& u : res.get())
						printf("    %s\n", u.c_str());
				}
			}
			else if (msg.size())
			{
				CZRPC_CALL(*m_con, sendMsg, msg);
			}
		}

		return EXIT_SUCCESS;
	}

private:
	virtual void onMsg(const std::string& name, const std::string& msg) override
	{
		printf("%s: %s\n", name=="" ? "SYSTEM" : name.c_str(), msg.c_str());
	}

	ASIO::io_service m_io;
	std::thread m_th;
	std::shared_ptr<ConType> m_con;
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

	ChatClient client(addr.first, addr.second);

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
	return client.run(name, pass);
}

