#include "BenchmarkPCH.h"

#define FATAL_ERROR(fmt, ...) \
	{ \
		printf(fmt##"\n", ##__VA_ARGS__); \
		exit(EXIT_FAILURE); \
	}

using namespace cz;
using namespace cz::rpc;

class BenchmarkServer
{
public:

	void waitToFinish()
	{
		m_finish.get_future().get();
	}

	// RPC interface
	void send(std::vector<uint8_t> data)
	{
	}
	void finish()
	{
		m_finish.set_value();
	}
private:
	std::promise<void> m_finish;
};

#define RPCTABLE_CLASS BenchmarkServer
#define RPCTABLE_CONTENTS \
	REGISTERRPC(send) \
	REGISTERRPC(finish)
#include "crazygaze/rpc/RPCGenerate.h"

Parameters gParams;

template<typename LOCAL, typename REMOTE>
struct SimpleClient
{
public:
	using Local = LOCAL;
	using Remote = REMOTE;

	SimpleClient()
	{
	}

	template<typename U=Local>
	explicit SimpleClient(typename std::enable_if<!std::is_void<U>::value, U>::type& localObj)
		:m_localObj(&localObj)
	{
	}

	~SimpleClient()
	{
		if (m_iothread.joinable())
		{
			m_io.stop();
			m_iothread.join();
		}
	}

	bool start(const std::string& ip, int port, std::string token="")
	{
		m_iothread = std::thread([this]
		{
			ASIO::io_service::work w(m_io);
			m_io.run();
		});

		printf("Connecting to %s:%d with token '%s'\n", ip.c_str(), port, token.c_str());
		m_con = AsioTransport<Local,Remote>::create(m_io, ip.c_str(), port).get();
		if (!m_con)
		{
			printf("Could not connect to server at %s:%d\n", ip.c_str(), port);
			return false;
		}
		auto point = static_cast<BaseAsioTransport*>(m_con->getTransport().get())->getLocalEndpoint();
		printf("Connected. LocalEndpoint=%s:%d\n", point.address().to_string().c_str(), point.port());

		bool authRes = false;
		CZRPC_CALLGENERIC(*m_con, "__auth", std::vector<Any>{ Any(token) }).ft().get().get().getAs(authRes);
		if (!authRes)
		{
			printf("Authentication failed\n");
			return false;
		}

		return true;
	}

	Connection<Local,Remote>& con()
	{
		return *m_con;
	}

private:
	std::shared_ptr<Connection<Local, Remote>> m_con;
	Local* m_localObj = nullptr;
	ASIO::io_service m_io;
	std::thread m_iothread;
};

int runClient()
{
	if (!gParams.has("ip"))
		FATAL_ERROR("ip parameter not specified");
	if (!gParams.has("port"))
		FATAL_ERROR("port parameter not specified");

	std::string ip = gParams.get("ip");
	int port = std::stoi(gParams.get("port"));

	SimpleClient<void, BenchmarkServer> client;
	if (!client.start(ip, port, "Benchmark"))
		FATAL_ERROR("");

	CZRPC_CALL(client.con(), finish);

	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	gParams.set(argc, argv);

	if (gParams.has("server"))
	{
		if (!gParams.has("port"))
			FATAL_ERROR("port parameter not specified");
		BenchmarkServer serverObj;
		SimpleServer<BenchmarkServer, void> server(serverObj, std::stoi(gParams.get("port")), "Benchmark");
		printf("Waiting for client connection...\n");
		server.obj().waitToFinish();
		printf("Finishing...\n");
		return EXIT_SUCCESS;
	}
	else
	{
		return runClient();
	}

}
