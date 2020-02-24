/*
Minimal sample

Does not use any helper classes to setup servers or clients, so a bit more verbose, but serves as an introduction.
It runs a Server and Client on separate threads, to simulate as if it was two separate processes.
*/

#include "MinimalPCH.h"

//////////////////////////////////////////////////////////////////////////
// RPC Interface
//////////////////////////////////////////////////////////////////////////

// Server class.
// The client calls functions on an instance of this class
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

//////////////////////////////////////////////////////////////////////////
//		Server
//////////////////////////////////////////////////////////////////////////

#define SERVER_PORT 9000

void runServer()
{
	// The class to be used for RPC calls doesn't need to know anything about RPCS, we can just create an instance
	Calc calc;

	// All the asynchronous work happens in a Service instance
	cz::spas::Service service;

	// For simplicity, we are only accepting 1 connection
	cz::rpc::SpasTransportAcceptor acceptor(service);

	// To setup an RPC connection, we need a rpc::Connection<LOCAL,REMOTE> object, and a transport.
	// The template parameters for the Connection:
	// LOCAL = Calc 
	//		This tells what interface we are using locally (as in, on our side of the connection)
	// REMOTE = void
	//		This tells what interface we are using on the peer's side, and since for this sample we are not using
	// bidirectional rpc connections, it is therefore void, as-in, a client doesn't have an interface to receive RPC
	// calls originating from the server
	cz::rpc::Connection<Calc, void> rpccon;
	cz::rpc::SpasTransport trp(service);

	// Setup the acceptor to accept just 1 connection
	// Notice how we specify the object to use for serving the rpc calls (calc).
	printf("SERVER: Starting on port %d\n", SERVER_PORT);
	cz::spas::Error ec = acceptor.listen(SERVER_PORT);
	if (ec)
	{
		printf("SERVER: Error starting the server socket: %s\n ", ec.msg());
		exit(EXIT_FAILURE);
	}

	acceptor.asyncAccept(nullptr, trp, rpccon, calc, [&](const cz::spas::Error& ec)
	{
		if (ec)
		{
			printf("SERVER: Error accepting a connection: %s\n", ec.msg());
		}
		else
		{
			printf("SERVER: Accepted connection from client\n");
		}

	});

	// Run the service.
	// This will return when there is no more asynchronous work pending and all connections are closed
	// That's why we are running this AFTER initiating the accept, otherwise it would return immediately.
	// Check the czspas documentation to understand why, and alternative ways to deal with this.
	service.run();
}


//////////////////////////////////////////////////////////////////////////
// Client
//////////////////////////////////////////////////////////////////////////

bool runClient()
{
	cz::spas::Service service;

	cz::rpc::Connection<void, Calc> rpccon;
	cz::rpc::SpasTransport trp(service);

	// Connect to the server
	cz::spas::Error ec = trp.connect(nullptr, rpccon, "127.0.0.1", SERVER_PORT);
	if (ec)
	{
		printf("CLIENT: Error connection to the server: %s\n", ec.msg());
		return false;
	}

	printf("CLIENT: Connected to server\n");

	// Now that we have an active rpc connection we can run the service, otherwise Service::run would return immediately
	// We run the service in a separate thread, so we can easily call rpcs and receive the replies without blocking
	std::thread iothread([&service]
	{
		printf("CLIENT: Service thread started\n");
		service.run();
		printf("CLIENT: Service thread finished\n");
	});

	//
	// We can handle RPC results in two ways (std::future or asynchronous callback)
	//

	// Handle the result with an std::future.
	std::future<cz::rpc::Result<int>> ft = CZRPC_CALL(rpccon, add, 1, 2).ft();
	// Block waiting for the future, and get the result from rpc::Result
	printf("add(1,2) = %d\n", ft.get().get());

	// Handle a result with an asynchronous callback
	// The callback is executed from service::run, which in this case means it's executed in another thread, so be
	// careful to protect any data that needs to be protected.
	CZRPC_CALL(rpccon, sub, 4, 3).async(
		[&rpccon](cz::rpc::Result<int> res)
	{
		printf("sub(4,3) = %d\n", res.get());
		// We won't be calling any more rpcs, so close the connection.
		// Since this is the only connection, it will cause the Service::run call to exit since there is no more
		// pending work
		rpccon.close();
	});

	// Once there is no more pending work, the service::run call in the iothread will return, and therefore finishing
	// that thread
	iothread.join();
	return true;
}


int main()
{
	std::thread server([]
	{
		runServer();
	});

	if (!runClient())
		return EXIT_FAILURE;

	server.join();
	return EXIT_SUCCESS;
}
