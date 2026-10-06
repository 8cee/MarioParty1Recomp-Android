#!/usr/bin/env python3
"""Export Mario Party 1 board-space data from a verified NTSC-U ROM.

The original engine loads board topology through LoadBoardSpaces(0x0A, file).
DK's Jungle Adventure uses file 0x45. Output is metadata/topology only.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
from pathlib import Path

EXPECTED_SHA1 = "1159bd56730094bfc71be30113e1cfc8bacf34f3"
MAINFS_START = 0x31C7E0
MAINFS_END = 0xFCB860

def be16(data: bytes, off: int) -> int:
    return struct.unpack_from(">H", data, off)[0]

def bes16(data: bytes, off: int) -> int:
    return struct.unpack_from(">h", data, off)[0]

def be32(data: bytes, off: int) -> int:
    return struct.unpack_from(">I", data, off)[0]

def bef32(data: bytes, off: int) -> float:
    return struct.unpack_from(">f", data, off)[0]

def decode_type1(src: bytes, expected_size: int) -> bytes:
    ring = bytearray(0x400)
    ring_pos = 0
    pos = 0
    out = bytearray()
    while len(out) < expected_size:
        if pos >= len(src):
            raise ValueError("truncated type-1 control stream")
        control = src[pos]
        pos += 1
        for _ in range(8):
            if len(out) >= expected_size:
                break
            if control & 1:
                if pos >= len(src):
                    raise ValueError("truncated type-1 literal")
                value = src[pos]
                pos += 1
                out.append(value)
                ring[ring_pos] = value
                ring_pos = (ring_pos + 1) & 0x3FF
            else:
                if pos + 2 > len(src):
                    raise ValueError("truncated type-1 backreference")
                a, b = src[pos], src[pos + 1]
                pos += 2
                read_pos = (((b & 0xC0) << 2) | a) & 0x3FF
                length = (b & 0x3F) + 3
                for n in range(length):
                    if len(out) >= expected_size:
                        break
                    value = ring[(read_pos + n + 66) & 0x3FF]
                    out.append(value)
                    ring[ring_pos] = value
                    ring_pos = (ring_pos + 1) & 0x3FF
            control >>= 1
    return bytes(out)

def table(data: bytes, base: int) -> list[int]:
    count = be32(data, base)
    return [be32(data, base + 4 + i * 4) for i in range(count)]

def read_mainfs_file(rom: bytes, directory: int, file_index: int) -> bytes:
    dirs = table(rom, MAINFS_START)
    if directory >= len(dirs):
        raise ValueError(f"directory {directory:#x} out of range")
    dbase = MAINFS_START + dirs[directory]
    files = table(rom, dbase)
    if file_index >= len(files):
        raise ValueError(f"file {file_index:#x} out of range in directory {directory:#x}")
    header = dbase + files[file_index]
    decoded_size = be32(rom, header)
    compression = be32(rom, header + 4)
    payload = header + 8
    if compression == 0:
        return rom[payload:payload + decoded_size]
    if compression == 1:
        return decode_type1(rom[payload:MAINFS_END], decoded_size)
    raise ValueError(f"unsupported compression type {compression}")

def parse_chain(blob: bytes, table_base: int, index: int) -> list[int]:
    rel = be16(blob, table_base + index * 2)
    pos = table_base + rel
    length = be16(blob, pos)
    pos += 2
    return [bes16(blob, pos + i * 2) for i in range(length)]

def parse_board(blob: bytes, board_file: int) -> dict:
    if len(blob) < 12:
        raise ValueError("board file too small")

    space_count = be16(blob, 0)
    chain_count_a = be16(blob, 2)
    chain_count_b = be16(blob, 4)
    spaces_off = be16(blob, 6)
    chains_a_off = be16(blob, 8)
    chains_b_off = be16(blob, 10)

    spaces = []
    pos = spaces_off
    for i in range(space_count):
        if pos + 16 > len(blob):
            raise ValueError(f"space {i} exceeds board file")
        unk2 = be16(blob, pos)
        space_type = be16(blob, pos + 2)
        x = bef32(blob, pos + 4) * 5.0
        y = bef32(blob, pos + 8) * 5.0
        z = bef32(blob, pos + 12) * 5.0
        spaces.append({
            "index": i,
            "unk2": unk2,
            "type": space_type,
            "position_n64": [x, y, z],
            # UE uses Z-up. Preserve scale but map N64 X,Y,Z -> UE X,Z,Y.
            "position_ue": [x, z, y],
        })
        pos += 16

    chains_a = [parse_chain(blob, chains_a_off, i) for i in range(chain_count_a)]
    chains_b = [parse_chain(blob, chains_b_off, i) for i in range(chain_count_b)]

    return {
        "schema": "mp1-ue5-board-v1",
        "mainfs_directory": 0x0A,
        "board_file": board_file,
        "space_count": space_count,
        "chain_count_a": chain_count_a,
        "chain_count_b": chain_count_b,
        "spaces": spaces,
        "chains_a": chains_a,
        "chains_b": chains_b,
    }

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("rom", type=Path)
    ap.add_argument("-o", "--output", type=Path, required=True)
    ap.add_argument("--board-file", type=lambda x: int(x, 0), default=0x45,
                    help="MainFS directory 0x0A file index (default: 0x45 DK board)")
    args = ap.parse_args()

    rom = args.rom.read_bytes()
    sha1 = hashlib.sha1(rom).hexdigest()
    if sha1 != EXPECTED_SHA1:
        raise SystemExit(f"unsupported ROM SHA-1 {sha1}; expected {EXPECTED_SHA1}")

    blob = read_mainfs_file(rom, 0x0A, args.board_file)
    board = parse_board(blob, args.board_file)
    board["source_rom_sha1"] = sha1
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(board, indent=2) + "\n", encoding="utf-8")

if __name__ == "__main__":
    main()
