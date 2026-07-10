cmake_minimum_required(VERSION 3.10)
project(mymod)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(mymod SHARED src/mymod.cpp)

find_library(log-lib log)
target_link_libraries(mymod ${log-lib})

set_target_properties(mymod PROPERTIES
    OUTPUT_NAME "mymod"
)
