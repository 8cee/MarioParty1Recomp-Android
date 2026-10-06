cmake_minimum_required(VERSION 3.20)
project(MarioParty1RecompWin64 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MP1_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(RT64_DIR "${MP1_ROOT}/lib/rt64")
set(NMR_DIR "${MP1_ROOT}/lib/N64ModernRuntime")
set(FRONTEND_DIR "${MP1_ROOT}/lib/RecompFrontend")

file(GLOB RECOMP_FUNCS "${MP1_ROOT}/recomp/generated/funcs_*.c")
if(NOT RECOMP_FUNCS)
    message(FATAL_ERROR "Generated Mario Party recomp sources are missing.")
endif()
if(NOT EXISTS "${MP1_ROOT}/recomp/rsp/aspMain.cpp")
    message(FATAL_ERROR "Generated Mario Party RSP source is missing.")
endif()

set(RT64_STATIC TRUE CACHE BOOL "" FORCE)
set(sdl2_SOURCE_DIR "${RT64_DIR}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3")

add_subdirectory("${RT64_DIR}" "${CMAKE_BINARY_DIR}/rt64")
add_subdirectory("${NMR_DIR}" "${CMAKE_BINARY_DIR}/n64modernruntime")

set(RECOMP_FRONTEND_N64MODERNRUNTIME_PATH "${NMR_DIR}")
set(RECOMP_FRONTEND_RT64_PATH "${RT64_DIR}")
add_subdirectory("${FRONTEND_DIR}" "${CMAKE_BINARY_DIR}/recompfrontend")

add_library(RecompiledFuncs STATIC ${RECOMP_FUNCS})
target_include_directories(RecompiledFuncs PRIVATE
    "${MP1_ROOT}/recomp/generated"
    "${NMR_DIR}/N64Recomp/include"
    "${NMR_DIR}/librecomp/include"
    "${NMR_DIR}/ultramodern/include")
if(MSVC)
    target_compile_options(RecompiledFuncs PRIVATE /W0)
else()
    target_compile_options(RecompiledFuncs PRIVATE -fno-strict-aliasing -Wno-unused-variable)
endif()

add_executable(MarioParty1Recomp
    mp1_main.cpp
    "${MP1_ROOT}/recomp/runtime/mp1_rsp_runtime.cpp"
    "${MP1_ROOT}/recomp/runtime/mp1_process_runtime.cpp"
    "${MP1_ROOT}/recomp/runtime/mp1_libultra_shims.cpp"
    "${MP1_ROOT}/recomp/runtime/mp1_overlays.cpp"
    "${MP1_ROOT}/recomp/generated/lookup.cpp"
    "${MP1_ROOT}/recomp/rsp/aspMain.cpp"
)

target_include_directories(MarioParty1Recomp PRIVATE
    "${MP1_ROOT}/recomp/generated"
    "${MP1_ROOT}/recomp/runtime"
    "${NMR_DIR}/N64Recomp/include"
    "${NMR_DIR}/librecomp/include"
    "${NMR_DIR}/ultramodern/include"
    "${NMR_DIR}/thirdparty"
    "${FRONTEND_DIR}/recompinput/include"
    "${FRONTEND_DIR}/recompui/include"
    "${FRONTEND_DIR}/recompui/src"
    "${RT64_DIR}/src/contrib"
    "${RT64_DIR}/src/contrib/hlslpp/include"
    "${RT64_DIR}/src/contrib/dxc/inc"
    "${RT64_DIR}/src"
    "${RT64_DIR}/src/rhi"
    "${RT64_DIR}/src/render"
    "${sdl2_SOURCE_DIR}/include"
)

target_compile_definitions(MarioParty1Recomp PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(MarioParty1Recomp PRIVATE /W3)
    target_link_options(MarioParty1Recomp PRIVATE /OPT:NOICF)
else()
    target_compile_options(MarioParty1Recomp PRIVATE -fno-strict-aliasing -fms-extensions)
endif()

target_link_directories(MarioParty1Recomp PRIVATE "${sdl2_SOURCE_DIR}/lib/x64")
target_link_libraries(MarioParty1Recomp PRIVATE
    RecompiledFuncs
    recompui
    recompinput
    librecomp
    ultramodern
    rt64
    nfd
    SDL2
    winmm
)

add_custom_command(TARGET MarioParty1Recomp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${sdl2_SOURCE_DIR}/lib/x64/SDL2.dll"
        "${RT64_DIR}/src/contrib/dxc/bin/x64/dxil.dll"
        "${RT64_DIR}/src/contrib/dxc/bin/x64/dxcompiler.dll"
        "$<TARGET_FILE_DIR:MarioParty1Recomp>"
)
