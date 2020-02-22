setup_czrpc_sample (ChatClient
		"ChatClient.cpp"
		"targetver.h"
		"ChatClientPCH.h"
		)

set_target_properties(czrpc_sample_ChatClient PROPERTIES FOLDER "${PROJECT_NAME}/samples/chat")
target_link_libraries(czrpc_sample_ChatClient czrpc_sample_ChatCommon)

