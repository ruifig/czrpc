/*
CalculatorServer

A slightly more complex rpc server, that uses some helper classes to make it shorter.
*/
#include "CalculatorServerPCH.h"
#include "CalculatorInterface.h"

#pragma warning(disable:4996)

#define CALCSERVER_DEFAULT_PORT 9000

using namespace cz;
using namespace cz::rpc;

//
// The calculator implementation
class Calculator : public CalculatorInterface
{
public:

	virtual float add(float a, float b) override
	{
		return a + b;
	}

	virtual float sub(float a, float b) override
	{
		return a - b;
	}

	// Example of an RPC that can execute asynchronous on the server, thus not returning right away to the client.
	// czrpc detects when a method returns a std::future, and deals with it accordingly on the server side.
	// From the client perspective, this kind of RPC is no different than the others, and its handled the same way.
	virtual std::future<float> sqrt(float a) override
	{
		return std::async(std::launch::async, [a]
		{
			return std::sqrt(a);
		});
	}
};

Parameters gParams;
int main(int argc, char* argv[])
{
	gParams.set(argc, argv);

	try
	{
		int port = CALCSERVER_DEFAULT_PORT;
		if (gParams.has("port"))
			port = std::stoi(gParams.get("port"));

		printf("Running CalculatorServer on port %d.\n", port);
		printf("Type any key to quit.\n");

		Calculator calc;
		SimpleServer<CalculatorInterface, void> server(calc, port);
		// Clients can use the "__getProperty" generic RPC to query properties, so we set the "name" property to
		// something that gives a clue about what the server is
		server.objData().setProperty("name", "calc");
		while (true)
		{
			if (my_getch())
				break;
		}
	}
	catch (const std::exception& e)
	{
		printf("Exception: %s\n", e.what());

	}
	return 0;
}
