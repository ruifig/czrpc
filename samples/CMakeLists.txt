cmake_minimum_required(VERSION 3.16)

function(setup_czrpc_sample target_name)
	add_executable(${target_name} ${ARGN})
	target_precompile_headers(${target_name} PUBLIC "${target_name}PCH.h")
	set_target_properties(${target_name} PROPERTIES FOLDER "samples/czrpc")
	target_link_libraries(${target_name} Common)
endfunction()

add_subdirectory(Common)
add_subdirectory(Minimal)
add_subdirectory(CalculatorServer)
add_subdirectory(ServerConsole)
add_subdirectory(Chat/ChatCommon)
add_subdirectory(Chat/ChatClient)
add_subdirectory(Chat/ChatServer)

