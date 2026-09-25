# Mario Party 1 recomp integration

The Android shell is designed around N64Recomp + N64ModernRuntime.

## Source metadata

Upstream decompilation:
- repository: mariopartyrd/marioparty
- target: Mario Party 1 US
- required ROM filename upstream: baserom.us.z64
- ELF produced by the decomp: build/marioparty.elf

The playable native runtime cannot be generated from the public source tree alone. The decomp build requires a clean user-supplied US big-endian Mario Party 1 ROM to extract/build the target ELF.

## Android integration sequence

1. Build the upstream decomp with the clean US ROM.
2. Feed its ELF and ROM metadata to N64Recomp.
3. Compile the generated C output into the Android `mp1runtime` library.
4. Implement N64ModernRuntime platform hooks for graphics, audio, input, storage and timing.
5. Connect `VirtualPadView` input state to controller port 1.
6. Add physical Android controllers and player-slot assignment.
7. Add deterministic frame/input synchronization for netplay.
8. Add per-frame state hashing and desync diagnostics.

ROMs and Nintendo assets must not be committed to this repository.
