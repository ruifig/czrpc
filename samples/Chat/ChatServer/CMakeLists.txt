setup_czrpc_sample(ChatServer
		"ChatServer.cpp"
		"targetver.h"
		)

set_target_properties(ChatServer PROPERTIES FOLDER "samples/czrpc/chat")
target_link_libraries(ChatServer ChatCommon)

