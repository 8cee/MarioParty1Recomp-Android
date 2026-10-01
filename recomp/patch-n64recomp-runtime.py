#!/usr/bin/env python3
from pathlib import Path

root = Path(".mp1-build/N64Recomp")
symbol_lists = root / "src/symbol_lists.cpp"
if not symbol_lists.exists():
    raise SystemExit("N64Recomp symbol list source not found")

text = symbol_lists.read_text()

# Mario Party's process system uses both setjmp and longjmp. Recent
# N64Recomp revisions already classify both as runtime functions.
if '"setjmp"' not in text:
    raise SystemExit("Unsupported N64Recomp revision: setjmp runtime symbol missing")

if '"longjmp"' in text:
    print("N64Recomp already treats longjmp as a runtime function")
else:
    text = text.replace('"setjmp"', '"setjmp",\n    "longjmp"', 1)
    symbol_lists.write_text(text)
    print("Added longjmp to N64Recomp runtime symbol list")
