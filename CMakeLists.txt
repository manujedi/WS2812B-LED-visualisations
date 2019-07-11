cmake_minimum_required(VERSION 2.8)
project(leds)

add_definitions(-DPC)

set(SDL2_INCLUDE_DIR /usr/include/SDL2)
set(SDL2_LIBRARY /usr/lib/libSDL2.so)

# includes cmake/FindSDL2.cmake
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

set(SOURCE_FILES main.c)

add_executable(leds ${SOURCE_FILES})
target_link_libraries(leds ${SDL2_LIBRARY})