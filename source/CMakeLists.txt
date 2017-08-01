cmake_minimum_required(VERSION 3.5)

ucm_add_files(
	"crazygaze/rpc/RPC.h"
	"crazygaze/rpc/RPCAny.h"
	"crazygaze/rpc/RPCCallstack.h"
	"crazygaze/rpc/RPCConnection.h"
	"crazygaze/rpc/RPCGenerate.h"
	"crazygaze/rpc/RPCGenericServer.h"
	"crazygaze/rpc/RPCLogger.h"
	"crazygaze/rpc/RPCObjectData.h"
	"crazygaze/rpc/RPCParamTraits.h"
	"crazygaze/rpc/RPCProcessor.h"
	"crazygaze/rpc/RPCResult.h"
	"crazygaze/rpc/RPCStream.h"
	"crazygaze/rpc/RPCTable.h"
	"crazygaze/rpc/RPCTCPSocket.h"
	"crazygaze/rpc/RPCTCPSocketTransport.h"
	"crazygaze/rpc/RPCTransport.h"
	"crazygaze/rpc/RPCUtils.h"
	"crazygaze/rpc/RPCSpasTransport.h"
	"crazygaze/rpc/RPCSpasTransportUtils.h"
	TO SOURCE_FILES
	)

# Adding as INTERFACE, since it's an header only library
add_library(czrpc INTERFACE)
target_include_directories(czrpc INTERFACE "")

# czrpc uses czspas in the default transport
target_link_libraries(czrpc INTERFACE czspas)

# Dummy target so that header only libraries show up in the IDE
add_custom_target(czrpc_ SOURCES ${SOURCE_FILES})

