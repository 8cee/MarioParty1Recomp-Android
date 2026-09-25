#!/usr/bin/env python3
from pathlib import Path
import re
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch-generated-process-runtime.py <generated-dir>")

root = Path(sys.argv[1])
files = sorted(root.glob("funcs_*.c"))
if not files:
    raise SystemExit(f"no funcs_*.c files found in {root}")

changed = 0
pattern = re.compile(r"(RECOMP_FUNC\s+void\s+)HuPrcCall(\s*\()")
for path in files:
    text = path.read_text()
    out, n = pattern.subn(r"\1HuPrcCall_original\2", text, count=1)
    if n:
        path.write_text(out)
        changed += n

if changed != 1:
    raise SystemExit(f"expected exactly one HuPrcCall definition, patched {changed}")

header = root / "funcs.h"
if header.exists():
    text = header.read_text()
    # Keep the public HuPrcCall declaration: the native runtime shim provides it.
    # Add a declaration for the renamed generated implementation only for
    # diagnostics/debugging; production code does not call it.
    if "HuPrcCall_original" not in text:
        text += "\nvoid HuPrcCall_original(uint8_t* rdram, recomp_context* ctx);\n"
        header.write_text(text)

print("Patched generated process scheduler: HuPrcCall -> HuPrcCall_original")
