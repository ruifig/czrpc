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
			struct DisableFatalAssert
			{
				DisableFatalAssert(const DisableFatalAssert&) = delete;
				DisableFatalAssert& operator =(const DisableFatalAssert&) = delete;
				DisableFatalAssert() : previous(ms_assertOnFatal) {
					ms_assertOnFatal = false;
				}
				~DisableFatalAssert() { ms_assertOnFatal = previous; }
				bool previous;
			};
			struct DisableLogging
			{
				DisableLogging(const DisableFatalAssert&) = delete;
				DisableLogging& operator =(const DisableFatalAssert&) = delete;
				DisableLogging() : previous(ms_logEnabled) {
					ms_logEnabled = false;
				}
				~DisableLogging() { ms_logEnabled = previous; }
				bool previous;
			};
			static bool ms_assertOnFatal;
			static bool ms_logEnabled;
			static void out(bool fatal, const char* type, const char* fmt, ...)
			{
				if (!ms_logEnabled)
					return;
				char buf[256];
				copyStrToFixedBuffer(buf, type);
				va_list args;
				va_start(args, fmt);
				vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 1, fmt, args);
				va_end(args);
				printf("%s\n",buf);
				if (fatal && ms_assertOnFatal)
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

