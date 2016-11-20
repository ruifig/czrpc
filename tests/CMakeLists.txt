cmake_minimum_required(VERSION 3.5)
project(tests)

include(../utils.cmake)

SET(SOURCE_FILES
		"Foo.h" "Foo.cpp"
		"Semaphore.h"
		"tests_any.cpp"
		#"tests_doc_a_small_taste.cpp"
		#"tests_doc_paramtraits.cpp"
		"tests_main.cpp"
		"tests_rpc.cpp"
		"tests_rpc_tcp.cpp"
		"tests_tcpsocket.cpp"
		"tests_traits.cpp"
		)

ADD_MSVC_PRECOMPILED_HEADER("testsPCH.h" "testsPCH.cpp" SOURCE_FILES)

message( "CMAKE_EXE_LINKER_FLAGS = " ${CMAKE_EXE_LINKER_FLAGS})
message( "COMPILER FLAGS USED")
message( "CMAKE_CXX_FLAGS                = " ${CMAKE_CXX_FLAGS})
message( "CMAKE_CXX_FLAGS_DEBUG          = " ${CMAKE_CXX_FLAGS_DEBUG})
message( "CMAKE_CXX_FLAGS_RELEASE        = " ${CMAKE_CXX_FLAGS_RELEASE})
message( "CMAKE_CXX_FLAGS_MINSIZEREL     = " ${CMAKE_CXX_FLAGS_MINSIZEREL})
message( "CMAKE_CXX_FLAGS_RELWITHDEBINFO = " ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})

include_directories(
		${UnitTest++_INCLUDE_DIRS}
		${czrpc_INCLUDE_DIRS}
		${CMAKE_SOURCE_DIR}/asio/asio/include
		)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
set_postfix()

target_link_libraries(${PROJECT_NAME} UnitTest++)
add_commonlibs()


