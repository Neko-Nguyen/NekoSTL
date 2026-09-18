cmake_minimum_required(VERSION 3.20)
project(neko LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

option(NEKO_SANITIZE "Build tests with ASan + UBSan" ON)

# The library itself is header-only: nothing to compile, just an include path.
add_library(neko INTERFACE)
target_include_directories(neko INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Warnings you want turned on while learning: they catch the classic
# container-implementation mistakes (shadowing, narrowing, missing returns).
add_library(neko_warnings INTERFACE)
target_compile_options(neko_warnings INTERFACE
  -Wall -Wextra -Wpedantic
  -Wshadow
  -Wnon-virtual-dtor
  -Wold-style-cast
  -Wcast-align
  -Wunused
  -Woverloaded-virtual
  -Wnull-dereference
  -Wdouble-promotion
)

add_library(neko_sanitizers INTERFACE)
if(NEKO_SANITIZE)
  target_compile_options(neko_sanitizers INTERFACE
    -fsanitize=address,undefined -fno-omit-frame-pointer -g)
  target_link_options(neko_sanitizers INTERFACE -fsanitize=address,undefined)
endif()

enable_testing()
add_subdirectory(tests)
