cmake_minimum_required(VERSION 3.22.1)
project(mp1_android LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(MP1_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../..")
set(RT64_DIR "${MP1_ROOT}/lib/rt64")
set(NMR_DIR "${MP1_ROOT}/lib/N64ModernRuntime")
set(FRONTEND_DIR "${MP1_ROOT}/lib/RecompFrontend")
set(ANDROID_PORT_DIR "${MP1_ROOT}/android")

# ROM-derived sources are deliberately private. Fail before configuring the
# heavyweight runtime tree when codegen has not been run.
file(GLOB RECOMP_FUNCS "${MP1_ROOT}/recomp/generated/funcs_*.c")
if(NOT RECOMP_FUNCS)
    message(FATAL_ERROR
        "Mario Party recompiled sources are missing. Run tools/build-mp1-recomp.sh "
        "with the verified US ROM before building the playable APK.")
endif()
if(NOT EXISTS "${MP1_ROOT}/recomp/rsp/aspMain.cpp")
    message(FATAL_ERROR "Mario Party RSP output is missing.")
endif()

foreach(_dep
        "${NMR_DIR}/CMakeLists.txt"
        "${RT64_DIR}/CMakeLists.txt"
        "${FRONTEND_DIR}/CMakeLists.txt")
    if(NOT EXISTS "${_dep}")
        message(FATAL_ERROR "Runtime submodule missing: ${_dep}")
    endif()
endforeach()

# Host-only RT64 embedding tool. Gradle receives -PHOST_FILE_TO_C from the
# private CI, then forwards it as -DHOST_FILE_TO_C.
set(HOST_FILE_TO_C "" CACHE FILEPATH "Host file_to_c executable")
set(FILE_TO_C_EXE "" CACHE FILEPATH "Host file_to_c executable used by RT64")
if(NOT FILE_TO_C_EXE AND HOST_FILE_TO_C)
    set(FILE_TO_C_EXE "${HOST_FILE_TO_C}" CACHE FILEPATH "" FORCE)
endif()
if(NOT FILE_TO_C_EXE OR NOT EXISTS "${FILE_TO_C_EXE}")
    message(FATAL_ERROR
        "HOST_FILE_TO_C is required for Android cross-build shader embedding")
endif()

list(APPEND CMAKE_MODULE_PATH "${ANDROID_PORT_DIR}/cmake/Modules")

set(RT64_STATIC TRUE)
set(RT64_SDL_WINDOW_VULKAN TRUE)
# Match the working DK64/Banjo Android ports: only the static Zstd runtime is
# needed on-device. Its CLI/tests are host programs and can break cross-builds.
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(PLUME_SDL_VULKAN_ENABLED TRUE)
add_compile_definitions(
    HLSL_CPU
    PLUME_SDL_VULKAN_ENABLED
    RT64_SDL_WINDOW_VULKAN
    RECOMP_ANDROID
)

include(FetchContent)

# SDL C and SDLActivity Java are kept on the same 2.30.8 release.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    sdl2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.8/SDL2-2.30.8.tar.gz
    URL_HASH SHA256=380c295ea76b9bd72d90075793971c8bcb232ba0a69a9b14da4ae8f603350058
)
FetchContent_MakeAvailable(sdl2)

# Source-tree SDL exposes <SDL.h>, while the recomp stack also includes
# <SDL2/SDL.h>. Build a compatibility include directory for both spellings.
set(SDL2_COMPAT_INCLUDE "${CMAKE_BINARY_DIR}/sdl2inc")
file(MAKE_DIRECTORY "${SDL2_COMPAT_INCLUDE}/SDL2")
file(GLOB _sdl_headers "${sdl2_SOURCE_DIR}/include/*.h")
foreach(_h ${_sdl_headers})
    get_filename_component(_name "${_h}" NAME)
    if(NOT _name MATCHES "^(SDL_config|SDL_revision)")
        file(COPY "${_h}" DESTINATION "${SDL2_COMPAT_INCLUDE}/SDL2")
    endif()
endforeach()
set(SDL2_ALL_INCLUDES
    "${sdl2_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>/SDL2"
    "${SDL2_COMPAT_INCLUDE}"
    "${sdl2_BINARY_DIR}/include"
    "${sdl2_SOURCE_DIR}/include"
)
set(SDL2_INCLUDE_DIRS "${SDL2_ALL_INCLUDES}")

if(NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 INTERFACE IMPORTED GLOBAL)
    set_target_properties(SDL2::SDL2 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_ALL_INCLUDES}"
        INTERFACE_LINK_LIBRARIES SDL2-static)
endif()

# Android has no desktop native-file-dialog implementation in this port.
add_library(nfd STATIC "${ANDROID_PORT_DIR}/native/stubs/nfd_stub.c")
target_include_directories(nfd PUBLIC
    "${RT64_DIR}/src/contrib/nativefiledialog-extended/src/include")

# RecompFrontend links its optional online mod-store client even though MP1 is
# currently local-only. Supply a small offline libcurl ABI shim.
add_library(curl_stub STATIC "${ANDROID_PORT_DIR}/native/stubs/curl_stub.cpp")
target_include_directories(curl_stub PUBLIC
    "${ANDROID_PORT_DIR}/native/stubs/include")
set(CURL_FAKE_CONFIG_DIR "${CMAKE_BINARY_DIR}/fakecurl")
file(MAKE_DIRECTORY "${CURL_FAKE_CONFIG_DIR}")
file(WRITE "${CURL_FAKE_CONFIG_DIR}/CURLConfig.cmake" "
if(NOT TARGET CURL::libcurl)
  add_library(CURL::libcurl INTERFACE IMPORTED GLOBAL)
  set_target_properties(CURL::libcurl PROPERTIES INTERFACE_LINK_LIBRARIES curl_stub)
endif()
set(CURL_FOUND TRUE)
")
set(CURL_DIR "${CURL_FAKE_CONFIG_DIR}" CACHE PATH "" FORCE)

# RmlUi requires Freetype. Build a small dependency-only configuration from
# source so the Android build never tries to locate a host library.
FetchContent_Declare(
    freetype
    URL https://github.com/freetype/freetype/archive/refs/tags/VER-2-13-2.tar.gz
    URL_HASH SHA256=427201f5d5151670d05c1f5b45bef5dda1f2e7dd971ef54f0feaaa7ffd2ab90c
)
set(FT_DISABLE_ZLIB TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI TRUE CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(freetype)
find_package(Freetype REQUIRED)

# Shader compiler commands run on the x86_64 CI host even though CMake targets
# arm64-v8a. RT64's Android patch makes the same host/target distinction.
set(DXC
    "LD_LIBRARY_PATH=${RT64_DIR}/src/contrib/dxc/lib/x64"
    "${RT64_DIR}/src/contrib/dxc/bin/x64/dxc-linux")
set(DXC_COMMON_OPTS "-I${RT64_DIR}/src")
set(DXC_DXIL_OPTS "-Wno-ignored-attributes")
set(DXC_SPV_OPTS "-spirv" "-fspv-target-env=vulkan1.0" "-fvk-use-dx-layout")
set(DXC_PS_OPTS "${DXC_COMMON_OPTS}" "-E" "PSMain" "-T ps_6_3")
set(DXC_VS_OPTS "${DXC_COMMON_OPTS}" "-E" "VSMain" "-T vs_6_3" "-fvk-invert-y")
set(DXC_CS_OPTS "${DXC_COMMON_OPTS}" "-E" "CSMain" "-T cs_6_3")
set(DXC_GS_OPTS "${DXC_COMMON_OPTS}" "-E" "GSMain" "-T gs_6_3")
set(DXC_LB_OPTS "${DXC_COMMON_OPTS}" "-D" "LIBRARY" "-T" "lib_6_3")
set(DXC_RT_OPTS "${DXC_COMMON_OPTS}" "-D" "RT_SHADER" "-T" "lib_6_3"
    "-fspv-target-env=vulkan1.1spirv1.4"
    "-fspv-extension=SPV_KHR_ray_tracing"
    "-fspv-extension=SPV_EXT_descriptor_indexing")

add_subdirectory("${RT64_DIR}" "${CMAKE_BINARY_DIR}/rt64")
add_subdirectory("${NMR_DIR}" "${CMAKE_BINARY_DIR}/n64modernruntime")

set(RECOMP_FRONTEND_N64MODERNRUNTIME_PATH "${NMR_DIR}")
set(RECOMP_FRONTEND_RT64_PATH "${RT64_DIR}")
add_subdirectory("${FRONTEND_DIR}" "${CMAKE_BINARY_DIR}/recompfrontend")

# Android isn't covered by the desktop SDL include branches in these projects.
foreach(_target rt64 plume ultramodern librecomp recompui recompinput)
    if(TARGET ${_target})
        target_include_directories(${_target} PRIVATE ${SDL2_ALL_INCLUDES})
    endif()
endforeach()

add_library(RecompiledFuncs STATIC ${RECOMP_FUNCS})
target_compile_options(RecompiledFuncs PRIVATE
    -fno-strict-aliasing
    -Wno-unused-variable
    -Wno-implicit-function-declaration)
target_include_directories(RecompiledFuncs PRIVATE
    "${MP1_ROOT}/recomp/generated"
    "${NMR_DIR}/N64Recomp/include"
    "${NMR_DIR}/librecomp/include"
    "${NMR_DIR}/ultramodern/include")

add_library(main SHARED
    runtime_bridge.cpp
    diagnostics.cpp
    mp1_main.cpp
    "${MP1_ROOT}/recomp/runtime/mp1_rsp_runtime.cpp"
    "${MP1_ROOT}/recomp/runtime/mp1_process_runtime.cpp"
    "${MP1_ROOT}/recomp/rsp/aspMain.cpp"
)

target_include_directories(main PRIVATE
    "${MP1_ROOT}/recomp/generated"
    "${MP1_ROOT}/recomp/runtime"
    "${NMR_DIR}/N64Recomp/include"
    "${NMR_DIR}/librecomp/include"
    "${NMR_DIR}/ultramodern/include"
    "${NMR_DIR}/thirdparty/sse2neon"
    "${FRONTEND_DIR}/recompinput/include"
    "${FRONTEND_DIR}/recompui/include"
    "${FRONTEND_DIR}/recompui/src"
    "${RT64_DIR}/src/contrib"
    "${RT64_DIR}/src/contrib/hlslpp/include"
    "${RT64_DIR}/src/contrib/dxc/inc"
    "${RT64_DIR}/src"
    "${RT64_DIR}/src/rhi"
    "${RT64_DIR}/src/render"
    ${SDL2_ALL_INCLUDES}
)

target_compile_options(main PRIVATE
    -fno-strict-aliasing
    -fms-extensions
    -Wno-unused-variable
    -Wno-unused-parameter)

target_link_libraries(main PRIVATE
    RecompiledFuncs
    recompui
    recompinput
    librecomp
    ultramodern
    rt64
    nfd
    SDL2-static
    android
    log
    ${CMAKE_DL_LIBS}
)

target_link_options(main PRIVATE "-Wl,-z,max-page-size=16384")
