cmake_minimum_required(VERSION 3.16)

set(SOURCE_FILES
	"crazygaze/rpc/RPC.h"
	"crazygaze/rpc/RPCAny.h"
	"crazygaze/rpc/RPCCallstack.h"
	"crazygaze/rpc/RPCBaseConnection.h"
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
	"crazygaze/rpc/RPCTransport.h"
	"crazygaze/rpc/RPCUtils.h"
	"crazygaze/rpc/RPCSpasTransport.h"
	)

source_group("crazygaze\\rpc" FILES ${SOURCE_FILES})

# Adding as INTERFACE, since it's an header only library
add_library(czrpc INTERFACE)
add_library(cz::rpc ALIAS czrpc)

target_include_directories(czrpc INTERFACE "")

# We need the czspas target
if (TARGET cz::spas)
	target_link_libraries(czrpc INTERFACE cz::spas)
else()
	message(FATAL_ERROR "czspas target not found. Either have the root project add czspas as a target, or clone czspas repository into czrpc's own root folder ( ${PROJECT_SOURCE_DIR} )")
endif()

# Dummy target so that header only libraries show up in the IDE
add_custom_target(czrpc_ SOURCES ${SOURCE_FILES})
set_target_properties(czrpc_ PROPERTIES FOLDER "${CZ_IDE_FOLDER}${PROJECT_NAME}")


