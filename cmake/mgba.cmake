include(FetchContent)

FetchContent_Declare(
    mgba
    GIT_REPOSITORY https://github.com/mgba-emu/mgba.git
    GIT_TAG 0.10.5
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(mgba)
