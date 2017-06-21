cmake_minimum_required(VERSION 3.5)

include(../cmake/ucm.cmake)
include(../cmake/utils.cmake)

ucm_add_files(
		"Foo.h" "Foo.cpp"
		"tests_any.cpp"
		#"tests_doc_a_small_taste.cpp"
		#"tests_doc_paramtraits.cpp"
		"tests_main.cpp"
		"tests_rpc_tcp.cpp"
		"tests_tcpsocket.cpp"
		"tests_traits.cpp"
		"testsPCH.cpp" "testsPCH.h"
		TO SOURCE_FILES
		)

add_executable(tests ${SOURCE_FILES})
cz_set_precompiled_header(tests "testsPCH.h" "testsPCH.cpp")
target_link_libraries(tests czrpc UnitTest++)
cz_set_postfix(tests)

