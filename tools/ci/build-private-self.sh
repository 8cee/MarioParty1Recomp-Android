#!/usr/bin/env bash
set -euo pipefail

ROOT="${GITHUB_WORKSPACE:?GITHUB_WORKSPACE is required}"
PRIVATE="$ROOT/.private-inputs"
SRC="$ROOT"
OUT="$ROOT/MarioParty1Recomp-release.apk"
EXPECTED_SHA1="1159bd56730094bfc71be30113e1cfc8bacf34f3"

echo "==> Preparing private Mario Party 1 Android build"
echo "Building directly from the checked-out private MP1 source repository."
echo "MP1 private pipeline revision: self-hosted-private-source"
HAVE_GENERATED=0
if [ -n "$(find "$ROOT/recomp/generated" -maxdepth 1 -type f -name 'funcs_*.c' -print -quit 2>/dev/null)" ] && [ -s "$ROOT/recomp/rsp/aspMain.cpp" ]; then
  HAVE_GENERATED=1
  echo "Using restored verified MP1 generated sources"
fi

ROM_INPUT=""
if [ "$HAVE_GENERATED" -eq 0 ]; then
  ROM_INPUT="$(find "$PRIVATE/mp1" -maxdepth 1 -type f \( -iname '*.n64' -o -iname '*.z64' -o -iname '*.v64' \) | head -1 || true)"

  if [ -z "$ROM_INPUT" ] && compgen -G "$PRIVATE/mp1/marioparty.z64.part*" > /dev/null; then
    mkdir -p "$ROOT/reconstructed-rom"
    ROM_INPUT="$ROOT/reconstructed-rom/marioparty.us.z64"
    cat "$PRIVATE"/mp1/marioparty.z64.part* > "$ROM_INPUT"
    echo "Reconstructed private ROM from split parts"
  fi

  if [ -z "$ROM_INPUT" ] || [ ! -f "$ROM_INPUT" ]; then
    echo "::error::No generated MP1 sources and no private Mario Party ROM input found"
    exit 1
  fi

  echo "Private ROM input: $(basename "$ROM_INPUT")"
  ACTUAL_SHA1="$(sha1sum "$ROM_INPUT" | awk '{print $1}')"
  if [ "$ACTUAL_SHA1" != "$EXPECTED_SHA1" ]; then
    echo "::error::Unsupported Mario Party ROM SHA1: $ACTUAL_SHA1"
    echo "::error::Expected: $EXPECTED_SHA1"
    exit 2
  fi
fi

cd "$SRC"

echo "==> Installing host build tools"
sudo apt-get update
sudo apt-get install -y ninja-build clang lld llvm xz-utils cmake make unzip zip rsync qemu-user python3-venv binutils-mips-linux-gnu gcc-multilib

SDK_ROOT="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
if [ -z "$SDK_ROOT" ]; then
  for candidate in /usr/local/lib/android/sdk /opt/android-sdk "$HOME/Android/Sdk"; do
    if [ -d "$candidate" ]; then
      SDK_ROOT="$candidate"
      break
    fi
  done
fi
SDKMANAGER="$(find "$SDK_ROOT/cmdline-tools" -type f -name sdkmanager 2>/dev/null | sort | tail -1 || true)"
test -x "$SDKMANAGER"
export ANDROID_HOME="$SDK_ROOT"
export ANDROID_SDK_ROOT="$SDK_ROOT"
yes | "$SDKMANAGER" --sdk_root="$SDK_ROOT" --licenses >/dev/null 2>&1 || true
"$SDKMANAGER" --sdk_root="$SDK_ROOT" "platforms;android-35" "build-tools;35.0.0" "ndk;27.2.12479018" "cmake;3.22.1"

if [ "$HAVE_GENERATED" -eq 0 ]; then
  echo "==> Building Mario Party decomp and generated CPU code"
  bash tools/build-mp1-recomp.sh "$ROM_INPUT"
else
  echo "==> Skipping ROM codegen; restored generated CPU/RSP code is present"
fi
test -f recomp/generated/funcs.h
test "$(find recomp/generated -maxdepth 1 -type f -name 'funcs_*.c' | wc -l)" -gt 0
test -f recomp/rsp/aspMain.cpp

echo "==> Verifying generated payload"
COUNT="$(find recomp/generated -maxdepth 1 -type f -name 'funcs_*.c' | wc -l)"
echo "Generated source files: $COUNT"
grep -R -q 'recomp_entrypoint' recomp/generated
grep -q 'aspMain' recomp/rsp/aspMain.cpp

# Preserve ROM-derived generated code privately so later Android-only fixes can
# rebuild without repeating the expensive decomp/recomp generation stage.
tar -czf "$ROOT/MarioParty1Recomp-generated-sources.tar.gz" recomp/generated recomp/rsp/aspMain.cpp

echo "==> Building RT64 host file_to_c"
cmake -DCMAKE_BUILD_TYPE=Release -G Ninja -S lib/rt64/src/tools/file_to_c -B build-filetoc
cmake --build build-filetoc -j 2
test -x build-filetoc/file_to_c

echo "==> Applying Android runtime source adjustments"
python3 - <<'PY'
from pathlib import Path

rt64 = Path("lib/rt64/CMakeLists.txt")
text = rt64.read_text()
repls = [
    (
        'if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")',
        'if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")'
    ),
    (
        'add_subdirectory(src/tools/file_to_c)',
        '''if (NOT ANDROID)
    add_subdirectory(src/tools/file_to_c)
else()
    if (FILE_TO_C_EXE)
        set(RT64_HOST_FILE_TO_C "${FILE_TO_C_EXE}")
    else()
        find_program(RT64_HOST_FILE_TO_C NAMES file_to_c)
    endif()
    if (NOT RT64_HOST_FILE_TO_C)
        message(FATAL_ERROR "Android RT64 build requires host FILE_TO_C_EXE")
    endif()
    if (NOT TARGET file_to_c)
        add_executable(file_to_c IMPORTED GLOBAL)
        set_target_properties(file_to_c PROPERTIES IMPORTED_LOCATION "${RT64_HOST_FILE_TO_C}")
    endif()
endif()'''
    ),
    (
        'add_subdirectory(src/contrib/nativefiledialog-extended)',
        '''if (NOT ANDROID)
    add_subdirectory(src/contrib/nativefiledialog-extended)
endif()'''
    ),
    (
        'add_subdirectory(src/tools/texture_hasher)\nadd_subdirectory(src/tools/texture_packer)',
        '''if (NOT ANDROID)
    add_subdirectory(src/tools/texture_hasher)
    add_subdirectory(src/tools/texture_packer)
endif()'''
    ),
]
for old, new in repls:
    if old not in text:
        raise SystemExit(f"RT64 source adjustment anchor missing: {old}")
    text = text.replace(old, new, 1)

# There are separate Apple and generic Unix DXC selection blocks. The first
# replacement above handles one occurrence; make every remaining x86_64 host
# tool decision use the build host rather than Android's arm64 target.
text = text.replace(
    'if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")',
    'if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")'
)
# RT64's pinned CMake still searches for a host-installed SDL2 package on
# every non-Windows target. Android already has the SDL2::SDL2 target created
# by the app's FetchContent block, so reuse that instead.
old = '''else()
    find_package(SDL2 REQUIRED)
endif()'''
new = '''elseif(ANDROID)
    if (NOT TARGET SDL2::SDL2)
        message(FATAL_ERROR "Android RT64 expects the parent project to provide SDL2::SDL2")
    endif()
    set(SDL2_LIBRARIES SDL2::SDL2)
else()
    find_package(SDL2 REQUIRED)
endif()'''
if old not in text:
    raise SystemExit("RT64 SDL2 package lookup anchor missing")
text = text.replace(old, new, 1)

old_cond = 'if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND RT64_SDL_WINDOW_VULKAN)'
new_cond = 'if ((CMAKE_SYSTEM_NAME MATCHES "Linux" OR ANDROID) AND RT64_SDL_WINDOW_VULKAN)'
count = text.count(old_cond)
if count != 2:
    raise SystemExit(f"Expected 2 RT64 SDL/Vulkan platform conditions, found {count}")
text = text.replace(old_cond, new_cond)
rt64.write_text(text)

window_cpp = Path("lib/rt64/src/hle/rt64_application_window.cpp")
window_text = window_cpp.read_text()
old = '#   elif defined(__ANDROID__)\n        static_assert(false && "Android unimplemented");'
new = '''#   elif defined(__ANDROID__)
        if (SDL_VideoInit(nullptr) != 0) {
            printf("Failed to init SDL2 video: %s\\n", SDL_GetError());
            assert(false && "Failed to init SDL2 video");
            return;
        }
        SDL_DisplayMode dm;
        if (SDL_GetDesktopDisplayMode(0, &dm) != 0) {
            printf("Failed to get SDL2 desktop display mode: %s\\n", SDL_GetError());
            assert(false && "Failed to get SDL2 desktop display mode");
            return;
        }
        bounds.left = (dm.w - Width) / 2;
        bounds.top = (dm.h - Height) / 2;
        bounds.width = Width;
        bounds.height = Height;'''
if old not in window_text:
    raise SystemExit("RT64 Android ApplicationWindow anchor missing")
window_text = window_text.replace(old, new, 1)

# Match the proven DK64 Android SDL/Vulkan handle path. RT64 has several
# desktop-oriented call sites that use RenderWindow directly; on Android the
# SDL Vulkan build must consistently treat that handle as SDL_Window*.
old = '''        windowHandle = window;

#   if defined(RT64_SDL_WINDOW_VULKAN)
        sdlWindow = window;
#   endif'''
new = '''        windowHandle = window;

#   if defined(__ANDROID__) && defined(RT64_SDL_WINDOW_VULKAN)
        sdlWindow = (SDL_Window *)window;
#   elif defined(RT64_SDL_WINDOW_VULKAN)
        sdlWindow = window;
#   endif'''
if old not in window_text:
    raise SystemExit("RT64 Android SDL constructor handle anchor missing")
window_text = window_text.replace(old, new, 1)

old = '''#   if defined(_WIN32)
        windowHandle = wmInfo.info.win.window;
#   elif defined(RT64_SDL_WINDOW_VULKAN)
        windowHandle = sdlWindow;
#   elif defined(__ANDROID__)
        static_assert(false && "Android unimplemented");'''
new = '''#   if defined(_WIN32)
        windowHandle = wmInfo.info.win.window;
#   elif defined(__ANDROID__) && defined(RT64_SDL_WINDOW_VULKAN)
        windowHandle = (RenderWindow)sdlWindow;
#   elif defined(RT64_SDL_WINDOW_VULKAN)
        windowHandle = sdlWindow;
#   elif defined(__ANDROID__)
        static_assert(false && "Android requires RT64_SDL_WINDOW_VULKAN");'''
if old not in window_text:
    raise SystemExit("RT64 Android SDL native handle anchor missing")
window_text = window_text.replace(old, new, 1)

window_text = window_text.replace(
    'SDL_SetWindowFullscreen(windowHandle, SDL_WINDOW_FULLSCREEN_DESKTOP);',
    'SDL_SetWindowFullscreen((SDL_Window *)windowHandle, SDL_WINDOW_FULLSCREEN_DESKTOP);'
)
window_text = window_text.replace(
    'SDL_SetWindowFullscreen(windowHandle, 0);',
    'SDL_SetWindowFullscreen((SDL_Window *)windowHandle, 0);'
)
window_text = window_text.replace(
    'int displayIndex = SDL_GetWindowDisplayIndex(windowHandle);',
    'int displayIndex = SDL_GetWindowDisplayIndex((SDL_Window *)windowHandle);'
)
window_text = window_text.replace(
    'SDL_GetWindowPosition(windowHandle, &newWindowLeft, &newWindowTop);',
    'SDL_GetWindowPosition((SDL_Window *)windowHandle, &newWindowLeft, &newWindowTop);'
)
# Assert that every initialization-relevant DK64 Android window conversion was
# actually applied. MP1 and DK64 pin identical RT64/RecompFrontend/NMR commits,
# so a missed anchor here means the Android runtime patch is incomplete.
for forbidden in (
    'SDL_SetWindowFullscreen(windowHandle, SDL_WINDOW_FULLSCREEN_DESKTOP)',
    'SDL_SetWindowFullscreen(windowHandle, 0)',
    'SDL_GetWindowDisplayIndex(windowHandle)',
    'SDL_GetWindowPosition(windowHandle, &newWindowLeft, &newWindowTop)',
):
    if forbidden in window_text:
        raise SystemExit(f"Unpatched RT64 Android SDL window call remains: {forbidden}")
if '#   elif defined(__ANDROID__)\\n        static_assert(false && "Android unimplemented");' in window_text:
    raise SystemExit("RT64 Android native window path is still unimplemented")
# Add narrow Android breadcrumbs around RT64's window wrapper construction.
# These go to logcat even if the renderer never returns to mp1_main.cpp.
if '#include <android/log.h>' not in window_text:
    window_text = window_text.replace(
        '#include "rt64_application_window.h"',
        '#include "rt64_application_window.h"\n#if defined(__ANDROID__)\n#include <android/log.h>\n#endif',
        1
    )
ctor_anchor = '        windowHandle = window;'
if ctor_anchor not in window_text:
    raise SystemExit("RT64 diagnostic constructor anchor missing")
window_text = window_text.replace(
    ctor_anchor,
    '#if defined(__ANDROID__)\n        __android_log_print(ANDROID_LOG_INFO, "MP1RT64", "ApplicationWindow ctor begin");\n#endif\n' + ctor_anchor,
    1
)
bounds_anchor = '        bounds.width = Width;\n        bounds.height = Height;'
if bounds_anchor not in window_text:
    raise SystemExit("RT64 diagnostic bounds anchor missing")
window_text = window_text.replace(
    bounds_anchor,
    bounds_anchor + '\n#if defined(__ANDROID__)\n        __android_log_print(ANDROID_LOG_INFO, "MP1RT64", "ApplicationWindow Android bounds ready %dx%d", bounds.width, bounds.height);\n#endif',
    1
)
window_cpp.write_text(window_text)

types = Path("lib/rt64/src/contrib/plume/plume_render_interface_types.h")
types_text = types.read_text()
old = '''#elif defined(__ANDROID__)
    typedef ANativeWindow* RenderWindow;
#elif defined(PLUME_SDL_VULKAN_ENABLED)
    typedef SDL_Window *RenderWindow;'''
new = '''#elif defined(PLUME_SDL_VULKAN_ENABLED)
    typedef SDL_Window *RenderWindow;
#elif defined(__ANDROID__)
    typedef ANativeWindow* RenderWindow;'''
if old not in types_text:
    raise SystemExit("Plume RenderWindow Android/SDL typedef anchor missing")
types.write_text(types_text.replace(old, new, 1))

slotmap = Path("lib/RecompFrontend/lib/SlotMap/slot_map.h")
slot_text = slotmap.read_text()
old = '''#else
// Posix
#include <stdlib.h>
#define SLOT_MAP_ALLOC(sizeInBytes, alignment) aligned_alloc(alignment, sizeInBytes)
#define SLOT_MAP_FREE(ptr) free(ptr)
#endif'''
new = '''#else
// Posix
#include <stdlib.h>
#if defined(__ANDROID__) && (defined(__ANDROID_API__) && __ANDROID_API__ < 28)
#include <malloc.h>
#define SLOT_MAP_ALLOC(sizeInBytes, alignment) memalign((alignment), (sizeInBytes))
#else
#define SLOT_MAP_ALLOC(sizeInBytes, alignment) aligned_alloc(alignment, sizeInBytes)
#endif
#define SLOT_MAP_FREE(ptr) free(ptr)
#endif'''
if old not in slot_text:
    raise SystemExit("SlotMap Android allocation anchor missing")
slotmap.write_text(slot_text.replace(old, new, 1))

plume = Path("lib/rt64/src/contrib/plume/CMakeLists.txt")
text = plume.read_text()
old = 'cmake_dependent_option(PLUME_SDL_VULKAN_ENABLED "Enable SDL Vulkan integration" OFF IS_LINUX OFF)'
new = 'cmake_dependent_option(PLUME_SDL_VULKAN_ENABLED "Enable SDL Vulkan integration" OFF "IS_LINUX OR ANDROID" OFF)'
if old not in text:
    raise SystemExit("Plume Android Vulkan option anchor missing")
plume.write_text(text.replace(old, new, 1))

zstd = Path("lib/rt64/src/contrib/zstd/lib/dictBuilder/cover.c")
text = zstd.read_text()
pairs = [
    (
        '#if !defined(_GNU_SOURCE) && !defined(__APPLE__) && !defined(_MSC_VER)',
        '#if (!defined(_GNU_SOURCE) || defined(__ANDROID__)) && !defined(__APPLE__) && !defined(_MSC_VER)'
    ),
    (
        '#elif defined(_GNU_SOURCE)',
        '#elif defined(_GNU_SOURCE) && !defined(__ANDROID__)'
    ),
]
for old, new in pairs:
    if old not in text:
        raise SystemExit(f"Zstd Android anchor missing: {old}")
    text = text.replace(old, new)
zstd.write_text(text)

# Android is Vulkan-only here. Avoid RT64's automatic desktop API probe
# and record whether the stall is in Application construction or setup().
frontend = Path("lib/RecompFrontend/recompui/src/renderer/rt64_render_context.cpp")
front = frontend.read_text()
front_anchor = '#include <memory>'
if front_anchor not in front:
    raise SystemExit("RecompFrontend RT64 include anchor missing")
front = front.replace(front_anchor, '#if defined(__ANDROID__)\n#include <android/log.h>\n#endif\n' + front_anchor, 1)
for before, after in (
    ('    app = std::make_unique<RT64::Application>(appCore, appConfig);',
     '#if defined(__ANDROID__)\n    __android_log_print(ANDROID_LOG_INFO, "MP1RT64", "construct Application begin");\n#endif\n    app = std::make_unique<RT64::Application>(appCore, appConfig);\n#if defined(__ANDROID__)\n    __android_log_print(ANDROID_LOG_INFO, "MP1RT64", "construct Application done");\n#endif'),
    ('    setup_result = map_setup_result(app->setup(thread_id));',
     '#if defined(__ANDROID__)\n    app->userConfig.graphicsAPI = RT64::UserConfiguration::GraphicsAPI::Vulkan;\n    __android_log_print(ANDROID_LOG_INFO, "MP1RT64", "Application setup begin (Vulkan)");\n#endif\n    setup_result = map_setup_result(app->setup(thread_id));\n#if defined(__ANDROID__)\n    __android_log_print(ANDROID_LOG_INFO, "MP1RT64", "Application setup returned %d", int(setup_result));\n#endif'),
):
    if before not in front:
        raise SystemExit("RecompFrontend RT64 setup anchor missing")
    front = front.replace(before, after, 1)
frontend.write_text(front)

print("Applied deterministic RT64/Plume/Zstd Android adjustments")
PY

echo "==> Building optimized Android release APK"
# Use the project's canonical Android entrypoint. Besides Gradle/CMake this
# stages SDLActivity.java and the rest of SDL's Android Java glue before the
# Kotlin compiler runs.
bash tools/android/build_android.sh

APK="$SRC/app/build/outputs/apk/release/app-release.apk"
test -f "$APK"

echo "==> Verifying APK"
unzip -l "$APK" | grep 'lib/arm64-v8a/libmain.so'
cp "$APK" "$OUT"
sha256sum "$OUT"
echo "==> APK ready: $OUT"
