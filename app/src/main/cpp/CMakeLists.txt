cmake_minimum_required(VERSION 3.22.1)
project(mp1runtime)

add_library(mp1runtime SHARED runtime_bridge.cpp)

find_library(log-lib log)

target_compile_features(mp1runtime PRIVATE cxx_std_20)
target_link_libraries(mp1runtime ${log-lib})
