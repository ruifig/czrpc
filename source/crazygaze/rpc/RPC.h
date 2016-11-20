/************************************************************************
Main czrpc file.
You should only need to include this for the czrpc core, plus any
transports you want to use
************************************************************************/
#pragma once

// If set to 1, exceptions thrown by RPCs will be caught, and passed to the
// caller as a string.
#if !defined(CZRPC_CATCH_EXCEPTIONS)
	#define CZRPC_CATCH_EXCEPTIONS 1
#endif

// If defined AND set to 1, it will use Boost Asio, instead of standalone Asio
#if !defined(CZRPC_HAS_BOOST)
	#define CZRPC_HAS_BOOST 0
#endif

#ifdef _WIN32
    #define CZRPC_DEBUG_BREAK __debugbreak
#else
    #define CZRPC_DEBUG_BREAK __builtin_trap
#endif

#ifdef NDEBUG
	#define CZRPC_ASSERT(expr) ((void)0)
#else
	#define CZRPC_ASSERT(expr) \
		if (!(expr)) CZRPC_DEBUG_BREAK();
#endif

//
// CZRPC_LOGGING
// If set to 1, it will log detailed RPC information
#ifdef NDEBUG
	#define CZRPC_LOGGING 0
#else
	#define CZRPC_LOGGING 0
#endif

//
// CZRPC_FORCE_CALL_DBG
// If set to 1 AND CZRPC_LOGGING is also 1, it will implicitly add debug information to all
// calls
#ifdef NDEBUG
	#define CZRPC_FORCE_CALL_DBG 0
#else
	#define CZRPC_FORCE_CALL_DBG 0
#endif
 

#if CZRPC_LOGGING
	#include "crazygaze/rpc/RPCLogger.h"
	extern cz::Logger g_rpcLogger;
	#define CZRPC_LOG(verbosity, fmt, ...) \
		g_rpcLogger.log(__FILE__, __LINE__, cz::Logger::Verbosity::verbosity, fmt, ##__VA_ARGS__)
#else
	#define CZRPC_LOG(verbosity, fmt, ...) ((void)0)
#endif

#define CZRPC_LOGSTR_CREATE  "N%010u:Create  : "
#define CZRPC_LOGSTR_COMMIT  "N%010u:Commit  : "
#define CZRPC_LOGSTR_SEND    "N%010u:Send    : "
#define CZRPC_LOGSTR_ABORT   "N%010u:Abort   : "
#define CZRPC_LOGSTR_RESULT  "N%010u:Result  : "
#define CZRPC_LOGSTR_RECEIVE "N%010u:Receive : "
#define CZRPC_LOGSTR_REPLY   "N%010u:Reply   : "

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <stdint.h>
#include <tuple>
#include <utility>
#include <mutex>
#include <type_traits>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <future>
#include <queue>
#include <functional>
#include <assert.h>
#include <cstring>
#include "crazygaze/rpc/RPCCallstack.h"
#include "crazygaze/rpc/RPCParamTraits.h"
#include "crazygaze/rpc/RPCAny.h"
#include "crazygaze/rpc/RPCObjectData.h"
#include "crazygaze/rpc/RPCResult.h"
#include "crazygaze/rpc/RPCStream.h"
#include "crazygaze/rpc/RPCUtils.h"
#include "crazygaze/rpc/RPCTransport.h"
#include "crazygaze/rpc/RPCTable.h"
#include "crazygaze/rpc/RPCProcessor.h"
#include "crazygaze/rpc/RPCConnection.h"
#include "crazygaze/rpc/RPCGenericServer.h"

