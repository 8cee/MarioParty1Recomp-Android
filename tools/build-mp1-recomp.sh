#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ROM="${1:-}"
EXPECTED="1159bd56730094bfc71be30113e1cfc8bacf34f3"
DECOMP_REPO="${MP1_DECOMP_REPO:-https://github.com/mariopartyrd/marioparty.git}"
DECOMP_REF="${MP1_DECOMP_REF:-main}"
[[ -f "$ROM" ]] || { echo "usage: $0 /path/to/MarioParty-USA.z64"; exit 2; }
[[ "$(sha1sum "$ROM" | awk '{print $1}')" == "$EXPECTED" ]] || { echo "Unsupported ROM"; exit 3; }
mkdir -p "$ROOT/.mp1-build"
if [[ ! -d "$ROOT/.mp1-build/marioparty/.git" ]]; then git clone "$DECOMP_REPO" "$ROOT/.mp1-build/marioparty"; fi
git -C "$ROOT/.mp1-build/marioparty" fetch origin "$DECOMP_REF"
git -C "$ROOT/.mp1-build/marioparty" checkout --detach FETCH_HEAD
cp "$ROM" "$ROOT/.mp1-build/marioparty/baserom.us.z64"
cd "$ROOT/.mp1-build/marioparty"
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
python3 -m splat split marioparty.yaml
python3 tools/configure.py
make -j"$(nproc)"
test -f build/marioparty.elf
cd "$ROOT"
if [[ ! -d .mp1-build/N64Recomp/.git ]]; then git clone https://github.com/N64Recomp/N64Recomp.git .mp1-build/N64Recomp; fi
python3 recomp/patch-n64recomp-mp1.py\ncmake -S .mp1-build/N64Recomp -B .mp1-build/N64Recomp/build -DCMAKE_BUILD_TYPE=Release
cmake --build .mp1-build/N64Recomp/build -j"$(nproc)"
rm -rf recomp/generated recomp/rsp
mkdir -p recomp/generated recomp/rsp
(cd recomp && ../.mp1-build/N64Recomp/build/N64Recomp mp1.us.toml)
(cd recomp && ../.mp1-build/N64Recomp/build/RSPRecomp aspMain.toml)
test -n "$(find recomp/generated -name 'funcs_*.c' -print -quit)"
test -s recomp/rsp/aspMain.cpp
echo "Mario Party recomp output generated."
