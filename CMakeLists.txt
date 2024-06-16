cmake_minimum_required(VERSION 3.22)
project(OS_Ex2)

set(CMAKE_CXX_STANDARD 17)

add_executable(Ex2
        uthreads.cpp
        uthreads.h
        NewTests/DriveTests/test1.in.cpp)
