cmake_minimum_required(VERSION 3.30)
project(dds_docs LANGUAGES CXX)

message(STATUS "DDS CMake support is documentation/IDE-only; use the repository Makefiles for supported library, example, and test builds.")

set(CMAKE_CXX_STANDARD 17)
#SET(BOOST_ROOT "/opt/local/libexec/boost/1.76/")

if(APPLE)
  add_compile_definitions(DDS_THREADS_GCD DDS_THREADS_STL)
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    add_compile_definitions(DDS_TARGET_APPLE_M1_MAX)
  endif()
  set(DDS_THREAD_INCLUDE_DIRS "")
else()
  find_package(Boost REQUIRED)
  set(DDS_THREAD_INCLUDE_DIRS ${Boost_INCLUDE_DIRS})
endif()

#find_package(OpenMP REQUIRED)

include_directories(../dds/include ../dds/examples /opt/homebrew/opt/libomp/include . ${DDS_THREAD_INCLUDE_DIRS})

find_package(Doxygen QUIET)

if(DOXYGEN_FOUND)
  add_custom_target(dds_docs
    COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../docs
    COMMENT "Generating DDS documentation with Doxygen"
    VERBATIM)
endif()

