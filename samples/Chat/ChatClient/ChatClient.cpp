// ChatClient.cpp : Defines the entry point for the console application.
//

#include "ChatClientPCH.h"
#include "../ChatCommon/Utils.inl"

using namespace cz::rpc;

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

		m_con = AsioTransport<ChatClientInterface, ChatServerInterface>::create(m_io, *this, "127.0.0.1", port).get();
		if (!m_con)
		{
			printf("Failed to connect to %s:%d\n", ip.c_str(), port);
			exit(0);
		}
		reinterpret_cast<BaseAsioTransport*>(m_con->transport.get())->setOnClosed([]
		{
			printf("Disconnected\n");
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
	ChatClient client("127.0.0.1", CHATSERVER_DEFAULT_PORT);
	return client.run(name, pass);
}

