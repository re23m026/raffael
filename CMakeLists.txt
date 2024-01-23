cmake_minimum_required(VERSION 3.22.1)

project(raffael)

include_directories(include)

file(GLOB SOURCES "src/*.cpp")

add_executable(raffael ${SOURCES})