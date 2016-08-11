#pragma once

#define CHATSERVER_DEFAULT_PORT 9000

// To allow RPC parameters in the form of "const T&", for any valid T
CZRPC_ALLOW_CONST_LVALUE_REFS;

class ChatServerInterface
{
public:

	//
	// \return
	// "OK" if login accepted, or failure reason
	virtual std::string login(const std::string& name, const std::string& pass) = 0;

	virtual void sendMsg(const std::string& msg) = 0;
	virtual void kick(const std::string& name) = 0;
	virtual std::vector<std::string> getUserList() = 0;
};

#define RPCTABLE_CLASS ChatServerInterface
#define RPCTABLE_CONTENTS \
		REGISTERRPC(login) \
		REGISTERRPC(sendMsg) \
		REGISTERRPC(kick) \
		REGISTERRPC(getUserList)
#include "crazygaze/rpc/RPCGenerate.h"

class ChatClientInterface
{
public:
	
	// The server calls this when another user types something.
	// If "name" is empty, it's a system message
	virtual void onMsg(const std::string& name, const std::string& msg) = 0;
};

#define RPCTABLE_CLASS ChatClientInterface
#define RPCTABLE_CONTENTS \
	REGISTERRPC(onMsg)
#include "crazygaze/rpc/RPCGenerate.h"
