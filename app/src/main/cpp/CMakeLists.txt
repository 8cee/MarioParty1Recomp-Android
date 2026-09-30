cmake_minimum_required(VERSION 3.22.1)
project(mp1_android LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(MP1_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../..")

file(GLOB RECOMP_FUNCS "${MP1_ROOT}/recomp/generated/funcs_*.c")
if(NOT RECOMP_FUNCS)
    message(FATAL_ERROR "Mario Party recompiled sources are missing. Run tools/build-mp1-recomp.sh with the verified US ROM before building the playable APK.")
endif()
if(NOT EXISTS "${MP1_ROOT}/recomp/rsp/aspMain.cpp")
    message(FATAL_ERROR "Mario Party RSP output is missing.")
endif()

add_library(RecompiledFuncs STATIC ${RECOMP_FUNCS})
target_include_directories(RecompiledFuncs PUBLIC "${MP1_ROOT}/recomp/generated")

add_library(main SHARED
    runtime_bridge.cpp
    mp1_main.cpp
    "${MP1_ROOT}/recomp/runtime/mp1_rsp_runtime.cpp"
    "${MP1_ROOT}/recomp/rsp/aspMain.cpp"
)
target_include_directories(main PRIVATE "${MP1_ROOT}/recomp/generated" "${MP1_ROOT}/recomp/runtime")
target_link_libraries(main PRIVATE RecompiledFuncs android log)
target_link_options(main PRIVATE "-Wl,-z,max-page-size=16384")
