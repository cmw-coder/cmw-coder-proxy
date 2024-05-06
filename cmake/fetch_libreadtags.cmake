include(FetchContent)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libreadtags-master.zip")
    FetchContent_Declare(
            libreadtags
            URL file://${CMAKE_CURRENT_SOURCE_DIR}/cmake/libreadtags-master.zip
    )
else ()
    FetchContent_Declare(
            libreadtags
            GIT_REPOSITORY https://github.com/ParticleG/libreadtags.git
            GIT_TAG master
    )
endif ()

FetchContent_MakeAvailable(libreadtags)