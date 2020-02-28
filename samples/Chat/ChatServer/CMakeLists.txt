setup_czrpc_sample(ChatServer
		"ChatServer.cpp"
		"targetver.h"
		"ChatServerPCH.h"
		)

set_target_properties(czrpc_sample_ChatServer PROPERTIES FOLDER "${CZ_IDE_FOLDER}${PROJECT_NAME}/samples/chat")
target_link_libraries(czrpc_sample_ChatServer czrpc_sample_ChatCommon)

