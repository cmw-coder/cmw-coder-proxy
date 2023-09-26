cmake_minimum_required(VERSION 3.26)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PROXY_MODULE_NAME "msimg32")

project(cmw-coder-proxy C CXX)

add_compile_definitions(CMAKE_PROJECT_NAME="${PROJECT_NAME}")
add_compile_definitions(PROXY_MODULE_NAME="${PROXY_MODULE_NAME}")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    message(STATUS "Compiler is Clang")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    message(STATUS "Compiler is GCC")
    add_compile_options(-Wno-volatile)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    message(STATUS "Compiler is Intel C++")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    message(STATUS "Compiler is Visual Studio C++")
    add_compile_options(/MP /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
endif ()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fetch_jsoncpp.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fetch_singleton.cmake)

add_library(${PROXY_MODULE_NAME} SHARED proxy.c main.cc)

target_link_libraries(${PROXY_MODULE_NAME} PRIVATE
        jsoncpp_static
        singleton::singleton
        Version.lib
)

target_include_directories(${PROXY_MODULE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/types/ TYPES)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/utils/ UTILS)

target_sources(${PROXY_MODULE_NAME} PRIVATE ${TYPES} ${UTILS})

add_custom_command(TARGET ${PROXY_MODULE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${PROXY_MODULE_NAME}.dll "C:/Program Files (x86)/Source Insight 3/"
)
