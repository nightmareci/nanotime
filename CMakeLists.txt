# You can choose this license, if possible in your jurisdiction:
#
# Unlicense
#
# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or distribute
# this software, either in source code form or as a compiled binary, for any
# purpose, commercial or non-commercial, and by any means.
#
# In jurisdictions that recognize copyright laws, the author or authors of this
# software dedicate any and all copyright interest in the software to the
# public domain. We make this dedication for the benefit of the public at large
# and to the detriment of our heirs and successors. We intend this dedication
# to be an overt act of relinquishment in perpetuity of all present and future
# rights to this software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# For more information, please refer to <http://unlicense.org/>
#
#
# Alternative license choice, if works can't be directly submitted to the
# public domain in your jurisdiction:
#
# The MIT License (MIT)
#
# Copyright © 2022 Brandon McGriff <nightmareci@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the “Software”), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.6)

project(test_nanotime LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99 CACHE STRING "The C language standard to use. C99 (\"99\") is the default.")
set(CMAKE_C_STANDARD_REQUIRED TRUE)

if(CMAKE_C_STANDARD STREQUAL 90)
	message(FATAL_ERROR "The C language standard used must be C99 (\"99\") or higher; C89/C90 (\"90\") is not supported.")
endif()

set(CMAKE_CXX_STANDARD 11 CACHE STRING "The C++ language standard to use. C++11 (\"11\") is the default.")
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if(UNIX OR APPLE OR MINGW)
	add_compile_definitions(_POSIX_VERSION=200112L)
endif()

set(C_EXECUTABLES
	test_nanotime_sleep_c
	test_nanotime_step
	render_thread_test_nanotime_step
)

set(CPP_EXECUTABLES
	test_nanotime_sleep_cpp
)

function(add_executables LIST_NAME SRC_EXT)
	foreach(NAME ${${LIST_NAME}})
		add_executable("${NAME}" "${NAME}.${SRC_EXT}" "nanotime.h")
	endforeach()
endfunction()

add_executables(C_EXECUTABLES "c")
add_executables(CPP_EXECUTABLES "cpp")

if(UNIX OR APPLE OR MINGW)
	include(GNUInstallDirs)
	install(TARGETS ${C_EXECUTABLES} ${CPP_EXECUTABLES} DESTINATION "${CMAKE_INSTALL_BINDIR}")
else()
	install(TARGETS ${C_EXECUTABLES} ${CPP_EXECUTABLES} DESTINATION ".")
endif()

option(SHOW_LOG "Show a log of live timing information. On by default." ON)
if(SHOW_LOG)
	target_compile_definitions(test_nanotime_step PRIVATE SHOW_LOG TRUE)
	target_compile_definitions(render_thread_test_nanotime_step PRIVATE SHOW_LOG TRUE)
endif()

option(MULTITHREADED "Make the test_nanotime_step program use multithreading.")
if(MULTITHREADED)
	target_compile_definitions(test_nanotime_step PRIVATE MULTITHREADED TRUE)
endif()

option(REALTIME "Make the thread(s) of the test_nanotime_step and render_thread_test_nanotime_step programs use realtime thread priority, where supported. Currently, only Linux is supported.")
if(REALTIME)
	target_compile_definitions(test_nanotime_step PRIVATE REALTIME TRUE)
	target_compile_definitions(render_thread_test_nanotime_step PRIVATE REALTIME TRUE)
endif()

if(MINGW)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET SDL2)
	target_link_libraries(test_nanotime_step
		PRIVATE PkgConfig::SDL2
	)
	target_link_libraries(render_thread_test_nanotime_step
		PRIVATE PkgConfig::SDL2
	)
else()
	find_package(SDL2 REQUIRED)
	target_link_libraries(test_nanotime_step
		PRIVATE SDL2::SDL2
	)
	target_link_libraries(render_thread_test_nanotime_step
		PRIVATE SDL2::SDL2
	)
	if(TARGET SDL2::SDL2main)
		target_link_libraries(test_nanotime_step
			PRIVATE SDL2::SDL2main
		)
		target_link_libraries(render_thread_test_nanotime_step
			PRIVATE SDL2::SDL2main
		)
	endif()
endif()

find_library(MATH_LIBRARY m)
if(NOT "${MATH_LIBRARY}" STREQUAL MATH_LIBRARY-NOTFOUND)
	target_link_libraries(test_nanotime_step PRIVATE "${MATH_LIBRARY}")
	target_link_libraries(render_thread_test_nanotime_step PRIVATE "${MATH_LIBRARY}")
endif()
