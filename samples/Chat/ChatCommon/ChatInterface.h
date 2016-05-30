#pragma once

// To allow RPC parameters in the form of "const T&", for any valid T
CZRPC_ALLOW_CONST_LVALUE_REFS;

class ChatServerInterface
{
public:

	//
	// \return
	// "OK" if login accepted, or failure reason
	virtual std::string login(const std::string& user, const std::string& pass) = 0;

	virtual void sendMsg(const std::string& msg) = 0;
	virtual void kick(const std::string& user) = 0;
};

#define RPCTABLE_CLASS ChatServerInterface
#define RPCTABLE_CONTENTS \
		REGISTERRPC(login) \
		REGISTERRPC(sendMsg) \
		REGISTERRPC(kick)
#include "crazygaze/rpc/RPCGenerate.h"

class ChatClientInterface
{
public:
	// The server calls this if the client was kicked
	virtual void onMsg(const std::string& user, const std::string& msg) = 0;
	virtual void onSystemMsg(const std::string& msg) = 0;
};

#define RPCTABLE_CLASS ChatClientInterface
#define RPCTABLE_CONTENTS \
	REGISTERRPC(onMsg) \
	REGISTERRPC(onSystemMsg)
#include "crazygaze/rpc/RPCGenerate.h"
