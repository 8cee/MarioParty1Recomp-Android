cmake_minimum_required(VERSION 3.22.1)
project(mp1_android LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(MP1_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../..")

set(RT64_STATIC TRUE CACHE BOOL "" FORCE)
set(RT64_SDL_WINDOW_VULKAN TRUE CACHE BOOL "" FORCE)

if(NOT EXISTS "${MP1_ROOT}/lib/N64ModernRuntime/CMakeLists.txt")
    message(FATAL_ERROR "N64ModernRuntime submodule missing; checkout recursively.")
endif()
if(NOT EXISTS "${MP1_ROOT}/lib/rt64/CMakeLists.txt")
    message(FATAL_ERROR "RT64 submodule missing; checkout recursively.")
endif()
if(NOT EXISTS "${MP1_ROOT}/lib/RecompFrontend/CMakeLists.txt")
    message(FATAL_ERROR "RecompFrontend submodule missing; checkout recursively.")
endif()

include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
FetchContent_Declare(SDL2 URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.8/SDL2-2.30.8.tar.gz)
FetchContent_MakeAvailable(SDL2)

add_subdirectory("${MP1_ROOT}/lib/rt64" "${CMAKE_BINARY_DIR}/rt64")
add_subdirectory("${MP1_ROOT}/lib/N64ModernRuntime" "${CMAKE_BINARY_DIR}/n64modernruntime")
add_subdirectory("${MP1_ROOT}/lib/RecompFrontend" "${CMAKE_BINARY_DIR}/recompfrontend")

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
    diagnostics.cpp
    mp1_main.cpp
    "${MP1_ROOT}/recomp/runtime/mp1_rsp_runtime.cpp"
    "${MP1_ROOT}/recomp/rsp/aspMain.cpp"
)
target_include_directories(main PRIVATE
    "${MP1_ROOT}/recomp/generated"
    "${MP1_ROOT}/recomp/runtime"
    "${MP1_ROOT}/lib/N64ModernRuntime/include"
    "${MP1_ROOT}/lib/RecompFrontend/include"
)
target_link_libraries(main PRIVATE
    RecompiledFuncs
    recompui
    recompinput
    librecomp
    ultramodern
    rt64
    SDL2-static
    android
    log
    ${CMAKE_DL_LIBS}
)
target_link_options(main PRIVATE "-Wl,-z,max-page-size=16384")
