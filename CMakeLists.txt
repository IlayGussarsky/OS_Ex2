cmake_minimum_required(VERSION 3.22)
project(OS_Ex2)

set(CMAKE_CXX_STANDARD 11)

add_executable(Ex2
        uthreads.h
        uthreads.cpp
        NewTests/DriveTests/test4.in.cpp
)
