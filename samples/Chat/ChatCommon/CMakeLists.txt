set(SOURCE_FILES
	"ChatInterface.h"
	)

add_library(ChatCommon INTERFACE)
target_include_directories(ChatCommon INTERFACE "")
add_custom_target(ChatCommon_ SOURCES ${SOURCE_FILES})
set_target_properties(ChatCommon_ PROPERTIES FOLDER "samples/czrpc/chat")

