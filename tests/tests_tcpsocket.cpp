#include "testsPCH.h"

#pragma warning(disable:4996)
#pragma warning(disable:4390)

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

//#define TCPINFO(fmt, ...) MyTCPLog::out(false, "Info: ", fmt, ##__VA_ARGS__)
#define TCPINFO(...)
#define TCPERROR(...)
}
}

#define TCPSOCKET_UNIT_TESTS 1
#include "crazygaze/rpc/RPCTCPSocket.h"

using namespace cz;
using namespace cz::rpc;

#define SERVER_PORT 9000

// A port we know its not available, so we can test listen failure
// On windows we use epmap (port 135)
#define SERVER_UNUSABLE_PORT 135

SUITE(TCPSocket)
{
TEST(TCPService_Listen_Success)
{
	TCPService io;
	auto th = std::thread([&io]
	{
		while (io.tick()) {}
	});

	TCPError ec;
	auto acceptor = io.listen(SERVER_PORT, 1, ec);
	CHECK(!ec && acceptor);

	io.stop();
	th.join();
}

TEST(TCPService_Listen_Failure)
{
	TCPService io;
	auto th = std::thread([&io]
	{
		while (io.tick()) {}
	});

	TCPError ec;
	auto acceptor = io.listen(SERVER_UNUSABLE_PORT, 1, ec);
	CHECK(ec && !acceptor);

	io.stop();
	th.join();
}

TEST(TCPService_Accept_And_Connect_Success)
{
	TCPService io;
	auto th = std::thread([&io]
	{
		while (io.tick()) {}
	});

	// First create an acceptor, so we can connect
	TCPError ec;
	auto acceptor = io.listen(SERVER_PORT, 1, ec);
	CHECK(!ec && acceptor);

	// Setup 2 accepts
	Semaphore sem;
	acceptor->accept([&sem](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
	{
		CHECK(!ec && sock);
		sem.notify();
	});
	acceptor->accept([&sem](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
	{
		CHECK(!ec && sock);
		sem.notify();
	});

	// Test synchronous connect
	{
		auto sock = io.connect("127.0.0.1", SERVER_PORT, ec);
		CHECK(!ec && sock);
		sem.wait(); // wait for the accept
	}

	// Test asynchronous connect
	{
		io.connect("127.0.0.1", SERVER_PORT, [&sem](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
		{
			CHECK(!ec && sock);
			sem.notify();
		});
		sem.wait(); // wait for the accept
		sem.wait(); // wait for the connect
	}

	io.stop();
	th.join();
}

TEST(TCPService_Accept_Failure)
{
	TCPService io;
	auto th = std::thread([&io]
	{
		while (io.tick()) {}
	});

	// First create an acceptor, so we can connect
	TCPError ec;
	auto acceptor = io.listen(SERVER_PORT, 1, ec);
	CHECK(!ec && acceptor);

	//
	// Test canceling 10 accepts directly with the acceptor
	//
	{
		Semaphore sem;
		for (int i = 0; i < 10; i++)
		{
			acceptor->accept([&sem](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
			{
				CHECK(ec.code == TCPError::Code::Cancelled && !sock);
				sem.notify();
			});
		}
		acceptor->cancel();
		for (int i = 0; i < 10; i++)
		{
			sem.wait();
		}
	}

	//
	// Test canceling 10 accepts through the TCPService
	//
	{
		Semaphore sem;
		for (int i = 0; i < 10; i++)
		{
			acceptor->accept([&sem](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
			{
				CHECK(ec.code == TCPError::Code::Cancelled && !sock);
				sem.notify();
			});
		}
		io.stop();
		for (int i = 0; i < 10; i++)
		{
			sem.wait();
		}
	}

	th.join();
}

TEST(TCPService_Connect_Failure)
{
	TCPService io;
	auto th = std::thread([&io]
	{
		while (io.tick()) {}
	});

	// Test synchronous connect failure
	TCPError ec;
	auto sock = io.connect("127.0.0.1", SERVER_PORT, ec);
	CHECK(ec.code==TCPError::Code::Other && !sock);

	Semaphore sem;
	// Test asynchronous connect failure
	const int count = 10;
	double times[count];
	double expectedTimes[count];
	UnitTest::Timer timer;
	timer.Start();
	for (int i = 0; i < count ; i++)
	{
		expectedTimes[i] = i * 100;
		times[i] = timer.GetTimeInMs();
		io.connect("127.0.0.1", SERVER_PORT, [&sem, i, &times, &timer](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
		{
			times[i] = timer.GetTimeInMs() - times[i];
			CHECK(ec.code == TCPError::Code::Timeout && !sock);
			sem.notify();
		}, (int)expectedTimes[i]);
	}
	for (int i = 0; i < count; i++)
	{
		sem.wait();
	}

	CHECK_ARRAY_CLOSE(expectedTimes, times, count, 3.0f);

	// Test asynchronous connect cancel
	io.setAsyncOpsDebugSleep(20); // Setup a delay to execute async listens and connects, so we can test cancel
	for (int i = 0; i < 5; i++)
	{
		io.connect("127.0.0.1", SERVER_PORT, [&sem](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
		{
			CHECK(ec.code == TCPError::Code::Cancelled && !sock);
			sem.notify();
		});
	}
	io.stop();
	for (int i = 0; i < 5; i++)
	{
		sem.wait();
	}

	th.join();
}


TEST(TCPSocket_recv_Success)
{
	TCPService io;
	auto th = std::thread([&io]
	{
		while (io.tick()) {}
	});

	TCPError ec;
	auto acceptor = io.listen(SERVER_PORT, 1, ec);
	CHECK(!ec && acceptor);

	Semaphore sem;
	std::shared_ptr<TCPSocket> serverSock;
	acceptor->accept([&sem, &serverSock](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
	{
		serverSock = sock;
		sem.notify();
	});

	auto sock = io.connect("127.0.0.1", SERVER_PORT, ec);
	CHECK(!ec && sock);
	sem.wait();

	// Test sending multiple chunks, but having it received in 1
	serverSock->send("AB", 2, [&sem](const TCPError& ec, int bytesTransfered) { sem.notify(); });
	serverSock->send("CD", 3, [&sem](const TCPError& ec, int bytesTransfered) { sem.notify(); });
	TCPBuffer buf(10);
	sem.wait(); sem.wait(); // Wait for both sends to finish, so we can test receiving in one chunk
	sock->recv(buf, [buf, &sem](const TCPError& ec, int bytesTransfered)
	{
		CHECK(bytesTransfered == 5);
		CHECK_EQUAL("ABCD", buf.ptr());
		sem.notify();
	});
	sem.wait(); // wait for the recv to process

	// Test sending multiple chunks with some delay, and have the receiver receive in different chunks
	serverSock->send("AB", 2, [&serverSock](const TCPError& ec, int bytesTransfered)
	{
		CHECK(bytesTransfered == 2);
		serverSock->send("CD", 3, nullptr);
	});

	TCPBuffer buf1(1);
	sock->recv(buf1, [buf1, &sem, sock](const TCPError& ec, int bytesTransfered)
	{
		CHECK(bytesTransfered == 1);
		CHECK(buf1.ptr()[0] == 'A');
		sem.notify();

		// Read the rest
		TCPBuffer buf2(10);
		sock->recv(buf2, [buf2, &sem](const TCPError& ec, int bytesTransfered)
		{
			CHECK(bytesTransfered == 4);
			CHECK_EQUAL("BCD", buf2.ptr());
			sem.notify();
		});
	});
	sem.wait(); // First recv
	sem.wait(); // second recv

	io.stop();
	th.join();
}

TEST(TCPSocket_recv_Failure)
{
	TCPService io;
	auto th = std::thread([&io]
	{
		while (io.tick()) {}
	});

	TCPError ec;
	auto acceptor = io.listen(SERVER_PORT, 1, ec);
	CHECK(!ec && acceptor);

	Semaphore sem;
	std::shared_ptr<TCPSocket> serverSock;
	acceptor->accept([&sem, &serverSock](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
	{
		CHECK(!ec && sock);
		serverSock = sock;
		sem.notify();
	});
	// Setup another accept for the same server side socket, so we can test disconnect,
	// then connect again to test canceling recv through the TCPService
	acceptor->accept([&sem, &serverSock](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
	{
		CHECK(!ec && sock);
		serverSock = sock;
		sem.notify();
	});

	auto sock = io.connect("127.0.0.1", SERVER_PORT, ec);
	CHECK(!ec && sock);
	sem.wait(); // wait for the first accept

	//
	// Test canceling directly with the socket
	//
	for (int i = 0; i < 10; i++)
	{
		TCPBuffer buf(10);
		sock->recv(buf, [buf, &sem](const TCPError& ec, int bytesTransfered)
		{
			CHECK(ec.code == TCPError::Code::Cancelled && bytesTransfered == 0);
			sem.notify();
		});
	}
	sock->cancel();
	for (int i = 0; i < 10; i++)
	{
		sem.wait();
	}

	//
	// Test connection closed
	//
	TCPBuffer buf(10);
	sock->recv(buf, [buf, &sem](const TCPError& ec, int bytesTransfered)
	{
		CHECK(ec.code == TCPError::Code::ConnectionClosed && bytesTransfered == 0);
		sem.notify();
	});
	// Closing the server side socket, will cause our client to detect it
	serverSock = nullptr;
	sem.wait();

	//
	// Test canceling recv through the TCPService
	//
	sock = io.connect("127.0.0.1", SERVER_PORT, ec);
	CHECK(!ec && sock);
	sem.wait(); // wait for the second accept
	CHECK(serverSock);
	for (int i = 0; i < 10; i++)
	{
		TCPBuffer buf(10);
		sock->recv(buf, [buf, &sem](const TCPError& ec, int bytesTransfered)
		{
			CHECK(ec.code == TCPError::Code::Cancelled && bytesTransfered == 0);
			sem.notify();
		});
	}
	io.stop();
	for (int i = 0; i < 10; i++)
	{
		sem.wait();
	}

	th.join();
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


// Test asynchronous listen and accept, but servicing on the same thread
/*
TEST(Async_Listen_And_Connect_SingleThreaded)
{
	TCPService io;
	bool done = false;

	// First get an acceptor
	io.listen(SERVER_PORT, [](const TCPError& ec, std::shared_ptr<TCPAcceptor> acceptor)
	{
		CHECK(!ec);


		// When we get an acceptor, wait for a connection
		acceptor->accept([](const TCPError&ec, std::shared_ptr<TCPSocket> sock)
		{
			CHECK(!ec);
			TCPBuffer buf(256);
			sock->recv(buf, [buf, sock](const TCPError& ec, int bytesTransfered)
			{
				CHECK(!ec);
				CHECK(bytesTransfered == 7);
				CHECK_EQUAL("HELLO!", buf.ptr());
				sock->send("OK!", 4, nullptr);
			});
		});
	});

	//
	io.connect("127.0.0.1", SERVER_PORT, [&io, &done](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
	{
		CHECK(!ec);
		sock->send("HELLO!", 7, [](const TCPError& ec, int bytesTransfered)
		{
			CHECK(!ec);
		});
		TCPBuffer buf(4);
		sock->recv(buf, [buf, &io, &done](const TCPError& ec, int bytesTransfered)
		{
			CHECK(!ec);
			CHECK(bytesTransfered == 4);
			CHECK_EQUAL("OK!", buf.ptr());
			io.stop();
			done = true;
		});
	});

	while(io.tick()) { }
	CHECK(done);
}
*/

// Test asynchronous listen and accept, with servicing in a different thread
/*
TEST(Async_Listen_And_Connect_Multithreaded)
{
	TCPService io;

	auto th = std::thread([&io]
	{
		while(io.tick()) {}
	});

	TCPError ec;
	// synchronous listen
	auto acceptor = io.listen(SERVER_PORT, ec);
	CHECK(!ec);

	ZeroSemaphore sem;
	sem.increment();
	sem.increment();

	acceptor->accept([&sem](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
	{
		CHECK(!ec);
		TCPBuffer buf(10);
		sock->recv(buf, [buf, &sem](const TCPError& ec, int bytesTransfered)
		{
			CHECK(!ec);
			CHECK(bytesTransfered == 4);
			CHECK_EQUAL("ABC", buf.ptr());
			sem.decrement();
		});

		sock->send("DEF", 4, [](const TCPError& ec, int bytesTransfered)
		{
			CHECK(!ec);
		});
	});

	// synchronous connect
	auto sock = io.connect("127.0.0.1", SERVER_PORT, ec);
	CHECK(!ec);
	TCPBuffer buf(10);
	sock->recv(buf, [buf, &sem](const TCPError& ec, int bytesTransfered)
	{
		CHECK(ec.code==TCPError::Code::Success || ec.code==TCPError::Code::ConnectionClosed);
		CHECK(bytesTransfered == 4);
		CHECK_EQUAL("DEF", buf.ptr());
		sem.decrement();
	});
	sock->send("ABC", 4, nullptr);

	sem.wait();
	io.stop();
	th.join();
}
*/

/*
TEST(Failure_Listen_Accept_Connect)
{
	TCPService io;
	auto th = std::thread([&io]
	{
		while(io.tick()) {}
	});

	Semaphore sem;
	TCPError ec;
	// Test synchronous listen failure
	auto acceptor = io.listen(SERVER_UNUSABLE_PORT, ec);
	CHECK(ec && !acceptor); // Needs to fail, since the port should not be available

	// Test asynchronous listen failure
	io.listen(SERVER_UNUSABLE_PORT, [&sem](const TCPError& ec, std::shared_ptr<TCPAcceptor> acceptor)
	{
		CHECK(ec && !acceptor); // Needs to fail, since the port should not be available
		sem.notify();
	});
	sem.wait();

	// Test synchronous connect failure
	auto sock = io.connect("127.0.0.1", SERVER_PORT, ec);
	CHECK(ec && !sock);

	// Test asynchronous connect failure
	io.connect("127.0.0.1", SERVER_PORT, [&sem](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
	{
		CHECK(ec && !sock); // Need to fail
		sem.notify();
	});
	sem.wait();

	// 
	// Test connection to an acceptor that is only accepting 1 connection
	acceptor = io.listen(SERVER_PORT, ec, 1);
	CHECK(!ec && acceptor);
	acceptor->accept([&sem](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
	{
		CHECK(!ec && sock);
		sem.notify();
	});
	auto sock1 = io.connect("127.0.0.1", SERVER_PORT, ec);
	CHECK(!ec && sock1); // First connect should work, since the server accepts it
	auto sock2 = io.connect("127.0.0.1", SERVER_PORT, ec);
	CHECK(!ec && sock2); // Second should work, because of the backlog
	auto sock3 = io.connect("127.0.0.1", SERVER_PORT, ec);
	// Third should fail because the backlog is not big enough.
	// This might not work properly on all Operating Systems, since the backlog setting
	// is just an hint to the OS
	CHECK(ec && !sock3); 

	sock2->send("ABC", 4, [&sem](const TCPError& ec, int bytesTransfered)
	{
		CHECK(!ec);
		sem.notify();
	});

	sem.wait();
	sem.wait();

	io.stop();
	th.join();
}
*/

/*
TEST(Callbacks_Cancel_Listen_Connect)
{
	TCPService io;
	int count=0;

	io.setAsyncOpsDebugSleep(10); // Set small debug delay, so we have time to cancel the operation
	io.listen(SERVER_PORT, [&count](const TCPError& ec, std::shared_ptr<TCPAcceptor> acceptor)
	{
		CHECK(ec.code==TCPError::Code::Cancelled);
		CHECK(!acceptor);
		count++;
	});
	io.connect("127.0.0.1", SERVER_PORT, [&count](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
	{
		CHECK(ec.code==TCPError::Code::Cancelled);
		CHECK(!sock);
		count++;
	});
	io.stop();
	while( io.tick() ) {}
	CHECK(count == 2);
}
*/

/*
TEST(Callbacks_Cancel_Accept)
{
	TCPService io;
	auto th = std::thread([&io]
	{
		while (io.tick()) {}
	});

	TCPError ec;
	auto acceptor = io.listen(SERVER_PORT, ec);
	CHECK(!ec);

	// First cancel directly on the acceptor
	Semaphore sem;
	acceptor->accept([&sem](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
	{
		CHECK(ec.code == TCPError::Code::Cancelled);
		CHECK(!sock);
		sem.notify();
	});
	acceptor->cancel();
	sem.wait();

	// Test by canceling from the TCPService
	acceptor->accept([&sem](const TCPError& ec, std::shared_ptr<TCPSocket> sock)
	{
		CHECK(ec.code == TCPError::Code::Cancelled);
		CHECK(!sock);
		sem.notify();
	});
	io.stop();
	sem.wait();

	th.join();
}
*/

/*
TEST(Callbacks_Cancel_Connect)
{
	TCPService io;
	auto th = std::thread([&io]
	{
		while (io.tick()) {}
	});

	TCPError ec;
	auto acceptor = io.listen(SERVER_PORT, ec);
	CHECK(!ec);

	auto sock = io.connect("127.0.0.1", SERVER_PORT, ec);
	CHECK(!ec);

	TCPBuffer buf(10);
	sock->recv(buf, [buf](const TCPError& ec, int bytesTransfered)
	{
		CHECK(ec.code == TCPError::Code::Cancelled);
		CHECK(bytesTransfered == 0);
	});

	th.join();
}
*/

}
