include(../cmake/ucm.cmake)
include(../cmake/utils.cmake)

function(cz_setup_sample target_name)
	add_executable(${target_name} ${ARGN})
	cz_set_precompiled_header(${target_name} "${target_name}PCH.h" "${target_name}PCH.cpp")
	set_target_properties(${target_name} PROPERTIES FOLDER "samples")
	target_link_libraries(${target_name} SamplesCommon)
	cz_set_postfix(${target_name})
endfunction()

add_subdirectory(SamplesCommon)
add_subdirectory(Minimal)
add_subdirectory(CalculatorServer)
add_subdirectory(ServerConsole)
add_subdirectory(Chat/ChatCommon)
add_subdirectory(Chat/ChatClient)
add_subdirectory(Chat/ChatServer)

