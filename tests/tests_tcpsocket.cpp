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
#define TCPINFO(...) ((void)0)
#define TCPERROR(...) ((void)0)
}
}

#include "crazygaze/rpc/RPCTCPSocket.h"

using namespace cz;
using namespace cz::rpc;

#define SERVER_PORT 9000

// A port we know its not available, so we can test listen failure
// On windows we use epmap (port 135)
#define SERVER_UNUSABLE_PORT 135

SUITE(TCPSocket)
{

#if 1
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

	//
	// Test synchronous connect failure
	//
	TCPError ec;
	auto sock = io.connect("127.0.0.1", SERVER_PORT, ec);
	CHECK(ec.code==TCPError::Code::Other && !sock);

	Semaphore sem;
	//
	// Test asynchronous connect failure
	//
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

	io.stop();
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
	serverSock->asyncSend("AB", 2, [&sem](const TCPError& ec, int bytesTransfered) { sem.notify(); });
	serverSock->asyncSend("CD", 3, [&sem](const TCPError& ec, int bytesTransfered) { sem.notify(); });
	TCPBuffer buf(10);
	sem.wait(); sem.wait(); // Wait for both sends to finish, so we can test receiving in one chunk
	sock->asyncRecv(buf, [buf, &sem](const TCPError& ec, int bytesTransfered)
	{
		CHECK(bytesTransfered == 5);
		CHECK_EQUAL("ABCD", buf.ptr());
		sem.notify();
	});
	sem.wait(); // wait for the recv to process

	// Test sending multiple chunks with some delay, and have the receiver receive in different chunks
	serverSock->asyncSend("AB", 2, [&serverSock](const TCPError& ec, int bytesTransfered)
	{
		CHECK(bytesTransfered == 2);
		serverSock->asyncSend("CD", 3, nullptr);
	});

	TCPBuffer buf1(1);
	sock->asyncRecv(buf1, [buf1, &sem, sock](const TCPError& ec, int bytesTransfered)
	{
		CHECK(bytesTransfered == 1);
		CHECK(buf1.ptr()[0] == 'A');
		sem.notify();

		// Read the rest
		TCPBuffer buf2(10);
		sock->asyncRecv(buf2, [buf2, &sem](const TCPError& ec, int bytesTransfered)
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

#endif 

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
		sock->asyncRecv(buf, [buf, &sem](const TCPError& ec, int bytesTransfered)
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
	sock->asyncRecv(buf, [buf, &sem](const TCPError& ec, int bytesTransfered)
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
		sock->asyncRecv(buf, [buf, &sem](const TCPError& ec, int bytesTransfered)
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

#if 1

// Test if the TCPSocket gets destroyed when we call cancel and we don't hold any strong references
TEST(TCPSocket_cancel_lifetime)
{
	TCPService io;
	auto th = std::thread([&io]
	{
		while (io.tick()) {}
	});

	TCPError ec;
	auto acceptor = io.listen(SERVER_PORT, 1, ec);
	CHECK(!ec && acceptor);

	auto sock = io.connect("127.0.0.1", SERVER_PORT, ec);
	CHECK(!ec && sock);
	// This should not do anything, since there aren't any pending asynchronous operations yet
	sock->cancel();

	Semaphore sem;
	for(int i=0; i<10; i++)
	{
		TCPBuffer buf(10);
		sock->asyncRecv(buf, [&sem](const TCPError& ec, int bytesTransfered)
		{
			CHECK(ec.code == TCPError::Code::Cancelled && bytesTransfered == 0);
			sem.notify();
		});
	}

	// Cancel all outstanding asynchronous operations
	sock->cancel();
	// Now, release our strong reference (and keep a weak reference).
	// The above cancel should cause the socket to be destroyed since all the strong references kept
	// by the TCPIOService should be removed
	std::weak_ptr<TCPSocket> wsock = sock;
	sock = nullptr; // remove our strong reference
	// wait for all the handlers to be cancelled, so TCPIOService loses all the strong references
	for (int i = 0; i < 10; i++)
	{
		sem.wait();
	}
	// Small delay to make sure the TCPIOService has time to remove strong references after calling the handlers. 
	// This is needed, because after we notify the semaphore, this thread might get here because the the io service
	// has time to remove the strong references from the internal containers
	UnitTest::TimeHelpers::SleepMs(10);
	CHECK(wsock.lock()==nullptr);

	io.stop();
	th.join();
}

TEST(TCPAcceptor_backlog)
{
	TCPService io;
	auto th = std::thread([&io]
	{
		while(io.tick()) {}
	});

	Semaphore sem;
	TCPError ec;
	auto acceptor = io.listen(SERVER_PORT, 1, ec);
	CHECK(!ec && acceptor);

	// Add only 1 accept operation, so the server only accepts 1 incoming connection.
	// Extra connection attempts by clients can still succeed to connect due to the backlog, but doesn't mean
	// the server will accept the connection
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

	sem.wait(); // Wait for the server accept
	CHECK(acceptor.unique());
	acceptor = nullptr; // Causes the acceptor to be destroy.

	// This send should fail because although we connected, the Acceptor never accepted the connection
	// and it should be closed once the Acceptor is destroyed
	sock2->asyncSend("ABC", 4, [&sem](const TCPError& ec, int bytesTransfered)
	{
		CHECK(ec && bytesTransfered == 0);
		sem.notify();
	});

	sem.wait();

	io.stop();
	th.join();
}

#endif

}
