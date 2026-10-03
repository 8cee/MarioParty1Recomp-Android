#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SDL_VERSION="2.30.8"
SDL_JAVA_DIR="$ROOT/app/src/main/java/org/libsdl/app"
HOST_TOOLS_DIR="$ROOT/.mp1-build/host-tools"
HOST_FILE_TO_C="$HOST_TOOLS_DIR/file_to_c"

if [[ ! -f "$SDL_JAVA_DIR/SDLActivity.java" ]]; then
  echo "[android] staging SDL Java activity sources"
  TMP="$(mktemp -d)"
  trap 'rm -rf "$TMP"' EXIT
  curl -fsSL "https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL2-${SDL_VERSION}.tar.gz" -o "$TMP/sdl.tar.gz"
  tar -xzf "$TMP/sdl.tar.gz" -C "$TMP"
  mkdir -p "$SDL_JAVA_DIR"
  cp "$TMP/SDL2-${SDL_VERSION}/android-project/app/src/main/java/org/libsdl/app/"*.java "$SDL_JAVA_DIR/"
fi

# RT64 shader embedding runs file_to_c on the x86_64 build host while the
# rest of CMake targets Android/arm64. Build the tiny host utility explicitly.
FILE_TO_C_SRC="$ROOT/lib/rt64/src/tools/file_to_c/file_to_c.cpp"
[[ -f "$FILE_TO_C_SRC" ]] || { echo "[android] missing RT64 file_to_c source"; exit 2; }

mkdir -p "$HOST_TOOLS_DIR"
if [[ ! -x "$HOST_FILE_TO_C" || "$FILE_TO_C_SRC" -nt "$HOST_FILE_TO_C" ]]; then
  echo "[android] building host RT64 file_to_c"
  "${CXX:-c++}" -std=c++17 -O2 "$FILE_TO_C_SRC" -o "$HOST_FILE_TO_C"
fi

BUILD_VARIANT="${MP1_ANDROID_VARIANT:-release}"
if [[ "$BUILD_VARIANT" == "debug" ]]; then
  GRADLE_TASK=":app:assembleDebug"
  APK="$ROOT/app/build/outputs/apk/debug/app-debug.apk"
else
  GRADLE_TASK=":app:assembleRelease"
  APK="$ROOT/app/build/outputs/apk/release/app-release.apk"
fi

echo "[android] assembling Mario Party native arm64 $BUILD_VARIANT APK"
gradle --no-daemon -PHOST_FILE_TO_C="$HOST_FILE_TO_C" "$GRADLE_TASK"
test -f "$APK"
unzip -l "$APK" > "$ROOT/.mp1-build/apk-contents.txt"
grep -q 'lib/arm64-v8a/libmain.so' "$ROOT/.mp1-build/apk-contents.txt"
if grep -Eqi '\.(z64|n64|v64)( |$)|baserom|decompressed' "$ROOT/.mp1-build/apk-contents.txt"; then
  echo "[android] ROM-derived input unexpectedly packaged in APK"
  exit 3
fi
sha256sum "$APK"
echo "[android] built $APK"
