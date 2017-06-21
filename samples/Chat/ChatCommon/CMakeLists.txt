ucm_add_files(
	"ChatInterface.h"
	TO SOURCE_FILES
	)

add_library(ChatCommon INTERFACE)
target_include_directories(ChatCommon INTERFACE "")
add_custom_target(ChatCommon_ SOURCES ${SOURCE_FILES})
set_target_properties(ChatCommon_ PROPERTIES FOLDER "samples/chat")

