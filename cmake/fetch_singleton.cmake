include(FetchContent)

set(SINGLETON_INJECT_ABSTRACT_CLASS ON)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/singleton-main.zip")
    FetchContent_Declare(
            singleton
            URL file://${CMAKE_CURRENT_SOURCE_DIR}/cmake/singleton-main.zip
    )
else ()
    FetchContent_Declare(
            singleton
            GIT_REPOSITORY https://github.com/jimmy-park/singleton.git
            GIT_TAG main
    )
endif ()

FetchContent_MakeAvailable(singleton)