# ----------------------------------------------------------------------------
#  NeoVis
#  Copyright(c) 2017-2024 Ogma Intelligent Systems Corp. All rights reserved.
#
#  This copy of NeoVis is licensed to you under the terms described
#  in the NEOVIS_LICENSE.md file included in this distribution.
# ----------------------------------------------------------------------------

# CMake version 3.6+ is required to enable C++14 features cleanly
cmake_minimum_required(VERSION 3.13)

project(NeoVis)
 
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

if(NOT CMAKE_BUILD_TYPE)
    message("CMAKE_BUILD_TYPE not set, setting it to Release")
    set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(BITNESS 64)
else()
  set(BITNESS 32)
endif()
message(STATUS "Bitness: ${BITNESS}")


############################################################################
# Add OpenGL

find_package(OpenGL REQUIRED)


############################################################################
# Find SFML include and libs

find_package(SFML 3 COMPONENTS System Window Graphics Network)

include_directories(${SFML_INCLUDE_DIRS})


############################################################################
#

file(GLOB_RECURSE NEOVIS_SRC
    "source/*.h"
    "source/*.cpp"
)

add_executable(NeoVis ${NEOVIS_SRC})

set_property(TARGET NeoVis PROPERTY CXX_STANDARD 14)
set_property(TARGET NeoVis PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(NeoVis SFML::System SFML::Window SFML::Graphics SFML::Network)
target_link_libraries(NeoVis OpenGL::OpenGL)
