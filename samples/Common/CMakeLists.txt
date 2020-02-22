cmake_minimum_required(VERSION 3.16)

add_library(czrpc_sample_Common STATIC
		"Misc.cpp"
		"Misc.h"
		"Parameters.cpp"
		"Parameters.h"
		"Semaphore.h"
		"SimpleServer.h"
		"StringUtil.cpp"
		"StringUtil.h"
		"targetver.h"
		"CommonPCH.h"
		)

target_precompile_headers(czrpc_sample_Common PUBLIC "CommonPCH.h")
set_property(TARGET czrpc_sample_Common PROPERTY FOLDER "${PROJECT_NAME}/samples")
target_link_libraries(czrpc_sample_Common PUBLIC cz::rpc)

