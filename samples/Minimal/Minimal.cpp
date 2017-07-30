/*
Minimal sample

Does not use any helper classes to setup servers or clients, so a bit more verbose, but serves as an introduction.
It runs an RPC server on on thread, and a client on another, to simulate as if it was two separate processes.
*/

#include "MinimalPCH.h"

//
// Server class.
// The client calls functions on an instance of this class
//
class Calc
{
  public:
	int add(int a, int b) { return a + b; }
	int sub(int a, int b) { return a - b; }
};

//
// Setup the RPC table for the Calc class, so it can be used for RPC calls
//
#define RPCTABLE_CLASS Calc
#define RPCTABLE_CONTENTS \
	REGISTERRPC(add) \
	REGISTERRPC(sub)
#include "crazygaze/rpc/RPCGenerate.h"

#define SERVER_PORT 9000

// Run a server
void runServer()
{
	// The class to be used for RPC calls doesn't need to know anything about RPCS, we can just create an instance
	Calc calc;
	// All the asynchronous work happens in a Service instance
	cz::spas::Service service;

	// For simplicity, we are only accepting 1 connection
	cz::rpc::SpasTransportAcceptor acceptor(service);

	// To setup an RPC connection, we need a rpc::Connection<LOCAL,REMOTE> object, and a transport
	// LOCAL = Calc  - This tells what interface we are using locally (as in, on our side of the connection)
	// REMOTE = void - This tells what interface we are using on the peer's side, and since for this sample
	// we are not using bidirectional rpc connections, it is therefore void, as-in, a client doesn't have an interface
	// to receive RPC calls originating from the server
	cz::rpc::Connection<Calc, void> rpccon;
	cz::rpc::SpasTransport trp(service);

	// Setup the acceptor to accept just 1 connection
	// Notice how we specify the object to use for serving the rpc calls (calc).
	std::cout << "SERVER: Starting on port " << SERVER_PORT << "\n";
	cz::spas::Error ec = acceptor.listen(SERVER_PORT);
	if (ec)
	{
		std::cerr << "SERVER: Error starting the server socket: " << ec.msg() << "\n";
		return;
	}

	acceptor.asyncAccept(nullptr, trp, rpccon, calc, [&](const cz::spas::Error& ec)
	{
		if (ec)
		{
			std::cerr << "SERVER: Error accepting a connection: " << ec.msg() << "\n";
			return;
		}

		std::cout << "SERVER: Accepted connection from client\n";
	});

	// Run the service.
	// This will return when there is no more asynchronous work pending and all connections are closed
	// That's why we are running this AFTER initiating the accept, otherwise it would return immediately.
	// Check the czspas documentation to understand why, and alternative ways to deal with this.
	service.run();
}


#if 0
void runClient()
{
	cz::spas::Service service;

	// From the client perspective, the template parameters for cz::rpc::Connection<LOCAL,REMOTE>:
	// LOCAL = void - The client doesn't have any local interface to receive rpc calls originated from the server.
	// It can only receive replies to rpc calls initiated on the client.
	// REMOTE = Calc - This is the interface the server has. The client is will only be able to call methods that
	// were registered in the rpc table for the Calc type. See the rpc table registration above
	cz::rpc::Connection<void, Calc> rpccon;
	cz::rpc::SpasTransport trp(service);

	// For simplicity, do a synchronous connect, so we don't need to have the service running yet
	cz::spas::Error ec = trp.connect(nullptr, rpccon, "127.0.0.1", SERVER_PORT);
	if (ec)
	{
		std::cerr << "CLIENT: Error connection to the server: " << ec.msg() << "\n";
		return;
	}

	std::cout << "CLIENT: Connected to server\n";

	// Now that we have an active rpc connection we can run the service, otherwise Service::run would return immediately
	// We run the service in a separate thread, so we can easily call rpcs one after another.
	std::thread iothread([&service]
	{
		std::cout << "CLIENT IO THREAD: Started\n";
		service.run();
		std::cout << "CLIENT IO THREAD: Finished\n";
	});

	//
	// Call an rpc, and handle the result with an std::future.
	// This is possible because Service is running on a separate thread.
	std::future<cz::rpc::Result<int>> ft = CZRPC_CALL(rpccon, add, 1, 2).ft();
	// Block waiting for the future, and get the result from rpc::Result
	std::cout << "1+2 = " << ft.get().get() << "\n";

	//
	// Call an rpc using the async version
	// In this case, the reply is handled with a lambda callback
	// The callback is executed from service::run, which in this case means it's executed in another thread
	CZRPC_CALL(rpccon, sub, 4, 3).async([](cz::rpc::Result<int> res)
	{
		std::cout << "4-3 = " << res.get() << "\n";
	});

	// Once there is no more pending work, the service::run call in the iothread will return, and therefore finishing
	// that thread
	iothread.join();
}

#else

struct ClientSession : cz::rpc::Session
{
	ClientSession(cz::spas::Service& service) : trp(service) {}
	~ClientSession()
	{
	}
	cz::rpc::Connection<void, Calc> rpccon;
	cz::rpc::SpasTransport trp;
};

void runClient()
{
	cz::spas::Service service;

	auto session = std::make_shared<ClientSession>(service);

	cz::spas::Error ec = session->trp.connect(session, session->rpccon, "127.0.0.1", SERVER_PORT);
	if (ec)
	{
		std::cerr << "CLIENT: Error connection to the server: " << ec.msg() << "\n";
		return;
	}

	std::cout << "CLIENT: Connected to server\n";

	// Now that we have an active rpc connection we can run the service, otherwise Service::run would return immediately
	// We run the service in a separate thread, so we can easily call rpcs one after another.
	std::thread iothread([&service]
	{
		std::cout << "CLIENT IO THREAD: Started\n";
		service.run();
		std::cout << "CLIENT IO THREAD: Finished\n";
	});

	//
	// Call an rpc, and handle the result with an std::future.
	// This is possible because Service is running on a separate thread.
	std::future<cz::rpc::Result<int>> ft = CZRPC_CALL(session->rpccon, add, 1, 2).ft();
	// Block waiting for the future, and get the result from rpc::Result
	std::cout << "1+2 = " << ft.get().get() << "\n";

	//
	// Call an rpc using the async version
	// In this case, the reply is handled with a lambda callback
	// The callback is executed from service::run, which in this case means it's executed in another thread
	CZRPC_CALL(session->rpccon, sub, 4, 3).async([session](cz::rpc::Result<int> res)
	{
		std::cout << "4-3 = " << res.get() << "\n";
		// We won't be calling any more rpcs, so close the connection.
		// 
		//session->rpccon.close();
	});

	session->rpccon.close();
	session = nullptr;

	// Once there is no more pending work, the service::run call in the iothread will return, and therefore finishing
	// that thread
	iothread.join();
}
#endif


int main()
{
	std::thread server([]
	{
		runServer();
	});

	runClient();

	server.join();
	return 0;
}
