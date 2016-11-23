cmake_minimum_required(VERSION 3.5)
project(czrpc)

SET(SOURCE_FILES 
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
	"crazygaze/rpc/RPCUtils.h" )

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
set(${PROJECT_NAME}_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}
    CACHE INTERNAL "${PROJECT_NAME}: Include Directories" FORCE)

include_directories(
	${PROJECT_NAME}_INCLUDE_DIRS}
)
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)
