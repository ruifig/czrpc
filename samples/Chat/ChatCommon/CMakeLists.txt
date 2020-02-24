set(SOURCE_FILES
	"ChatInterface.h"
	)

add_library(czrpc_sample_ChatCommon INTERFACE)
target_include_directories(czrpc_sample_ChatCommon INTERFACE "")
add_custom_target(czrpc_sample_ChatCommon_ SOURCES ${SOURCE_FILES})
set_target_properties(czrpc_sample_ChatCommon_ PROPERTIES FOLDER "${PROJECT_NAME}/samples/chat")

