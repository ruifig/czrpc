cz_setup_sample (ChatClient
		"ChatClient.cpp"
		"targetver.h"
		"ChatClientPCH.cpp" "ChatClientPCH.h"
		)
set_target_properties(ChatClient PROPERTIES FOLDER "samples/chat")
target_link_libraries(ChatClient ChatCommon)

