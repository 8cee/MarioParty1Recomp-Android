#!/usr/bin/env python3
from pathlib import Path
import struct, sys

try:
    from elftools.elf.elffile import ELFFile
except ImportError:
    raise SystemExit("pyelftools is required (pip install pyelftools)")

ROOT = Path(__file__).resolve().parents[1]
ELF = ROOT / ".mp1-build/marioparty/build/marioparty.elf"
MAP = ROOT / ".mp1-build/marioparty/build/marioparty.map"
ROM = ROOT / ".mp1-build/marioparty/baserom.us.z64"
OUT = ROOT / "recomp/aspMain.toml"

if not ELF.is_file() or not ROM.is_file():
    raise SystemExit("MP1 ELF/ROM missing; build the verified decomp first")

def map_symbols_from_linker_map(names):
    if not MAP.is_file():
        return {}
    import re
    wanted = set(names)
    found = {}
    # GNU ld map files commonly emit either:
    #   0x00000000800B1830                aspMainTextStart
    # or the symbol name before an assignment. Accept both forms.
    for line in MAP.read_text(errors="replace").splitlines():
        for name in tuple(wanted - found.keys()):
            if name not in line:
                continue
            m = re.search(r"0x([0-9A-Fa-f]{8,16})", line)
            if m:
                found[name] = int(m.group(1), 16)
    return found

def symbol_bytes(elf, start_name, end_name):
    syms = {}
    symtab = elf.get_section_by_name(".symtab")
    if symtab is not None:
        for sym in symtab.iter_symbols():
            if sym.name in (start_name, end_name):
                syms[sym.name] = int(sym["st_value"])

    if start_name not in syms or end_name not in syms:
        fallback = map_symbols_from_linker_map((start_name, end_name))
        syms.update(fallback)

    if start_name not in syms or end_name not in syms:
        raise RuntimeError(
            f"Missing {start_name}/{end_name} in ELF and linker map {MAP}"
        )

    start, end = syms[start_name], syms[end_name]
    if end <= start:
        raise RuntimeError(f"Invalid symbol range {start_name}/{end_name}")
    for sec in elf.iter_sections():
        lo = int(sec["sh_addr"])
        hi = lo + int(sec["sh_size"])
        if lo <= start and end <= hi:
            data = sec.data()
            off = start - lo
            return start, end, data[off:off + (end-start)]
    raise RuntimeError(f"Could not map {start_name} to an ELF section")

with ELF.open("rb") as f:
    elf = ELFFile(f)
    _, _, text = symbol_bytes(elf, "aspMainTextStart", "aspMainTextEnd")
    _, _, data = symbol_bytes(elf, "aspMainDataStart", "aspMainDataEnd")

rom = ROM.read_bytes()
text_offset = rom.find(text)
if text_offset < 0:
    raise SystemExit("aspMain text bytes from ELF were not found in the verified ROM")
if rom.find(text, text_offset + 1) >= 0:
    raise SystemExit("aspMain text occurs more than once in ROM; refusing ambiguous config")

data_offset = rom.find(data)
if data_offset < 0:
    raise SystemExit("aspMain data bytes from ELF were not found in the verified ROM")

# Standard libultra ABI audio tasks dispatch 16 commands through the first
# 16 halfwords at DMEM 0x10 in aspMainData. These are exact IMEM targets.
if len(data) < 0x30:
    raise SystemExit("aspMain data is too small to contain the command dispatch table")
targets = [struct.unpack_from(">H", data, 0x10 + i*2)[0] for i in range(16)]
targets = [x for x in targets if 0x1000 <= x < 0x2000]
if len(targets) < 8:
    raise SystemExit(f"aspMain dispatch table did not look valid: {targets!r}")

lines = [
    "# Generated from the verified Mario Party (USA) decomp ELF/ROM.",
    f"text_offset = 0x{text_offset:X}",
    f"text_size = 0x{len(text):X}",
    "text_address = 0x04001080",
    'rom_file_path = "../.mp1-build/marioparty/baserom.us.z64"',
    'output_file_path = "rsp/aspMain.cpp"',
    'output_function_name = "aspMain"',
    "",
    "extra_indirect_branch_targets = [",
]
for i in range(0, len(targets), 4):
    lines.append("    " + ", ".join(f"0x{x:04X}" for x in targets[i:i+4]) + ",")
lines += ["]", ""]
OUT.write_text("\n".join(lines))
print(f"Derived aspMain: ROM=0x{text_offset:X} size=0x{len(text):X} data=0x{data_offset:X}")
print("Dispatch targets:", ", ".join(f"0x{x:04X}" for x in targets))
