include(FetchContent)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/compact_enc_det-master.zip")
    FetchContent_Declare(
            ced
            URL file://${CMAKE_CURRENT_SOURCE_DIR}/cmake/compact_enc_det-master.zip
    )
else ()
    FetchContent_Declare(
            ced
            GIT_REPOSITORY https://github.com/ParticleG/compact_enc_det
            GIT_TAG master
    )
endif ()

FetchContent_MakeAvailable(ced)