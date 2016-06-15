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
#include <assert.h>
#include "crazygaze/rpc/RPCCallstack.h"
#include "crazygaze/rpc/RPCParamTraits.h"
#include "crazygaze/rpc/RPCAny.h"
#include "crazygaze/rpc/RPCResult.h"
#include "crazygaze/rpc/RPCStream.h"
#include "crazygaze/rpc/RPCUtils.h"
#include "crazygaze/rpc/RPCTransport.h"
#include "crazygaze/rpc/RPCTable.h"
#include "crazygaze/rpc/RPCProcessor.h"
#include "crazygaze/rpc/RPCConnection.h"
#include "crazygaze/rpc/RPCGenericServer.h"
