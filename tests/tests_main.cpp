#include "testsPCH.h"
#include "crazygaze/rpc/RPCTCPSocketTransport.h"


//
// Entry points to try samples used in the documentation
void RunDocTest_ASmallTaste();
void RunDocTest_ParamTraits();

using namespace cz::rpc;

void TestTCPTransport()
{
	TCPSocketSet set;

	auto th = std::thread([&]
	{
		while (set.tick()) { }
	});

	std::shared_ptr<TCPSocket> c;
	auto acceptor = set.listen(9000,
		[&c, &set](std::shared_ptr<TCPSocket> s)
	{
		c = s;

		s->setOnRecv([&set](const char* buf, int len)
		{
			std::string str(buf, len);
			printf("Server: Received %d bytes: %s \n", len, str.c_str());
			set.stop();
		});
	});

	auto client = set.connect("127.0.0.1", 9000, [](const char* buf, int len)
	{
		std::string str(buf, len);
		printf("Client: Received %d bytes: %s \n", len, str.c_str());
	});

	//Sleep(10000);
	client->send("Hello", 6);
	th.join();
	printf("DONE...\n");
}
int main()
{
	TestTCPTransport();
	//RunDocTest_ParamTraits();
	auto res = UnitTest::RunAllTests();
	return res == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
