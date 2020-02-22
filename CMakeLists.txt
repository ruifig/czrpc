cmake_minimum_required(VERSION 3.16)
project(czrpc
		DESCRIPTION "Header only rpc library, without code generation"
		LANGUAGES CXX)

add_subdirectory(source)
add_subdirectory(samples)

# If we have a unittest-cpp folder but the parent project didn't add UnitTest++, then automatically
# define the UnitTest++ target
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/unittest-cpp" AND NOT TARGET UnitTest++)
	message("unittest-cpp folder found in czrpc. Adding UnitTest++ target")
	add_subdirectory(unittest-cpp)
endif()

if (TARGET UnitTest++)
	message("Enabling czrpc tests")
	add_subdirectory(tests)
endif()

