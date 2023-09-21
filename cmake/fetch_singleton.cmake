include(FetchContent)

set(SINGLETON_INJECT_ABSTRACT_CLASS ON)

FetchContent_Declare(
        singleton
        URL https://github.com/jimmy-park/singleton/archive/main.tar.gz
)
FetchContent_MakeAvailable(singleton)