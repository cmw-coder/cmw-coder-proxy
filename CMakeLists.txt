cmake_minimum_required(VERSION 3.26)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json VCPKG_JSON)
string(JSON VERSION_STRING GET ${VCPKG_JSON} version-string)
message(STATUS "Project version: ${VERSION_STRING}")

set(USE_ZLIB OFF CACHE BOOL "Use zlib" FORCE)

project(cmw-coder-proxy CXX)

if ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
    message("Architecture: i386")
else ()
    message(FATAL_ERROR "Platform arch is not supported")
endif ()
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    add_compile_options(/MP /utf-8)
    add_compile_definitions(
            _CRT_SECURE_NO_WARNINGS
            NOMINMAX
            NOGDI
            WIN32_LEAN_AND_MEAN
    )
else ()
    message(FATAL_ERROR "Compiler is not supported")
endif ()

set(PROXY_MODULE_NAME "msimg32")

add_compile_definitions(CMAKE_PROJECT_NAME="${PROJECT_NAME}")
add_compile_definitions(PROXY_MODULE_NAME="${PROXY_MODULE_NAME}")
add_compile_definitions(VERSION_STRING="${VERSION_STRING}")

add_library(${PROXY_MODULE_NAME} SHARED proxy.cc main.cc)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fetch_ced.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fetch_libreadtags.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fetch_singleton.cmake)

find_package(ixwebsocket CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(${PROXY_MODULE_NAME} PRIVATE
        ced
        ixwebsocket::ixwebsocket
        magic_enum::magic_enum
        nlohmann_json::nlohmann_json
        singleton::singleton
        universal-ctags::readtags
        Version.lib
)

target_include_directories(${PROXY_MODULE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/components/ COMPONENTS)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/helpers/ HELPERS)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/models/ MODELS)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/types/ TYPES)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/utils/ UTILS)

target_sources(${PROXY_MODULE_NAME} PRIVATE
        ${COMPONENTS}
        ${HELPERS}
        ${MODELS}
        ${TYPES}
        ${UTILS}
)