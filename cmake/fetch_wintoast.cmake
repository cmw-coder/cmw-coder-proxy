include(FetchContent)

set(WINTOASTLIB_BUILD_EXAMPLES OFF)

FetchContent_Declare(
        WinToast
        GIT_REPOSITORY https://github.com/mohabouje/WinToast.git
        GIT_TAG master
)
FetchContent_MakeAvailable(WinToast)