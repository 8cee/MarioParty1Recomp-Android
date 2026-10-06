#!/usr/bin/env python3
"""Enumerate Mario Party 1's main filesystem from a verified US ROM.

This is a structural validator for the future O2R extractor. It does not emit
copyrighted assets; it emits metadata only.
"""
from __future__ import annotations
import argparse, hashlib, json, struct
from pathlib import Path

EXPECTED_SHA1 = "1159bd56730094bfc71be30113e1cfc8bacf34f3"\nMAINFS_START = 0x31C7E0\nMAINFS_END = 0xFCB860

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
    # Pinned for the supported NTSC-U ROM. The decomp/MP1 tooling identify the
    # main filesystem as 0x31C7E0..0xFCB860.
    if len(data) < MAINFS_END:
        raise ValueError("ROM is too small for the NTSC-U main filesystem")
    count, offsets = table(data, MAINFS_START)
    if not (1 <= count <= 512):
        raise ValueError(f"invalid main-FS directory count {count}")
    if offsets != sorted(offsets):
        raise ValueError("main-FS directory offsets are not monotonic")
    if any(o < 4 + count * 4 or MAINFS_START + o >= MAINFS_END for o in offsets):
        raise ValueError("main-FS directory offset outside pinned range")
    return MAINFS_START

def manifest(data: bytes, base: int) -> dict:
    dir_count, dir_offsets = table(data, base)
    entries = []
    for d, doff in enumerate(dir_offsets):
        dbase = base + doff
        if not (MAINFS_START <= dbase < MAINFS_END):
            raise ValueError(f"directory {d} outside main-FS range")
        file_count, file_offsets = table(data, dbase)
        if file_offsets != sorted(file_offsets):
            raise ValueError(f"directory {d} file offsets are not monotonic")
        for f, foff in enumerate(file_offsets):
            h = dbase + foff
            if foff < 4 + file_count * 4 or h + 8 > MAINFS_END:
                raise ValueError(f"file {d:04X}/{f:04X} header outside main-FS range")
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
    return {"mainfs_rom_offset": base, "mainfs_rom_end": MAINFS_END, "directory_count": dir_count, "file_count": len(entries), "files": entries}

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
