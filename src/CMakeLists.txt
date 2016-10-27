cmake_minimum_required(VERSION 2.6)

project(WSL-FS)

set(CMAKE_C_FLAGS "-Wall -Wextra -std=c11 -D_DEFAULT_SOURCE")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../cmake/")

find_package(Fuse REQUIRED)

add_definitions(${FUSE_DEFINITIONS})
include_directories(${FUSE_INCLUDE_DIRS})
add_executable(wslfs wslfs.c)
target_link_libraries(wslfs ${FUSE_LIBRARIES})

install(TARGETS wslfs DESTINATION bin)
