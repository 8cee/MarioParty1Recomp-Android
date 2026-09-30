# Mario Party recomp output

The files under generated/ and rsp/ are generated locally from a legally obtained supported Mario Party (USA) ROM and are intentionally excluded from this public repository.

Run:

    tools/build-mp1-recomp.sh /path/to/marioparty.us.z64

The script validates SHA-1 before decompilation/code generation. The Android build deliberately fails when the generated game code is absent, preventing a launcher-only APK from being mistaken for a playable build.
