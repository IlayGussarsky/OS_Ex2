cmake_minimum_required(VERSION 3.22)
project(OS_Ex2)

set(CMAKE_CXX_STANDARD 17)

add_executable(Ex2
        Tests/test0_sanity.cpp
        uthreads.h
        uthreads.cpp)
