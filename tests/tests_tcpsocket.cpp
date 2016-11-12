#include "testsPCH.h"

#define LONGTEST 0

#pragma warning(disable:4996)
#pragma warning(disable:4390)

// #TODO : Change CHECK to CHECK_EQUAL where appropriate

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

	TCPAcceptor acceptor(io);
	TCPError ec = acceptor.listen(SERVER_PORT, 1);
	CHECK(!ec && acceptor.isValid());

	io.stop();
	th.join();
}

TEST(TCPService_Listen_Failure)
{
	MyTCPLog::DisableLogging dummy;
	TCPService io;
	auto th = std::thread([&io]
	{
		while (io.tick()) {}
	});

	TCPAcceptor acceptor(io);
	TCPError ec = acceptor.listen(SERVER_UNUSABLE_PORT, 1);
	CHECK(ec && !acceptor.isValid());

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
	TCPAcceptor acceptor(io);
	TCPError ec = acceptor.listen(SERVER_PORT, 1);
	CHECK(!ec && acceptor.isValid());

	// Setup 2 accepts
	Semaphore sem;
	TCPSocket sock1(io);
	acceptor.asyncAccept(sock1, [&sem, &sock1](const TCPError& ec)
	{
		CHECK(!ec);
		CHECK(sock1.isValid());
		sem.notify();
	});
	TCPSocket sock2(io);
	acceptor.asyncAccept(sock2, [&sem, &sock2](const TCPError& ec)
	{
		CHECK(!ec);
		CHECK(sock2.isValid());
		sem.notify();
	});

	// Test synchronous connect
	{
		TCPSocket sock(io);
		ec = sock.connect("127.0.0.1", SERVER_PORT);
		CHECK(!ec);
		CHECK(sock.isValid());
		sem.wait(); // wait for the accept
	}

	// Test asynchronous connect
	{
		TCPSocket sock(io);
		sock.asyncConnect("127.0.0.1", SERVER_PORT, [&sem, &sock](const TCPError& ec)
		{
			CHECK(!ec);
			CHECK(sock.isValid());
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
	TCPAcceptor acceptor(io);
	TCPError ec = acceptor.listen(SERVER_PORT, 1);
	CHECK(!ec && acceptor.isValid());

	//
	// Test canceling 10 accepts directly with the acceptor
	//
	{
		Semaphore sem;
		int count = 10;
		for (int i = 0; i < count; i++)
		{
			auto sock = std::make_shared<TCPSocket>(io);
			acceptor.asyncAccept(*sock, [&sem, sock](const TCPError& ec)
			{
				CHECK_EQUAL((int)TCPError::Code::Cancelled, (int)ec.code);
				CHECK(!sock->isValid());
				sem.notify();
			});
		}
		acceptor.asyncCancel([] {});
		for (int i = 0; i < count; i++)
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
			auto sock = std::make_shared<TCPSocket>(io);
			acceptor.asyncAccept(*sock, [&sem, sock](const TCPError& ec)
			{
				CHECK_EQUAL((int)TCPError::Code::Cancelled, (int)ec.code);
				CHECK(!sock->isValid());
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
	MyTCPLog::DisableLogging dummy;
	TCPService io;
	auto th = std::thread([&io]
	{
		while (io.tick()) {}
	});

	//
	// Test synchronous connect failure
	//
	{
		TCPSocket sock(io);
		TCPError ec = sock.connect("127.0.0.1", SERVER_PORT);
		CHECK_EQUAL((int)TCPError::Code::Other, (int)ec.code);
		CHECK(!sock.isValid());
	}

	Semaphore sem;
	//
	// Test asynchronous connect failure, with different timeouts
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
		// Initially I was using "127.0.0.1" to test the asynchronous connect timeout, but it seems that on linux
		// it fails right away. Probably the kernel treats connections to the localhost in a different way, detecting
		// right away that if a connect is not possible, without taking into consideration the timeout specified in
		// the "select" function.
		// On Windows, connect attempts to localhost still take into consideration the timeout.
		// The solution is to try an connect to some external ip, like "254.254.254.254". This causes Linux to
		// to actually wait for the connect attempt.
		auto sock = std::make_shared<TCPSocket>(io);
		sock->asyncConnect("254.254.254.254", SERVER_PORT, [&sem, i, &times, &timer, sock](const TCPError& ec)
		{
			times[i] = timer.GetTimeInMs() - times[i];
			CHECK_EQUAL((int)TCPError::Code::ConnectFailed, (int)ec.code);
			CHECK(!sock->isValid());
			sem.notify();
		}, (int)expectedTimes[i]);
	}
	for (int i = 0; i < count; i++)
	{
		sem.wait();
	}
	CHECK_ARRAY_CLOSE(expectedTimes, times, count, 100.0f);

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

	TCPAcceptor acceptor(io);
	TCPError ec = acceptor.listen(SERVER_PORT, 1);
	CHECK(!ec && acceptor.isValid());

	Semaphore sem;
	TCPSocket serverSock(io);
	acceptor.asyncAccept(serverSock, [&sem, &serverSock](const TCPError& ec)
	{
		CHECK(!ec);
		CHECK(serverSock.isValid());
		sem.notify();
	});

	TCPSocket sock(io);
	ec = sock.connect("127.0.0.1", SERVER_PORT);
	CHECK(!ec && sock.isValid());
	sem.wait();

	// Test sending multiple chunks, but having it received in 1
	serverSock.asyncWrite("AB", 2, [&sem](const TCPError& ec, int bytesTransfered) { sem.notify(); });
	serverSock.asyncWrite("CD", 3, [&sem](const TCPError& ec, int bytesTransfered) { sem.notify(); });
	TCPBuffer buf(10);
	sem.wait(); sem.wait(); // Wait for both sends to finish, so we can test receiving in one chunk
	sock.asyncReadSome(buf, [buf, &sem](const TCPError& ec, int bytesTransfered)
	{
		CHECK(bytesTransfered == 5);
		CHECK_EQUAL("ABCD", buf.ptr());
		sem.notify();
	});
	sem.wait(); // wait for the recv to process

	// Test sending multiple chunks with some delay, and have the receiver receive in different chunks
	serverSock.asyncWrite("AB", 2, [&serverSock, &sem](const TCPError& ec, int bytesTransfered)
	{
		CHECK(bytesTransfered == 2);
		serverSock.asyncWrite("CD", 3, [&sem](const TCPError& ec, int bytesTransfered)
		{
			sem.notify();
		});
	});
	sem.wait(); // wait for the two chunks to be sent

	TCPBuffer buf1(1);
	sock.asyncReadSome(buf1, [buf1, &sem, &sock](const TCPError& ec, int bytesTransfered)
	{
		CHECK(bytesTransfered == 1);
		CHECK(buf1.ptr()[0] == 'A');
		sem.notify();

		// Read the rest
		TCPBuffer buf2(10);
		buf2.zero();
		sock.asyncReadSome(buf2, [buf2, &sem](const TCPError& ec, int bytesTransfered)
		{
			CHECK_EQUAL(4, bytesTransfered);
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

	TCPAcceptor acceptor(io);
	TCPError ec = acceptor.listen(SERVER_PORT, 1);
	CHECK(!ec && acceptor.isValid());

	Semaphore sem;
	TCPSocket serverSock1(io);
	acceptor.asyncAccept(serverSock1, [&sem, &serverSock1](const TCPError& ec)
	{
		CHECK(!ec);
		CHECK(serverSock1.isValid());
		sem.notify();
	});
	TCPSocket serverSock2(io);
	acceptor.asyncAccept(serverSock2, [&sem, &serverSock2](const TCPError& ec)
	{
		CHECK(!ec);
		CHECK(serverSock2.isValid());
		sem.notify();
	});

	auto sock = std::make_shared<TCPSocket>(io);
	ec = sock->connect("127.0.0.1", SERVER_PORT);
	CHECK(!ec && sock->isValid());
	sem.wait(); // wait for the first accept

	//
	// Test canceling directly with the socket
	//
	for (int i = 0; i < 10; i++)
	{
		TCPBuffer buf(10);
		sock->asyncReadSome(buf, [buf, &sem, sock](const TCPError& ec, int bytesTransfered)
		{
			CHECK_EQUAL((int)TCPError::Code::Cancelled, (int)ec.code);
			CHECK_EQUAL(0, bytesTransfered);
			sem.notify();
		});
	}
	sock->asyncCancel([] {});
	for (int i = 0; i < 10; i++)
	{
		sem.wait();
	}

	//
	// Test connection closed
	//
	TCPBuffer buf(10);
	sock->asyncReadSome(buf, [buf, &sem, sock](const TCPError& ec, int bytesTransfered)
	{
		CHECK_EQUAL((int)TCPError::Code::ConnectionClosed, (int)ec.code);
		CHECK_EQUAL(0, bytesTransfered);
		sem.notify();
	});
	// Closing the server side socket will cause our client to detect it and cancel the pending operations
	serverSock1.asyncClose([] {});
	sem.wait();

	//
	// Test canceling recv through the TCPService
	//
	sock = std::make_shared<TCPSocket>(io);
	ec = sock->connect("127.0.0.1", SERVER_PORT);
	CHECK(!ec && sock->isValid());
	sem.wait(); // wait for the second accept
	CHECK(serverSock2.isValid());
	for (int i = 0; i < 10; i++)
	{
		TCPBuffer buf(10);
		sock->asyncReadSome(buf, [buf, &sem, sock](const TCPError& ec, int bytesTransfered)
		{
			CHECK_EQUAL((int)TCPError::Code::Cancelled, (int)ec.code);
			CHECK_EQUAL(0, bytesTransfered);
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

// Test if the TCPSocket gets destroyed when we call cancel and we don't hold any strong references
TEST(TCPSocket_cancel_lifetime)
{
	TCPService io;
	auto th = std::thread([&io]
	{
		while (io.tick()) {}
	});


	TCPAcceptor acceptor(io);
	TCPError ec = acceptor.listen(SERVER_PORT, 1);
	CHECK(!ec && acceptor.isValid());

	auto sock = std::make_shared<TCPSocket>(io);
	ec = sock->connect("127.0.0.1", SERVER_PORT);
	CHECK(!ec && sock->isValid());
	// This should not do anything, since there aren't any pending asynchronous operations yet
	sock->asyncCancel([] {});

	Semaphore sem;
	const int count = 10;
	for(int i=0; i<count; i++)
	{
		TCPBuffer buf(10);
		// Initiate asynchronous operations, passing a strong reference to the handler to keep the
		// socket alive
		sock->asyncReadSome(buf, [&sem, sock](const TCPError& ec, int bytesTransfered)
		{
			CHECK(ec.code == TCPError::Code::Cancelled && bytesTransfered == 0);
			sem.notify();
		});
	}

	// Get a weak reference and release our strong one, so we can check if it was indeed destroyed
	// when all operations were cancelled and the strong references passed to the handlers were correctly lost
	std::weak_ptr<TCPSocket> wsock = sock;
	sock->asyncCancel([sock] {});
	sock = nullptr; // Release our strong reference

	// wait for all the handlers to be cancelled, so all the passed strong references are lost
	for (int i = 0; i < count; i++)
	{
		sem.wait();
	}

	// Small delay to make sure the TCPIOService has time to remove strong references after calling the handlers.
	// This is needed, because after we notify the semaphore, this thread might get here before the service
	// has time to remove the strong references from the internal containers
	UnitTest::TimeHelpers::SleepMs(10);
	CHECK(wsock.lock()==nullptr);

	io.stop();
	th.join();
}

TEST(TCPAcceptor_backlog)
{
	MyTCPLog::DisableLogging dummy;
	TCPService io;
	auto th = std::thread([&io]
	{
		while(io.tick()) {}
	});

	Semaphore sem;
	TCPAcceptor acceptor(io);
	TCPError ec = acceptor.listen(SERVER_PORT, 1);
	CHECK(!ec && acceptor.isValid());

	// Add only 1 accept operation, so the server only accepts 1 incoming connection.
	// Extra connection attempts by clients can still succeed to connect due to the backlog, but doesn't mean
	// the server will accept the connection
	TCPSocket serverSock(io);
	acceptor.asyncAccept(serverSock, [&sem](const TCPError& ec)
	{
		CHECK(!ec);
		sem.notify();
	});

	TCPSocket sock1(io);
	ec = sock1.connect("127.0.0.1", SERVER_PORT);
	CHECK(!ec); CHECK(sock1.isValid()); // First connect should work, since the server accepts it
	TCPSocket sock2(io);
	ec = sock2.connect("127.0.0.1", SERVER_PORT);
	CHECK(!ec); CHECK(sock2.isValid()); // Second should work, because of the backlog
	TCPSocket sock3(io);
	ec = sock3.connect("127.0.0.1", SERVER_PORT);
	// Third should fail because the backlog is not big enough.
	// This might not work properly on all Operating Systems, since the backlog setting
	// is just an hint to the OS.
	// It seems to work as I expect on Windows, but not Linux:
	// See: http://veithen.github.io/2014/01/01/how-tcp-backlog-works-in-linux.html
#if _WIN32
	CHECK(ec); CHECK(!sock3.isValid());
#endif

	sem.wait(); // Wait for 1 accept to run
	// Close the acceptor.
	// This should cause the following state:
	// sock1 - Is fully connected already because of the accept we allowed
	// sock2 - Is connected, but the server didn't accept it
	// sock3 - OS dependent
	acceptor.asyncClose([&sem]
	{
		sem.notify();
	});
	sem.wait(); // wait for the close to finish

	// This send should fail because although we connected, the acceptor never accepted the connection
	sock2.asyncWrite("ABC", 4, [&sem](const TCPError& ec, int bytesTransfered)
	{
		CHECK_EQUAL((int)TCPError::Code::ConnectionClosed, (int)ec.code);
		CHECK_EQUAL(0, bytesTransfered);
		sem.notify();
	});
	sem.wait(); // Wait for the write to finish

	io.stop();
	th.join();
}

void testLatency_serverRecv(TCPSocket& sock)
{
	TCPBuffer buf(1);
	sock.asyncReadSome(buf, [buf, &sock](TCPError ec, int bytesTransfered)
	{
		if (ec)
			return;
		CHECK_EQUAL(1, bytesTransfered);
		sock.asyncWrite(buf, [buf](const TCPError& ec, int bytesTransfered) {});
		testLatency_serverRecv(sock);
	});
}

TEST(Latency)
{
	TCPService io;
	auto th = std::thread([&io]
	{
		while(io.tick()) {}
	});

	UnitTest::Timer timer;
	timer.Start();

	const int count = (LONGTEST) ? 200 : 20;
	const int waitMs = 5;
	std::vector<std::pair<double, char>> times(count, std::make_pair(double(0), char(0)));

	TCPAcceptor acceptor(io);
	TCPError ec = acceptor.listen(SERVER_PORT, 1);

	TCPSocket serverSock(io);
	acceptor.asyncAccept(serverSock, [&serverSock](const TCPError& ec)
	{
		testLatency_serverRecv(serverSock);
	});

	TCPSocket client(io);
	ec = client.connect("127.0.0.1", SERVER_PORT);
	CHECK(!ec && client.isValid());

	Semaphore sem;
	for(int i=0; i<count; i++)
	{
		// Setup receive first, to use the best case scenario where the data is sent really fast.
		// If the sent is done first, it might happen the thread is put on hold before preparing the receive, causing the
		// sent data to take a bit more to arrive.
		{
			TCPBuffer rcvBuf(1);
			client.asyncReadSome(rcvBuf, [&sem, count, rcvBuf, i, &times, &timer](const TCPError& ec, int bytesTransfered)
			{
				CHECK_EQUAL(1, bytesTransfered);
				CHECK_EQUAL((char)i, *rcvBuf.ptr());
				times[i].first = timer.GetTimeInMs() - times[i].first;
				times[i].second = *rcvBuf.ptr();
				if (i == count - 1)
					sem.notify();
			});
		}

		// Do the send
		{
			TCPBuffer sndBuf(1);
			*sndBuf.ptr() = (char)i;
			times[i].first = timer.GetTimeInMs();
			client.asyncWrite(sndBuf, [sndBuf](const TCPError& ec, int bytesTransfered)
			{
			});
		}

		// Small pause so we don't saturate, since we are just testing latency
		UnitTest::TimeHelpers::SleepMs(waitMs);
	}

	sem.wait();

	double low = std::numeric_limits<double>::max();
	double high = std::numeric_limits<double>::min();
	double total = 0;
	for (auto&& t : times)
	{
		low = std::min(low, t.first);
		high = std::max(high, t.first);
		total += t.first;
	}
	printf("RPCTCPSocket latency (round-trip) (%d 1-byte messages, wait time between sends=%dms)\n", count, waitMs);
	printf("        min=%0.4fms\n", low);
	printf("        max=%0.4fms\n", high);
	printf("        avg=%0.4fms\n", total/count);

	io.stop();
	th.join();
}

struct ThroughputData
{
	uint64_t done = 0;
	TCPSocket sock;
	TCPBuffer buf;
	Semaphore finished;
	ThroughputData(TCPService& io)
		: sock(io)
		, buf(1 * 1024 * 1024/4)
	{}

	void setupSend()
	{
		sock.asyncWrite(buf, [this](const TCPError& ec, int bytesTransfered)
		{
			done += bytesTransfered;
			if (ec)
			{
				sock.asyncClose([] {});
				finished.notify();
			}
			else
				setupSend();
		});
	}

	void setupReceive()
	{
		sock.asyncReadSome(buf, [this](const TCPError& ec, int bytesTransfered)
		{
			if (ec)
			{
				finished.notify();
				return;
			}

			done += bytesTransfered;
			setupReceive();
		});
	}
};

TEST(Throughput)
{
	TCPService ioRcv;
	auto thRcv = std::thread([&ioRcv]
	{
		while(ioRcv.tick()) {}
	});

	TCPService ioSnd;
	auto thSnd = std::thread([&ioSnd]
	{
		while(ioSnd.tick()) {}
	});

	TCPAcceptor acceptor(ioRcv);
	TCPError ec = acceptor.listen(SERVER_PORT, 1);
	CHECK(!ec && acceptor.isValid());

	ThroughputData snd(ioSnd), rcv(ioRcv);

	Semaphore sem;
	acceptor.asyncAccept(rcv.sock, [&sem](const TCPError& ec)
	{
		CHECK(!ec);
		sem.notify();
	});

	ec = snd.sock.connect("127.0.0.1", SERVER_PORT);
	CHECK(!ec && snd.sock.isValid());

	sem.wait();

	uint64_t rcvDone = 0;
	uint64_t sndDone = 0;
	UnitTest::Timer timer;
	timer.Start();
	auto start = timer.GetTimeInMs();
	rcv.setupReceive();
	snd.setupSend();
	UnitTest::TimeHelpers::SleepMs((LONGTEST) ? 30000 : 4000);
	snd.sock.asyncClose([] {});
	rcv.finished.wait();
	auto end = timer.GetTimeInMs();
	ioRcv.stop();
	ioSnd.stop();
	thRcv.join();
	thSnd.join();

	auto seconds = (end - start) / 1000;
	auto mb = (double)rcv.done/(1000*1000);
	printf("TCPSocket throughput: %0.2f Mbit/s (%0.2f MB/s)\n", (mb*8)/seconds, mb/seconds);
}

}
