cmake_minimum_required(VERSION 2.8)

project(jrtplibversion)

find_package(JThread REQUIRED)
find_package(JRTPLIB REQUIRED)

include_directories(${JTHREAD_INCLUDE_DIRS})
include_directories(${JRTPLIB_INCLUDE_DIRS})

add_executable(jrtplibversiontest jrtplibversion.cpp)
target_link_libraries(jrtplibversiontest ${JTHREAD_LIBRARIES} ${JRTPLIB_LIBRARIES})

