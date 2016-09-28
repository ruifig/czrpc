cmake_minimum_required(VERSION 3.5)
project(tests)

# From http://stackoverflow.com/questions/148570/using-pre-compiled-headers-with-cmake
# Given a variable MySources with all the source files, use as follow:
# ADD_MSVC_PRECOMPILED_HEADER("precompiled.h" "precompiled.cpp" MySources)
MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(PrecompiledBinary "${CMAKE_CURRENT_BINARY_DIR}/${PrecompiledBasename}.pch")
    SET(Sources ${${SourcesVar}})

    SET_SOURCE_FILES_PROPERTIES(${PrecompiledSource}
                                PROPERTIES COMPILE_FLAGS "/Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\""
                                           OBJECT_OUTPUTS "${PrecompiledBinary}")
    SET_SOURCE_FILES_PROPERTIES(${Sources}
                                PROPERTIES COMPILE_FLAGS "/Yu\"${PrecompiledHeader}\" /FI\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\""
                                           OBJECT_DEPENDS "${PrecompiledBinary}")  
    # Add precompiled header to SourcesVar
    LIST(APPEND ${SourcesVar} ${PrecompiledHeader} ${PrecompiledSource})
  ENDIF(MSVC)
ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)


# Remember x86/x64
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    SET( EX_PLATFORM 64)
    SET( EX_PLATFORM_NAME "x64")
else (CMAKE_SIZEOF_VOID_P EQUAL 8)
    SET( EX_PLATFORM 32)
    SET( EX_PLATFORM_NAME "x86")
endif (CMAKE_SIZEOF_VOID_P EQUAL 8)

set (CMAKE_DEBUG_POSTFIX "_Debug")
set (CMAKE_RELEASE_POSTFIX "_Release")
set (CMAKE_MINSIZEREL_POSTFIX "_MinSizeRel")
set (CMAKE_RELWITHDEBINFO_POSTFIX "_RelWithDebInfo")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin)

SET(SOURCE_FILES
		"Foo.h" "Foo.cpp"
		"Semaphore.h"
		"tests_any.cpp"
		"tests_doc_a_small_taste.cpp"
		"tests_doc_paramtraits.cpp"
		"tests_main.cpp"
		"tests_rpc.cpp"
		"tests_tcpsocket.cpp"
		"tests_traits.cpp"
		)

ADD_MSVC_PRECOMPILED_HEADER("testsPCH.h" "testsPCH.cpp" SOURCE_FILES)

if (MSVC)
	SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"))
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
endif()

if(MINGW OR CYGWIN)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mbig-obj")
endif()
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
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX _${EX_PLATFORM_NAME}${CMAKE_DEBUG_POSTFIX})
set_target_properties(${PROJECT_NAME} PROPERTIES RELEASE_POSTFIX _${EX_PLATFORM_NAME}${CMAKE_RELEASE_POSTFIX})
set_target_properties(${PROJECT_NAME} PROPERTIES MINSIZEREL_POSTFIX _${EX_PLATFORM_NAME}${CMAKE_MINSIZEREL_POSTFIX})
set_target_properties(${PROJECT_NAME} PROPERTIES RELWITHDEBINFO_POSTFIX _${EX_PLATFORM_NAME}${CMAKE_RELWITHDEBINFO_POSTFIX})

target_link_libraries(${PROJECT_NAME} UnitTest++)
if (MINGW)
	target_link_libraries(${PROJECT_NAME} ws2_32 mswsock )
	#SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lws2_32")
elseif(CMAKE_COMPILER_IS_GNUCXX OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"))
	target_link_libraries(${PROJECT_NAME} pthread )
endif()


