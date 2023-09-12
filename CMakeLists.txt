cmake_minimum_required(VERSION 3.26)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
project(msimg32)

add_compile_definitions(CMAKE_PROJECT_NAME="${PROJECT_NAME}")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    message(STATUS "Compiler is Clang")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    message(STATUS "Compiler is GCC")
    add_compile_options(-Wno-volatile)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    message(STATUS "Compiler is Intel C++")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    message(STATUS "Compiler is Visual Studio C++")
    add_compile_options(/MP /source-charset:utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
endif ()

add_library(${PROJECT_NAME} SHARED library.cc)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/types/ TYPES)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/utils/ UTILS)

target_sources(${PROJECT_NAME} PRIVATE ${TYPES} ${UTILS})

install(FILES
        ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.dll
        DESTINATION
        "C:/Program Files (x86)/Source Insight 3/")
