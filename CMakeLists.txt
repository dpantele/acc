cmake_minimum_required(VERSION 3.2)

include("cmake/HunterGate.cmake")
HunterGate(
    URL "https://github.com/ruslo/hunter/archive/v0.12.31.tar.gz"
    SHA1 "93d813520a528c5e2faf8acfce39daed5d3b28b8"
)

project(acc)

# guard against in-source builds

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt. ")
endif()

# to make things easier, we require c++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED On)

# guard against bad build-type strings

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" cmake_build_type_tolower)
if(    NOT cmake_build_type_tolower STREQUAL "debug"
    AND NOT cmake_build_type_tolower STREQUAL "release"
    AND NOT cmake_build_type_tolower STREQUAL "relwithdebinfo"
    AND NOT cmake_build_type_tolower STREQUAL "minsizerel")
  message(FATAL_ERROR "Unknown build type \"${CMAKE_BUILD_TYPE}\". Allowed values are Debug, Release, RelWithDebInfo, MinSizeRel (case-insensitive).")
endif()

enable_testing()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"
    OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer -Wall -Werror")
endif()

include_directories(.)

add_subdirectory(googletest/googletest)
target_include_directories(gtest INTERFACE googletest/googletest/include)

add_subdirectory(crag)