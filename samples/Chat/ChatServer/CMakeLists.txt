cz_setup_sample(ChatServer
		"ChatServer.cpp"
		"targetver.h"
		"ChatServerPCH.cpp" "ChatServerPCH.h"
		)
set_target_properties(ChatServer PROPERTIES FOLDER "samples/chat")
target_link_libraries(ChatServer ChatCommon)

