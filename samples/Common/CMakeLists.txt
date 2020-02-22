cmake_minimum_required(VERSION 3.16)

add_library(Common STATIC
		"Misc.cpp"
		"Misc.h"
		"Parameters.cpp"
		"Parameters.h"
		"Semaphore.h"
		"SimpleServer.h"
		"StringUtil.cpp"
		"StringUtil.h"
		"targetver.h"
		)

target_precompile_headers(Common PUBLIC "CommonPCH.h")
set_property(TARGET Common PROPERTY FOLDER "samples/czrpc")
target_link_libraries(Common PUBLIC cz::rpc)

