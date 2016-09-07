#include "testsPCH.h"

//
// Entry points to try samples used in the documentation
void RunDocTest_ASmallTaste();
void RunDocTest_ParamTraits();

using namespace cz::rpc;
#if 0
void TestTCPTransport()
{
	TCPService set;

	auto th = std::thread([&]
	{
		while (set.tick()) { }
	});

	TCPError ec;

	//
	// Server
	//
	set.listen(9000, [&set](const TCPError& ec, std::shared_ptr<TCPAcceptor> acceptor)
	{
		assert(!ec);
		printf("async listen called\n");
		acceptor->accept([&set](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
		{
			assert(!ec);
			TCPBuffer buf(256);
			sock->recv(buf, [&, sock, buf](const TCPError& ec, int bytesTransfered)
			{
				assert(!ec);
				std::string str(buf.ptr(), bytesTransfered);
				printf("Server: Received %d bytes: %s \n", bytesTransfered, str.c_str());
				sock->send("there!", 7, nullptr);
			});
		});
	});

	//
	// Client
	//
	set.connect("127.0.0.1", 9000, [&set](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
	{
		assert(!ec);
		TCPBuffer buf(256);
		sock->recv(buf, [&, buf](const TCPError& ec, int bytesTransfered)
		{
			if (bytesTransfered == 0)
				return;
			std::string str(buf.ptr(), bytesTransfered);
			printf("Client: Received %d bytes: %s \n", bytesTransfered, str.c_str());
			set.stop();
		});

		sock->send("Hello", 6, [](const TCPError& ec, int bytesTransfered)
		{
			assert(!ec);
			printf("Client: Sent %d bytes\n", bytesTransfered);
		});
	});

	th.join();
	printf("DONE...\n");
}
#endif

int main()
{
	//TestTCPTransport();
	//RunDocTest_ParamTraits();
	auto res = UnitTest::RunAllTests();
	return res == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
