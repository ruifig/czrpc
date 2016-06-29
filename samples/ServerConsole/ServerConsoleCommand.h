#pragma once

using namespace cz::rpc;

struct ConInfo
{
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
	std::shared_ptr<Connection<void, GenericServer>> con;
	bool closed = false;
};

struct GenericCommand
{
	std::string conName;
	std::string cmd;
	std::vector<cz::rpc::Any> params;
};

extern std::unordered_map<std::string, std::shared_ptr<ConInfo>> gCons;
extern std::shared_ptr<ASIO::io_service> gIOService;

bool processCommand(const std::string& str);
