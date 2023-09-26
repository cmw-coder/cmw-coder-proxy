include(FetchContent)

set(JSONCPP_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_PKGCONFIG_SUPPORT OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        jsoncpp
        URL https://github.com/open-source-parsers/jsoncpp/archive/master.tar.gz
)

FetchContent_GetProperties(jsoncpp)
if (NOT jsoncpp_POPULATED)
    message("-- Populating jsoncpp...")
    FetchContent_Populate(jsoncpp)
    add_subdirectory(${jsoncpp_SOURCE_DIR} ${jsoncpp_BINARY_DIR})
endif ()

FetchContent_MakeAvailable(jsoncpp)