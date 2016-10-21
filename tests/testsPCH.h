#pragma once

#ifdef _WIN32
#include "targetver.h"
#endif

//
// czrpc
//
#include "crazygaze/rpc/RPC.h"
#include "crazygaze/rpc/RPCAsioTransport.h"

namespace cz {
	namespace rpc {

		struct MyTCPLog
		{
			static void out(bool fatal, const char* type, const char* fmt, ...)
			{
				char buf[256];
				strcpy(buf, type);
				va_list args;
				va_start(args, fmt);
				vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 1, fmt, args);
				va_end(args);
				printf("%s\n",buf);
				if (fatal)
				{
					CZRPC_DEBUG_BREAK();
					exit(1);
				}
			}
		};
//#define TCPINFO(fmt, ...) MyTCPLog::out(false, "Info: ", fmt, ##__VA_ARGS__)
#define TCPINFO(...) ((void)0)
#define TCPWARNING(fmt, ...) MyTCPLog::out(false, "Warning: ", fmt, ##__VA_ARGS__)
#define TCPERROR(fmt, ...) MyTCPLog::out(true, "Error: ", fmt, ##__VA_ARGS__)
}
}

#include "crazygaze/rpc/RPCTCPSocketTransport.h"

#include <stdio.h>
#include <vector>
#include <string>
#include <queue>
#include <cstdarg>

//
// UnitTest++
//
#include "UnitTest++/UnitTest++.h"
#include "UnitTest++/CurrentTest.h"

#include "Semaphore.h"

