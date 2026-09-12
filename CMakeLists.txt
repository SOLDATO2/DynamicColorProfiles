cmake_minimum_required(VERSION 4.0)
project(DynamicColorProfiles)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

file(GLOB MODULE "src/*.ixx")

add_executable(DynamicColorProfiles main.cpp)

target_sources(DynamicColorProfiles
        PRIVATE
        FILE_SET CXX_MODULES FILES
        ${MODULE}
)
