cmake_minimum_required(VERSION 3.0)
project(ConsoleTetris)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_VERBOSE_MAKEFILE ON)

if (MSVC)
    add_compile_options(/source-charset:utf-8 /W4)
    add_compile_options($<$<CONFIG:RELEASE>:/MT>)
    add_compile_options($<$<CONFIG:DEBUG>:/MTd>)
else ()
    add_compile_options(-Wall)
endif ()

if (WIN32)
    set(platform_source platform_win32.c)
elseif (UNIX)
    set(platform_source platform_posix.c)
endif ()

add_executable(
        ConsoleTetris
        main.c
        platform.h
        ${platform_source}
)
