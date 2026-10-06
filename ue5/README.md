# Mario Party 1 — Unreal Engine 5 migration

This directory starts the UE5 migration without discarding the existing recomp work.

## Architecture

- **N64 recomp CPU/RSP logic stays reusable.**
- **UE5 owns the application window, Android lifecycle, input, audio and final presentation.**
- RT64/SDL must not create a second Android window once the recomp bridge is active.
- `FMP1RecompBridge` is the integration boundary between UE5 and the existing runtime.

## Current phase

The UE5 native module and recomp bridge are present. The bridge validates the selected ROM and provides explicit initialize/tick/shutdown lifecycle points.

The next phase is to link the existing generated Mario Party recomp sources and N64ModernRuntime into the UE5 module, then replace RT64's window/present path with a UE5 RHI texture upload/presentation path.

No copyrighted ROM data or game assets are stored in this project.
