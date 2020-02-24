cmake_minimum_required(VERSION 3.16)

add_executable(czrpc_tests
		"Foo.h" "Foo.cpp"
		"tests_any.cpp"
		"tests_main.cpp"
		"tests_rpc_spas.cpp"
		"tests_rpc_spas_helper.cpp"
		"tests_rpc_spas_helper.h"
		"tests_traits.cpp"
		"testsPCH.h"
		)

target_precompile_headers(czrpc_tests PUBLIC "testsPCH.h")
target_link_libraries(czrpc_tests cz::rpc UnitTest++)
set_property(TARGET czrpc_tests PROPERTY FOLDER "${PROJECT_NAME}")

