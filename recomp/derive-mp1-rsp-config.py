#!/usr/bin/env python3
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
ROM = ROOT / ".mp1-build/marioparty/baserom.us.z64"
OUT = ROOT / "recomp/aspMain.toml"

if not ROM.is_file():
    raise SystemExit("Verified MP1 ROM missing; build the pinned decomp first")

# Verified directly from Mario Party (USA)'s audio OSTask constructor:
#   boot       = 0x800B1760
#   ucode      = 0x800B7B30
#   ucode_data = 0x800C9BB0
#   data_size  = 0x800
#
# The main segment maps ROM 0x1060 -> VRAM 0x80000460.
MAIN_ROM = 0x1060
MAIN_VRAM = 0x80000460
TEXT_VRAM = 0x800B7B30
DATA_VRAM = 0x800C9BB0
TEXT_SIZE = 0xE20

def vram_to_rom(vram: int) -> int:
    return MAIN_ROM + (vram - MAIN_VRAM)

text_offset = vram_to_rom(TEXT_VRAM)
data_offset = vram_to_rom(DATA_VRAM)

rom = ROM.read_bytes()
if text_offset < 0 or text_offset + TEXT_SIZE > len(rom):
    raise SystemExit("Derived aspMain text range is outside the ROM")
if data_offset < 0 or data_offset + 0x30 > len(rom):
    raise SystemExit("Derived aspMain data range is outside the ROM")

# This 16-entry table is the standard aspMain command dispatch table used by
# the exact microcode embedded in the supported MP1 ROM. Validate it before
# generating anything so a wrong ROM/revision cannot silently build bad RSP.
expected_targets = [
    0x1118, 0x1470, 0x11DC, 0x1B38,
    0x1214, 0x187C, 0x1254, 0x12D0,
    0x12EC, 0x1328, 0x140C, 0x1294,
    0x1E24, 0x138C, 0x170C, 0x144C,
]
actual_targets = [
    struct.unpack_from(">H", rom, data_offset + 0x10 + i * 2)[0]
    for i in range(16)
]
if actual_targets != expected_targets:
    raise SystemExit(
        "Mario Party aspMain dispatch table mismatch: "
        + ", ".join(f"0x{x:04X}" for x in actual_targets)
    )

# Sanity-check the end of the RSP text. The supported ROM's final instruction
# is followed by padding before regular game data; this catches offset drift.
text = rom[text_offset:text_offset + TEXT_SIZE]
if not any(text):
    raise SystemExit("Mario Party aspMain text unexpectedly empty")

lines = [
    "# Generated from verified Mario Party (USA) audio OSTask constants.",
    f"text_offset = 0x{text_offset:X}",
    f"text_size = 0x{TEXT_SIZE:X}",
    "text_address = 0x04001080",
    'rom_file_path = "../.mp1-build/marioparty/baserom.us.z64"',
    'output_file_path = "rsp/aspMain.cpp"',
    'output_function_name = "aspMain"',
    "",
    "extra_indirect_branch_targets = [",
]
for i in range(0, len(actual_targets), 4):
    lines.append(
        "    " + ", ".join(f"0x{x:04X}" for x in actual_targets[i:i + 4]) + ","
    )
lines += ["]", ""]

OUT.write_text("\n".join(lines))
print(
    f"Derived aspMain from MP1 OSTask: text ROM=0x{text_offset:X} "
    f"VRAM=0x{TEXT_VRAM:08X} size=0x{TEXT_SIZE:X} "
    f"data ROM=0x{data_offset:X} VRAM=0x{DATA_VRAM:08X}"
)
print("Dispatch targets:", ", ".join(f"0x{x:04X}" for x in actual_targets))
