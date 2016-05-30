// ChatServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

using namespace cz::rpc;

#define CZ_TEMPORARY_STRING_MAX_SIZE 512
#define CZ_TEMPORARY_STRING_MAX_NESTING 20

char* getTemporaryString()
{
	// Use several static strings, and keep picking the next one, so that callers can hold the string for a while without risk of it
	// being changed by another call.
	__declspec( thread ) static char bufs[CZ_TEMPORARY_STRING_MAX_NESTING][CZ_TEMPORARY_STRING_MAX_SIZE];
	__declspec( thread ) static int nBufIndex=0;

	char* buf = bufs[nBufIndex];
	nBufIndex++;
	if (nBufIndex==CZ_TEMPORARY_STRING_MAX_NESTING)
		nBufIndex = 0;

	return buf;
}

char* formatStringVA(const char* format, va_list argptr)
{
	char* buf = getTemporaryString();
	if (_vsnprintf(buf, CZ_TEMPORARY_STRING_MAX_SIZE, format, argptr) == CZ_TEMPORARY_STRING_MAX_SIZE)
		buf[CZ_TEMPORARY_STRING_MAX_SIZE-1] = 0;
	return buf;
}
const char* formatString(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	const char *str= formatStringVA(format, args);
	va_end(args);
	return str;
}

#define BROADCAST_RPC(excludedClient, func, ...)             \
	{                                                        \
		auto excluded = excludedClient;                      \
		for (auto&& c : m_clients)                           \
		{                                                    \
			if (c.second.get() == excluded)                  \
				continue;                                    \
			CZRPC_CALL(*(c.second->con), func, __VA_ARGS__); \
		}                                                    \
	}

struct ClientInfo
{
	std::shared_ptr<Connection<ChatServerInterface, ChatClientInterface>> con;
	bool isAdmin = false;
};

class ChatServer : public ChatServerInterface
{
public:
	using ConType = Connection<ChatServerInterface, ChatClientInterface>;

	ChatServer(int port)
	{
		m_th = std::thread([this]
		{
			ASIO::io_service::work w(m_io);
			m_io.run();
		});

		m_acceptor = std::make_shared<AsioTransportAcceptor<ChatServerInterface, ChatClientInterface>>(m_io, *this);
		m_acceptor->start(port, [&](std::shared_ptr<ConType> con)
		{
			auto info = std::make_shared<ClientInfo>();
			info->con = con;
			m_clients.insert(std::make_pair(con.get(), info));
		});
	}

	~ChatServer()
	{
		m_io.stop();
		m_th.join();
	}

private:
	//

	// ChatServerInterface
	//
	virtual std::string login(const std::string& user, const std::string& pass) override
	{
		if (pass != "pass")
			return "Wrong password";
		// #TODO

		BROADCAST_RPC(nullptr, onMsg, "", formatString("%s joined the chat", user.c_str()));
		return "OK";
	}

	virtual void sendMsg(const std::string& msg) override
	{
		// #TODO
		BROADCAST_RPC(nullptr, onMsg, "rui", "msg");
	}

	virtual void kick(const std::string& user) override
	{
		// #TODO
	}

	ClientInfo* getCurrentUser()
	{
		if (ConType::getCurrent())
			return m_clients[ConType::getCurrent()].get();
		else
			return nullptr;
	}

	ASIO::io_service m_io;
	std::thread m_th;
	std::shared_ptr<AsioTransportAcceptor<ChatServerInterface, ChatClientInterface>> m_acceptor;
	std::unordered_map<ConType*, std::shared_ptr<ClientInfo>> m_clients;
};



int main()
{
    return 0;
}

