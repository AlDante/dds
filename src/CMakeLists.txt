cmake_minimum_required(VERSION 3.30)
project(dds)

message(STATUS,  "CMAKE_MODULE_PATH is: " ${CMAKE_MODULE_PATH})

set(CMAKE_CXX_STANDARD 17)
#SET(BOOST_ROOT "/opt/local/libexec/boost/1.76/")

find_package(Boost REQUIRED)
#find_package(OpenMP REQUIRED)

include_directories(../dds/include ../dds/examples /opt/homebrew/opt/libomp/include  . ${Boost_INCLUDE_DIRS} )
#target_link_libraries(dds ${Boost_LIBRARIES})
