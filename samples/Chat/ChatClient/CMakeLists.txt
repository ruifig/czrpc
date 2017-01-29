cmake_minimum_required(VERSION 3.5)
project(ChatClient)

include(../../../utils.cmake)

SET(SOURCE_FILES
		"ChatClient.cpp"
		"targetver.h"
		)

ADD_MSVC_PRECOMPILED_HEADER("${PROJECT_NAME}PCH.h" "${PROJECT_NAME}PCH.cpp" SOURCE_FILES)

message( "CMAKE_EXE_LINKER_FLAGS = " ${CMAKE_EXE_LINKER_FLAGS})
message( "COMPILER FLAGS USED")
message( "CMAKE_CXX_FLAGS                = " ${CMAKE_CXX_FLAGS})
message( "CMAKE_CXX_FLAGS_DEBUG          = " ${CMAKE_CXX_FLAGS_DEBUG})
message( "CMAKE_CXX_FLAGS_RELEASE        = " ${CMAKE_CXX_FLAGS_RELEASE})
message( "CMAKE_CXX_FLAGS_MINSIZEREL     = " ${CMAKE_CXX_FLAGS_MINSIZEREL})
message( "CMAKE_CXX_FLAGS_RELWITHDEBINFO = " ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})

include_directories(
		${czrpc_INCLUDE_DIRS}
		${ChatCommon_INCLUDE_DIRS}
		)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
set_postfix()
set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "samples/chat")
target_link_libraries(${PROJECT_NAME} SamplesCommon)
add_commonlibs()



