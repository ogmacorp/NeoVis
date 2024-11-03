# ----------------------------------------------------------------------------
#  NeoVis
#  Copyright(c) 2017-2024 Ogma Intelligent Systems Corp. All rights reserved.
#
#  This copy of NeoVis is licensed to you under the terms described
#  in the NEOVIS_LICENSE.md file included in this distribution.
# ----------------------------------------------------------------------------

# CMake version 3.1+ is required to enable C++14 features cleanly
cmake_minimum_required(VERSION 3.1)

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
# Add SFML

find_package(SFML 2 COMPONENTS system window graphics network)

message(STATUS "SFML incs: " ${SFML_INCLUDE_DIR})
message(STATUS "SFML libs: " ${SFML_LIBRARIES})


############################################################################
#

file(GLOB_RECURSE NEOVIS_SRC
    "source/*.cpp"
)

add_executable(NeoVis ${NEOVIS_SRC})

set_property(TARGET NeoVis PROPERTY CXX_STANDARD 14)
set_property(TARGET NeoVis PROPERTY CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    if(BITNESS EQUAL 64)
        set_target_properties(NeoVis PROPERTIES COMPILE_FLAGS "-m64" LINK_FLAGS "-m64")
    endif()
endif()

target_include_directories(NeoVis PUBLIC ${SFML_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR})

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    target_link_libraries(NeoVis ${SFML_LIBRARIES} ${OPENGL_LIBRARIES} ${SFML_DEPENDENCIES} -lpthread)
else()
    target_link_libraries(NeoVis ${SFML_LIBRARIES} ${OPENGL_LIBRARIES} ${SFML_DEPENDENCIES})
endif()
