#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SDL_VERSION="2.30.8"
SDL_JAVA_DIR="$ROOT/app/src/main/java/org/libsdl/app"
if [[ ! -f "$SDL_JAVA_DIR/SDLActivity.java" ]]; then
  echo "[android] staging SDL Java activity sources"
  TMP="$(mktemp -d)"
  trap 'rm -rf "$TMP"' EXIT
  curl -fsSL "https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL2-${SDL_VERSION}.tar.gz" -o "$TMP/sdl.tar.gz"
  tar -xzf "$TMP/sdl.tar.gz" -C "$TMP"
  mkdir -p "$SDL_JAVA_DIR"
  cp "$TMP/SDL2-${SDL_VERSION}/android-project/app/src/main/java/org/libsdl/app/"*.java "$SDL_JAVA_DIR/"
fi
echo "[android] assembling Mario Party native arm64 debug APK"
gradle --no-daemon :app:assembleDebug
APK="$ROOT/app/build/outputs/apk/debug/app-debug.apk"
test -f "$APK"
unzip -l "$APK" | grep -q 'lib/arm64-v8a/libmain.so'
echo "[android] built $APK"
