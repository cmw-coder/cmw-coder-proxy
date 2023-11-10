include(FetchContent)

set(SINGLETON_INJECT_ABSTRACT_CLASS ON)

FetchContent_Declare(
        singleton
        GIT_REPOSITORY https://github.com/jimmy-park/singleton.git
        GIT_TAG main
)
FetchContent_MakeAvailable(singleton)