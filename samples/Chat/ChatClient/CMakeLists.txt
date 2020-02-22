setup_czrpc_sample (ChatClient
		"ChatClient.cpp"
		"targetver.h"
		)

set_target_properties(ChatClient PROPERTIES FOLDER "samples/czrpc/chat")
target_link_libraries(ChatClient ChatCommon)

