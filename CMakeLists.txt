cmake_minimum_required(VERSION 3.5)
project(czrpc)


set_property( GLOBAL PROPERTY USE_FOLDERS ON )

# Will cause building to display what compiler parameters are being used, among other things
SET(CMAKE_VERBOSE_MAKEFILE_ON)

add_subdirectory(UnitTest++) # Foo is a static library
add_subdirectory(source)
add_subdirectory(tests)

add_subdirectory(samples/SamplesCommon)
add_subdirectory(samples/Minimal)
add_subdirectory(samples/CalculatorServer)
add_subdirectory(samples/ServerConsole)
add_subdirectory(samples/Chat/ChatCommon)
add_subdirectory(samples/Chat/ChatClient)
add_subdirectory(samples/Chat/ChatServer)

