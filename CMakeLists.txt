cmake_minimum_required(VERSION 3.20)
project(gbnh-native VERSION 0.1.0 LANGUAGES C)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(LIBMGBA_ONLY ON CACHE BOOL "" FORCE)
set(M_CORE_GBA ON CACHE BOOL "" FORCE)
set(M_CORE_GB OFF CACHE BOOL "" FORCE)

include(cmake/mgba.cmake)

add_library(gbnh_native SHARED src/gbnh_native.c)
target_include_directories(gbnh_native PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(gbnh_native PRIVATE c_std_11)

if(NOT TARGET mgba)
    message(FATAL_ERROR "mGBA target was not created; check the pinned mGBA configuration")
endif()
target_compile_definitions(gbnh_native PRIVATE GBNH_HAS_MGBA=1)
target_link_libraries(gbnh_native PRIVATE mgba)

set_target_properties(gbnh_native PROPERTIES OUTPUT_NAME gbnh_mgba)

include(GNUInstallDirs)
install(TARGETS gbnh_native RUNTIME DESTINATION native LIBRARY DESTINATION native)
install(FILES LICENSE-mgba.txt DESTINATION native OPTIONAL)
