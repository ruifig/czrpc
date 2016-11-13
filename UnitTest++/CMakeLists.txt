cmake_minimum_required(VERSION 3.5)
project(UnitTest++)

include(../utils.cmake)

file(GLOB SOURCE_FILES_1
		"UnitTest++/*.cpp"
		"UnitTest++/*.h" )
source_group(src FILES ${SOURCE_FILES_1})
if (WIN32)
	file(GLOB SOURCE_FILES_2 "UnitTest++/Win32/*.*")
	source_group(src\\Win32 FILES ${SOURCE_FILES_2})
else()
	file(GLOB SOURCE_FILES_2 "UnitTest++/Posix/*.*")
	source_group(src\\Posix FILES ${SOURCE_FILES_2})
endif()

SET(SOURCE_FILES ${SOURCE_FILES_1} ${SOURCE_FILES_2})

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
set_postfix()
set(${PROJECT_NAME}_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}
    CACHE INTERNAL "${PROJECT_NAME}: Include Directories" FORCE)
