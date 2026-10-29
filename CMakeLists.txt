cmake_minimum_required(VERSION 3.21)
project(ainwin LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
add_executable(ainwin src/app/main.cpp)
target_include_directories(ainwin PRIVATE vendor src/core src/search)
target_compile_definitions(ainwin PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(ainwin PRIVATE /utf-8)
# The C runtime linked in: the exe needs no DLL, and the strict sandbox (low integrity) loads nothing else.
set_target_properties(ainwin PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# Built straight into the plugin folder, where plugin.json names it.
set_target_properties(ainwin PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}" RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}")
