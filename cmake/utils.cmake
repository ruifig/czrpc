
# Remember x86/x64
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    SET( EX_PLATFORM 64)
    SET( EX_PLATFORM_NAME "x64")
else (CMAKE_SIZEOF_VOID_P EQUAL 8)
    SET( EX_PLATFORM 32)
    SET( EX_PLATFORM_NAME "x86")
endif (CMAKE_SIZEOF_VOID_P EQUAL 8)

if (MSVC)
	SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D \"_CRT_SECURE_NO_WARNINGS\" /MP /bigobj")
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"))
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
endif()

if(MINGW OR CYGWIN)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mbig-obj")
endif()

function(cz_set_postfix target_name)
	set_target_properties( ${target_name}
		PROPERTIES

		# output directories
	    # CMAKE_BINARY_DIR will be the top level of wherever the projects are generated - https://cmake.org/cmake/help/v3.7/variable/CMAKE_BINARY_DIR.html
		# By using a configuration name that doesn't exist as a check in the generator expression, it means it will always expand to nothing
		ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib$<$<CONFIG:DummyConfigName>:>
		LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib$<$<CONFIG:DummyConfigName>:>
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin$<$<CONFIG:DummyConfigName>:>

		# Output file name
		OUTPUT_NAME ${target_name}_${EX_PLATFORM_NAME}_$<CONFIG>
	)
endfunction()

function(cz_add_commonlibs target_name)
	if (MSVC)
		target_link_libraries(${target_name} ws2_32)
	elseif (MINGW)
		target_link_libraries(${target_name} ws2_32 mswsock )
		#SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lws2_32")
	elseif(CMAKE_COMPILER_IS_GNUCXX OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"))
		target_link_libraries(${target_name} pthread )
	endif()
endfunction()

#
# Check if a file was added to a target
# found : output value (TRUE if found, FALSE otherwise)
# target_name : Target to check
# file: File to look for
function(cz_find_file_in_target found target_name file)
	get_target_property( _srcs ${target_name} SOURCES)
	#message("${_srcs}")
	foreach(_src IN LISTS _srcs)
		get_filename_component(_name ${_src} NAME)
		#message(STATUS "== ${_src} - ${_name}")
		if ( _name STREQUAL file OR _src STREQUAL file)
			set (found TRUE PARENT_SCOPE)
			return()
		endif()
	endforeach()
	set( found FALSE PARENT_SCOPE )
endfunction()

#
# http://stackoverflow.com/questions/148570/using-pre-compiled-headers-with-cmake
#	Shows a couple of ways to enabled precompiled headers for msvc
#
#
# target_name : Target to set the precompiled header for
# precompiled_header: Header file to use as precompiled header, without paths
# precompiled_source: Source file used to create the precompiled header, with the path as cmake sees it.
#
# Example, considering the following folder structure for project Core
# 
# MyProject
#     |- Private
#     |   ....
#     |   MyProjectPCH.h
#     |   MyProjectPCH.cpp
#     |- Public
#         ....
#     CMakeLists.txt
#
# cz_set_precompiled_header(MyProject MyProjectPCH.h Private/MyProjectPCH.cpp)
#
function(cz_set_precompiled_header target_name precompiled_header precompiled_source)
	if(MSVC)
		# Check if the header and source file exist in the target
		cz_find_file_in_target(found ${target_name} ${precompiled_header})
		if (NOT found)
			message(FATAL_ERROR "Precompiled header file ${precompiled_header} not found in project ${target_name}")
		endif()
		cz_find_file_in_target(found ${target_name} ${precompiled_source})
		if (NOT found)
			message(FATAL_ERROR "Precompiled source file ${precompiled_source} not found in project ${target_name}")
		endif()

		target_compile_options(${target_name} PRIVATE "/Yu${precompiled_header}")
		set_source_files_properties(${precompiled_source} PROPERTIES COMPILE_FLAGS "/Yc${precompiled_header}")
    endif()
endfunction()

