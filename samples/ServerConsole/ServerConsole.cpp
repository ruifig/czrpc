#include "ServerConsolePCH.h"


#pragma warning(disable: 4996)

#include "ServerConsoleCommand.h"
using namespace cz::rpc;

std::unordered_map<std::string, std::shared_ptr<ConInfo>> gCons;


int main()
{
	std::cout << "Type :h for help\n";
	bool quit = false;
	CommandLineReader cmdReader("COMMAND> ");

	if (!processCommand(":c \"127.0.0.1:9000\""))
		quit = true;

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

	getSharedData<TCPServiceThread>()->stop();
	gCons.clear(); // No connections can outlive the io service they use

    return 0;
}

