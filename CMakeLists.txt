cmake_minimum_required(VERSION 3.22)
project(Ex2)

set(CMAKE_CXX_STANDARD 11)

add_executable(Ex2
        Tests/test0_sanity.cpp
        uthreads.h
        uthreads.cpp)
