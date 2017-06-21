cmake_minimum_required(VERSION 3.5)

ucm_add_files(
		"Misc.cpp"
		"Misc.h"
		"Parameters.cpp"
		"Parameters.h"
		"Semaphore.h"
		"SimpleServer.h"
		"StringUtil.cpp"
		"StringUtil.h"
		"targetver.h"
		"SamplesCommonPCH.cpp" "SamplesCommonPCH.h"
		TO SOURCE_FILES
		)

add_library(SamplesCommon STATIC ${SOURCE_FILES})
cz_set_precompiled_header(SamplesCommon "SamplesCommonPCH.h" "SamplesCommonPCH.cpp")
set_target_properties(SamplesCommon PROPERTIES FOLDER "samples")
target_link_libraries(SamplesCommon PUBLIC czrpc)
cz_set_postfix(SamplesCommon)

