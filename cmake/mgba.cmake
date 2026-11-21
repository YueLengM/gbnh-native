include(FetchContent)

FetchContent_Declare(
    mgba
    GIT_REPOSITORY https://github.com/mgba-emu/mgba.git
    GIT_TAG 0.10.5
    GIT_SHALLOW TRUE
)

# mGBA 0.10.5 declares a policy version older than CMake 4 accepts.
# Raise the dependency's compatibility floor without changing its fetched source
# or overriding a newer floor explicitly requested by the caller.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM OR CMAKE_POLICY_VERSION_MINIMUM VERSION_LESS "3.5")
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    endif()
endif()

FetchContent_MakeAvailable(mgba)
