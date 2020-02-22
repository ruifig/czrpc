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

###############################################################################################
# Try and add czspas as a transport
###############################################################################################

# If there is a czspas folder present and the parent project didn't define a czspas target
# then we define it
# We check if the parent project added czspas and if not we look locally
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/czspas" AND NOT TARGET cz::spas)
	message("czspas folder found in czrpc. Adding czspas as a target")
	add_subdirectory(czspas)
endif()

# Finally, if czspas target exists, then we add it
if (TARGET cz::spas)
	target_link_libraries(czrpc INTERFACE cz::spas)
endif()

# Dummy target so that header only libraries show up in the IDE
add_custom_target(czrpc_ SOURCES ${SOURCE_FILES})

