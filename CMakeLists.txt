cmake_minimum_required(VERSION 3.20)
project(gbnh-native VERSION 0.2.0 LANGUAGES C)

# mGBA 0.10.5 declares a policy minimum older than CMake 4 accepts.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(LIBMGBA_ONLY ON CACHE BOOL "" FORCE)
set(M_CORE_GBA ON CACHE BOOL "" FORCE)
set(M_CORE_GB OFF CACHE BOOL "" FORCE)

include(cmake/mgba.cmake)

if(NOT WIN32)
    message(FATAL_ERROR "The packaged JNI runtime currently supports Windows only")
endif()

add_library(gbnh_native SHARED src/gbnh_native.c src/gbnh_jni.c)
target_include_directories(gbnh_native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party/jni
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party/jni/win32)
target_compile_features(gbnh_native PRIVATE c_std_11)

if(NOT TARGET mgba)
    message(FATAL_ERROR "mGBA target was not created; check the pinned mGBA configuration")
endif()
target_compile_definitions(gbnh_native PRIVATE GBNH_HAS_MGBA=1 GBNH_NATIVE_BUILD=1)
target_link_libraries(gbnh_native PRIVATE mgba)
if(MINGW)
    # Do not depend on GCC or winpthread runtime DLLs in local UCRT64 builds.
    target_link_options(gbnh_native PRIVATE -static -static-libgcc)
endif()

set_target_properties(gbnh_native PROPERTIES OUTPUT_NAME gbnh_mgba)
if(WIN32)
    set_target_properties(gbnh_native PROPERTIES PREFIX "")
endif()

# Install only our runtime component, not mGBA's SDK, frontends, or static archive.
install(TARGETS gbnh_native
    RUNTIME DESTINATION native COMPONENT gbnh-native
    LIBRARY DESTINATION native COMPONENT gbnh-native)
install(FILES "${mgba_SOURCE_DIR}/LICENSE" DESTINATION native
    RENAME LICENSE-mgba.txt COMPONENT gbnh-native)
install(DIRECTORY "${mgba_SOURCE_DIR}/res/licenses/" DESTINATION native/licenses
    COMPONENT gbnh-native FILES_MATCHING PATTERN "*.txt")
install(FILES LICENSE-mgba.txt DESTINATION native
    RENAME SOURCE-mgba.txt COMPONENT gbnh-native)
install(FILES third-party/jni/LICENSE DESTINATION native
    RENAME LICENSE-jni.txt COMPONENT gbnh-native)
install(FILES jni-abi.properties DESTINATION native COMPONENT gbnh-native)
