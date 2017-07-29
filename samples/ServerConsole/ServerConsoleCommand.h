#pragma once

using namespace cz::rpc;
using namespace cz;

struct ConInfo : public cz::rpc::Session, std::enable_shared_from_this<ConInfo>
{
	ConInfo(spas::Service& service)
		: trp(service)
	{
	}

	~ConInfo()
	{
		if (name != "")
			std::cout << "Connection '" << name << "' deleted\n";
	}

	std::string name;
	struct Addr
	{
		std::string ip;
		int port;
		std::string to_string()
		{
			return ip + ":" + std::to_string(port);
		}
		bool operator==(const Addr& other) const
		{
			return ip == other.ip && port == other.port;
		}
	} addr;

	Connection<void, GenericServer> con;
	SpasTransport trp;
};

struct GenericCommand
{
	std::string conName;
	std::string cmd;
	std::vector<cz::rpc::Any> params;
};

struct Data
{
	std::unordered_map<std::string, std::shared_ptr<ConInfo>> cons;
	spas::Service service;
	std::atomic<bool> finish = false;
};

extern Data* gData;

void processCommand(const std::string& str);
