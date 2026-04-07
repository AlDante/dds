cmake_minimum_required(VERSION 3.30)
project(dds)

message(STATUS,  "CMAKE_MODULE_PATH is: " ${CMAKE_MODULE_PATH})

set(CMAKE_CXX_STANDARD 17)
#SET(BOOST_ROOT "/opt/local/libexec/boost/1.76/")

if(APPLE)
  add_compile_definitions(DDS_THREADS_GCD DDS_THREADS_STL)
  set(DDS_THREAD_INCLUDE_DIRS "")
else()
  find_package(Boost REQUIRED)
  set(DDS_THREAD_INCLUDE_DIRS ${Boost_INCLUDE_DIRS})
endif()

#find_package(OpenMP REQUIRED)

include_directories(../dds/include ../dds/examples /opt/homebrew/opt/libomp/include  . ${DDS_THREAD_INCLUDE_DIRS} )
#target_link_libraries(dds ${Boost_LIBRARIES})
