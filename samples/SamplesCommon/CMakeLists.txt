cmake_minimum_required(VERSION 3.5)
project(SamplesCommon)

include(../../utils.cmake)

SET(SOURCE_FILES
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

ADD_MSVC_PRECOMPILED_HEADER("SamplesCommonPCH.h" "SamplesCommonPCH.cpp" SOURCE_FILES)

message( "CMAKE_EXE_LINKER_FLAGS = " ${CMAKE_EXE_LINKER_FLAGS})
message( "COMPILER FLAGS USED")
message( "CMAKE_CXX_FLAGS                = " ${CMAKE_CXX_FLAGS})
message( "CMAKE_CXX_FLAGS_DEBUG          = " ${CMAKE_CXX_FLAGS_DEBUG})
message( "CMAKE_CXX_FLAGS_RELEASE        = " ${CMAKE_CXX_FLAGS_RELEASE})
message( "CMAKE_CXX_FLAGS_MINSIZEREL     = " ${CMAKE_CXX_FLAGS_MINSIZEREL})
message( "CMAKE_CXX_FLAGS_RELWITHDEBINFO = " ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
set_postfix()
set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "samples")

set(${PROJECT_NAME}_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}
    CACHE INTERNAL "${PROJECT_NAME}: Include Directories" FORCE)



