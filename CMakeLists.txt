cmake_minimum_required(VERSION 3.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

project(test_nanosleep LANGUAGES C)

add_executable(test_nanosleep test_nanosleep.c)