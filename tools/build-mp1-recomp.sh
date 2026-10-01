#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ROM="${1:-}"
EXPECTED="1159bd56730094bfc71be30113e1cfc8bacf34f3"
DECOMP_REPO="${MP1_DECOMP_REPO:-https://github.com/mariopartyrd/marioparty.git}"
DECOMP_REF="${MP1_DECOMP_REF:-26dca4f3cf2dab1839bc1de3579b7227b65b9ee6}"
N64RECOMP_REF="${N64RECOMP_REF:-81213c1831fab2521a6a5459c67b63437d67e253}"
[[ -f "$ROM" ]] || { echo "usage: $0 /path/to/MarioParty-USA.z64"; exit 2; }
[[ "$(sha1sum "$ROM" | awk '{print $1}')" == "$EXPECTED" ]] || { echo "Unsupported ROM"; exit 3; }
mkdir -p "$ROOT/.mp1-build"
if [[ ! -d "$ROOT/.mp1-build/marioparty/.git" ]]; then
    git clone "$DECOMP_REPO" "$ROOT/.mp1-build/marioparty"
fi
git -C "$ROOT/.mp1-build/marioparty" fetch origin "$DECOMP_REF"
git -C "$ROOT/.mp1-build/marioparty" checkout --detach FETCH_HEAD
cp "$ROM" "$ROOT/.mp1-build/marioparty/baserom.us.z64"
cd "$ROOT/.mp1-build/marioparty"
rm -rf venv
python3 -m venv venv
. venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -r requirements.txt pyelftools
# Current mariopartyrd/marioparty build flow: setup performs the clean
# split/configure stage, then make produces the matching ROM + ELF.
make setup
make -j"$(nproc)"
test -f build/marioparty.elf

# Mario Party loads all gameplay/board/minigame overlays at the same VRAM base.
# N64Recomp needs those ELF output sections explicitly marked relocatable.
# Derive the list from the freshly built ELF so it always matches the pinned
# decomp instead of keeping a fragile hand-maintained section list.
python - "$ROOT/recomp/mp1.us.overlays.txt" <<'PY'
from pathlib import Path
import sys
from elftools.elf.elffile import ELFFile

elf_path = Path("build/marioparty.elf")
out_path = Path(sys.argv[1])
overlay_vram = 0x800F65E0

with elf_path.open("rb") as f:
    elf = ELFFile(f)
    names = [
        sec.name for sec in elf.iter_sections()
        if int(sec["sh_addr"]) == overlay_vram and int(sec["sh_size"]) > 0
    ]

if not names:
    raise SystemExit(
        f"No ELF sections found at Mario Party overlay VRAM 0x{overlay_vram:08X}"
    )

out_path.write_text("\n".join(names) + "\n")
print(f"Derived {len(names)} relocatable MP1 ELF sections -> {out_path}")
PY

cd "$ROOT"
if [[ ! -d .mp1-build/N64Recomp/.git ]]; then
    git clone --recurse-submodules https://github.com/N64Recomp/N64Recomp.git .mp1-build/N64Recomp
fi
git -C .mp1-build/N64Recomp fetch origin "$N64RECOMP_REF"
git -C .mp1-build/N64Recomp checkout --detach FETCH_HEAD
git -C .mp1-build/N64Recomp submodule update --init --recursive
python3 recomp/patch-n64recomp-mp1.py
python3 recomp/patch-n64recomp-runtime.py
cmake -S .mp1-build/N64Recomp -B .mp1-build/N64Recomp/build -DCMAKE_BUILD_TYPE=Release
cmake --build .mp1-build/N64Recomp/build --target N64RecompCLI RSPRecomp -j"$(nproc)"
rm -rf recomp/generated recomp/rsp
mkdir -p recomp/generated recomp/rsp
(cd recomp && ../.mp1-build/N64Recomp/build/N64Recomp mp1.us.toml)
python3 recomp/patch-generated-process-runtime.py
python3 recomp/derive-mp1-rsp-config.py
(cd recomp && ../.mp1-build/N64Recomp/build/RSPRecomp aspMain.toml)
test -n "$(find recomp/generated -name 'funcs_*.c' -print -quit)"
test -s recomp/rsp/aspMain.cpp
echo "Mario Party recomp output generated."
