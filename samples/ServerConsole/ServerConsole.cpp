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

	//auto clientCon = AsioTransport<void, GenericServer>::create(io, "127.0.0.1", 9000).get();

	std::cout << "Type :h for help\n";
	bool quit = false;
	while(!quit)
	{
		while(!kbhit())
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

		std::cout << "COMMAND- ";
		std::string cmdstr;
		std::getline(std::cin, cmdstr);
		if (!processCommand(cmdstr))
			quit = true;
	}

	gIOService->stop();
	iothread.join();

    return 0;
}

