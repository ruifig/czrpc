cmake_minimum_required(VERSION 3.16)

project(czrpc
		DESCRIPTION "Header only rpc library, without code generation"
		LANGUAGES CXX)

option(CZRPC_Tests "Create projects for czrpc tests" OFF)
option(CZRPC_Samples "Create projects for czrpc samples" OFF)

add_subdirectory(source)

# If there is a czspas folder present and the parent project didn't define a czspas target
# then we define it
# We check if the parent project added czspas and if not we look locally
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/czspas" AND NOT TARGET cz::spas)
	message("czspas folder found in ${PROJECT_NAME}'s root. Adding czspas as a target")
	add_subdirectory(czspas)
endif()

if (CZRPC_Tests)
	# If we have a unittest-cpp folder but the parent project didn't add UnitTest++, then automatically
	# define the UnitTest++ target
	if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/unittest-cpp" AND NOT TARGET UnitTest++)
		message("unittest-cpp folder found in ${PROJECT_NAME}'s root. Adding UnitTest++ target")
		add_subdirectory(unittest-cpp)
	endif()

	if (TARGET UnitTest++)
		message("Enabling ${PROJECT_NAME} tests")
		add_subdirectory(tests)
	endif()
endif()

if (CZRPC_Samples)
	add_subdirectory(samples)
endif()




