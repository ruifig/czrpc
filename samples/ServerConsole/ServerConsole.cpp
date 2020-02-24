#include "ServerConsolePCH.h"


#pragma warning(disable: 4996)

#include "ServerConsoleCommand.h"
using namespace cz::rpc;
using namespace cz;

Data* gData;

int main()
{
	// Run the service thread
	Data data;
	auto keepAliveWork = std::make_unique<spas::Service::Work>(data.service);
	std::thread ioth([&data]
	{
		data.service.run();
	});
	gData = &data;

	std::cout << "Type :h for help\n";
	CommandLineReader cmdReader("COMMAND> ");

	// #TODO Remove this
	gData->service.post([]
	{
		processCommand(":c \"127.0.0.1:9000\"");
	});

	while(!gData->finish)
	{
		std::string cmd;
		while(!cmdReader.tryGet(cmd))
			std::this_thread::sleep_for(std::chrono::milliseconds(5));

		gData->service.post([cmd=std::move(cmd)]
		{
			processCommand(cmd);
		});
	}

	// Release our dummy work item, and close all active connections.
	// This causes the Service::run to exit when there is no more work to be done
	keepAliveWork = nullptr;
	gData->service.post([&]
	{
		for (auto&& c : gData->cons)
			c.second->con.close();
	});
	// At this point everything should be closing automatically
	ioth.join();
	assert(gData->cons.size() == 0);

    return 0;
}

