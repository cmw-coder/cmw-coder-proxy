cmake_minimum_required(VERSION 3.26)
project(msimg32)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} SHARED library.cc)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/types/ TYPES)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/utils/ UTILS)

target_sources(${PROJECT_NAME} PRIVATE ${TYPES} ${UTILS})
