# MarioParty1Recomp-Android

Android-focused Mario Party 1 recompilation/port project.

## Goals
- ARM64 Android build
- User-supplied legally owned Mario Party 1 ROM
- Touch controls and physical controllers
- Local 1-4 player support
- Online multiplayer architecture
- Built-in diagnostics and crash logging

## Current status
Initial Android project skeleton and diagnostics foundation.

## ROM policy
This repository does not include Nintendo game data, ROMs, or proprietary assets.


## Supported ROM

The Android port currently targets the exact clean **Mario Party (USA)** big-endian `.z64` dump:

- size: 33,554,432 bytes
- game code: `CLBE`
- SHA-1: `1159bd56730094bfc71be30113e1cfc8bacf34f3`

The ROM itself is never stored in this repository.

## Recomp build helper

`tools/build-mp1-recomp.sh` validates the ROM, clones/builds the upstream Mario Party decomp, and builds N64Recomp in a private local working directory. It never uploads the ROM.
