cmake_minimum_required(VERSION 3.5)

ucm_add_dirs("UnitTest++/" TO UNITTESTPP_SRC)

if (WIN32)
	ucm_add_dirs("UnitTest++/Win32/" TO UNITTESTPP_SRC)
else()
	ucm_add_dirs("UnitTest++/Posix/" TO UNITTESTPP_SRC)
endif()

add_library(UnitTest++ STATIC ${UNITTESTPP_SRC})
target_include_directories(UnitTest++ PUBLIC "")
cz_set_postfix(UnitTest++)

