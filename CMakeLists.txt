cmake_minimum_required(VERSION 3.5)
project(czrpc)

# Will cause building to display what compiler parameters are being used, among other things
SET(CMAKE_VERBOSE_MAKEFILE_ON)

add_subdirectory(UnitTest++) # Foo is a static library
add_subdirectory(source)
add_subdirectory(tests)
