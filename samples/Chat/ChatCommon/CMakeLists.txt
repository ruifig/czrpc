cmake_minimum_required(VERSION 3.5)
project(ChatCommon)

SET(SOURCE_FILES 
	"ChatInterface.h"
	)

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
set(${PROJECT_NAME}_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}
    CACHE INTERNAL "${PROJECT_NAME}: Include Directories" FORCE)

include_directories(
	${PROJECT_NAME}_INCLUDE_DIRS}
)
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)
set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "samples/chat")
