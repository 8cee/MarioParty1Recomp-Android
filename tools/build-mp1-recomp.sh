#!/usr/bin/env bash
set -euo pipefail

ROM="${1:-baserom.us.z64}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="${MP1_BUILD_DIR:-$ROOT/.mp1-build}"
DECOMP_DIR="$WORK/marioparty"
RECOMP_DIR="$WORK/N64Recomp"

EXPECTED_SHA1="1159bd56730094bfc71be30113e1cfc8bacf34f3"

if [[ ! -f "$ROM" ]]; then
  echo "Missing ROM: $ROM" >&2
  echo "Usage: tools/build-mp1-recomp.sh /path/to/Mario\ Party\ \(USA\).z64" >&2
  exit 2
fi

ACTUAL_SHA1="$(sha1sum "$ROM" | awk '{print $1}')"
if [[ "$ACTUAL_SHA1" != "$EXPECTED_SHA1" ]]; then
  echo "Unsupported ROM checksum: $ACTUAL_SHA1" >&2
  echo "Expected: $EXPECTED_SHA1" >&2
  exit 3
fi

mkdir -p "$WORK"

if [[ ! -d "$DECOMP_DIR/.git" ]]; then
  git clone --depth 1 https://github.com/mariopartyrd/marioparty.git "$DECOMP_DIR"
fi

cp "$ROM" "$DECOMP_DIR/baserom.us.z64"

pushd "$DECOMP_DIR" >/dev/null
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
make setup
make -j"$(nproc)"
deactivate
popd >/dev/null

ELF="$DECOMP_DIR/build/marioparty.elf"
if [[ ! -f "$ELF" ]]; then
  echo "Decomp build did not produce $ELF" >&2
  exit 4
fi

if [[ ! -d "$RECOMP_DIR/.git" ]]; then
  git clone --recurse-submodules https://github.com/N64Recomp/N64Recomp.git "$RECOMP_DIR"
fi

cmake -S "$RECOMP_DIR" -B "$RECOMP_DIR/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$RECOMP_DIR/build" -j"$(nproc)"

echo
echo "Mario Party decomp ELF ready:"
echo "  $ELF"
echo
echo "Next step: run N64Recomp with the Mario Party-specific TOML metadata."
