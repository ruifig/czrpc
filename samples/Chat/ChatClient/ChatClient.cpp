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
	ChatClient(spas::Service& service, const std::string& ip, int port)
		: m_trp(service)
	{
		printf("SYSTEM: Connecting to Chat Server at %s:%d\n", ip.c_str(), port);
		spas::Error ec = m_trp.asyncConnect(nullptr, m_con, *this, ip.c_str(), port).get();
		if (ec)
		{
			printf("Failed to connect to %s:%d\n", ip.c_str(), port);
			printf("Error: %s\n", ec.msg());
			m_finished = true;
			return;
		}
		printf("SYSTEM: Connected to server %s:%d\n", ip.c_str(), port);

		m_con.setOnDisconnect([this]
		{
			printf("Disconnected\n");
			system("pause");
			m_finished = true;
		});
	}

	~ChatClient()
	{
	}

	bool isFinished()
	{
		return m_finished;
	}

	int run(const std::string& name, const std::string& pass)
	{
		if (m_finished)
			return EXIT_FAILURE;

		auto res = CZRPC_CALL(m_con, login, name, pass).ft().get().get();
		if (res!="OK")
		{
			printf("LOGIN ERROR: %s\n", res.c_str());
			return EXIT_FAILURE;
		}

		while (!m_finished)
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

		return EXIT_SUCCESS;
	}

private:
	virtual void onMsg(const std::string& name, const std::string& msg) override
	{
		printf("%s: %s\n", name=="" ? "SYSTEM" : name.c_str(), msg.c_str());
	}

	bool m_finished = false;
	ConType m_con; // #TODO : Rename this to m_con after the refactoring is working
	SpasTransport m_trp; // #TODO : Rename this to m_trp, or possibly group it together with the con object
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

	// Start the service
	spas::Service service;
	std::thread ioth = std::thread([&service]
	{
		spas::Service::Work work(service);
		service.run();
	});

	ChatClient client(service, addr.first, addr.second);
	if (client.isFinished())
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
}

