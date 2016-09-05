#include "testsPCH.h"
//#include "crazygaze/rpc/RPCTCPSocketTransport.h"
#pragma warning(disable:4996)
namespace cz {
	namespace rpc {

		struct MyTCPLog
		{
			static void out(bool fatal, const char* type, const char* fmt, ...)
			{
				char buf[256];
				strcpy(buf, type);
				va_list args;
				va_start(args, fmt);
				vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 1, fmt, args);
				va_end(args);
				printf(buf);
				printf("\n");
				if (fatal)
				{
					__debugbreak();
					exit(1);
				}
			}
		};

#define TCPINFO(fmt, ...) MyTCPLog::out(false, "Info: ", fmt, ##__VA_ARGS__)

}
}

#include "crazygaze/rpc/RPCTCPSocket.h"

//
// Entry points to try samples used in the documentation
void RunDocTest_ASmallTaste();
void RunDocTest_ParamTraits();

using namespace cz::rpc;

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
int main()
{
	TestTCPTransport();
	//RunDocTest_ParamTraits();
	auto res = UnitTest::RunAllTests();
	return res == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
