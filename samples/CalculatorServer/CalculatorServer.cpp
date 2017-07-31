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

	virtual std::future<float> sqrt(float a) override
	{
		return std::async([a]
		{
			return std::sqrt(a);
		});
	}

};

CZRPC_ALLOW_CONST_LVALUE_REFS;

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
