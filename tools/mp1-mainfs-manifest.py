#!/usr/bin/env python3
"""Enumerate Mario Party 1's main filesystem from a verified US ROM.

This is a structural validator for the future O2R extractor. It does not emit
copyrighted assets; it emits metadata only.
"""
from __future__ import annotations
import argparse, hashlib, json, struct
from pathlib import Path

EXPECTED_SHA1 = "1159bd56730094bfc71be30113e1cfc8bacf34f3"

def be32(data: bytes, off: int) -> int:
    if off < 0 or off + 4 > len(data):
        raise ValueError(f"u32 outside ROM at 0x{off:X}")
    return struct.unpack_from(">I", data, off)[0]

def table(data: bytes, base: int) -> tuple[int, list[int]]:
    count = be32(data, base)
    if count > 0x10000:
        raise ValueError(f"implausible table count {count} at 0x{base:X}")
    offsets = [be32(data, base + 4 + i * 4) for i in range(count)]
    return count, offsets

def locate_mainfs(data: bytes) -> int:
    # DataInit receives the main-FS ROM address at runtime. Until that callsite
    # is promoted to a generated constant, locate the nested table structurally.
    # Candidate must contain sane directory offsets and sane first file headers.
    candidates = []
    for base in range(0, len(data) - 0x40, 4):
        count = be32(data, base)
        if not (1 <= count <= 512):
            continue
        end = base + 4 + count * 4
        if end > len(data):
            continue
        offs = [be32(data, base + 4 + i * 4) for i in range(min(count, 8))]
        if not offs or offs[0] < 4 + count * 4 or any(o >= len(data) - base for o in offs):
            continue
        if offs != sorted(offs):
            continue
        try:
            dbase = base + offs[0]
            files = be32(data, dbase)
            if not (1 <= files <= 4096):
                continue
            first = be32(data, dbase + 4)
            if first < 4 + files * 4 or dbase + first + 8 > len(data):
                continue
            size = be32(data, dbase + first)
            comp = be32(data, dbase + first + 4)
            if size == 0 or size > 0x2000000 or comp not in (0, 1):
                continue
            candidates.append(base)
        except (ValueError, struct.error):
            pass
    if len(candidates) != 1:
        raise RuntimeError(f"expected one main-FS candidate, found {len(candidates)}: {[hex(x) for x in candidates[:16]]}")
    return candidates[0]

def manifest(data: bytes, base: int) -> dict:
    dir_count, dir_offsets = table(data, base)
    entries = []
    for d, doff in enumerate(dir_offsets):
        dbase = base + doff
        file_count, file_offsets = table(data, dbase)
        for f, foff in enumerate(file_offsets):
            h = dbase + foff
            decoded_size = be32(data, h)
            compression = be32(data, h + 4)
            entries.append({
                "id": f"{d:04X}/{f:04X}",
                "directory": d,
                "file": f,
                "rom_offset": h + 8,
                "decoded_size": decoded_size,
                "compression_type": compression,
            })
    return {"mainfs_rom_offset": base, "directory_count": dir_count, "file_count": len(entries), "files": entries}

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("rom", type=Path)
    ap.add_argument("-o", "--output", type=Path)
    args = ap.parse_args()
    data = args.rom.read_bytes()
    sha1 = hashlib.sha1(data).hexdigest()
    if sha1 != EXPECTED_SHA1:
        raise SystemExit(f"unsupported ROM SHA-1 {sha1}; expected {EXPECTED_SHA1}")
    out = manifest(data, locate_mainfs(data))
    out["rom_sha1"] = sha1
    text = json.dumps(out, indent=2) + "\n"
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end="")

if __name__ == "__main__":
    main()
