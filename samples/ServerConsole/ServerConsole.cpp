#include "ServerConsolePCH.h"


#pragma warning(disable: 4996)

#include "ServerConsoleCommand.h"
using namespace cz::rpc;

std::unordered_map<std::string, std::shared_ptr<ConInfo>> gCons;
std::shared_ptr<ASIO::io_service> gIOService;

int main()
{
	gIOService = std::make_shared<ASIO::io_service>();
	std::thread iothread = std::thread([&]
	{
		ASIO::io_service::work w(*gIOService);
		gIOService->run();
	});

	std::cout << "Type :h for help\n";
	bool quit = false;
	CommandLineReader cmdReader("COMMAND> ");
	while(!quit)
	{
		std::string cmd;
		while(!cmdReader.tryGet(cmd))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(5));

			// Delete any closed connections
			for(auto it = gCons.begin(); it!=gCons.end();)
			{
				if (it->second->closed)
				{
					std::cout << "Connection '" << it->second->name << "' closed.\n";
					it = gCons.erase(it);
				}
				else
				{
					it++;
				}
			}
		}

		if (!processCommand(cmd))
			quit = true;
	}

	gIOService->stop();
	iothread.join();

    return 0;
}

