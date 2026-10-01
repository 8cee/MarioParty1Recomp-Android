#!/usr/bin/env python3
from pathlib import Path
import re

root = Path("recomp/generated")
targets = ("HuPrcCall", "HuPrcSleep", "HuPrcChildWatch", "HuPrcTerminate")
counts = {name: 0 for name in targets}

for path in sorted(root.glob("funcs_*.c")):
    text = path.read_text()
    original = text
    for name in targets:
        # Rename only the generated function definition. Calls and funcs.h
        # keep the original symbol and therefore resolve to mp1_process_runtime.cpp.
        pattern = re.compile(
            rf"(?m)^(\s*(?:RECOMP_FUNC\s+)?void\s+){re.escape(name)}(\s*\(\s*uint8_t\s*\*\s*rdram\s*,\s*recomp_context\s*\*\s*ctx\s*\)\s*\{{)"
        )
        text, n = pattern.subn(rf"\1{name}_original\2", text)
        counts[name] += n
    if text != original:
        path.write_text(text)

bad = {name: n for name, n in counts.items() if n != 1}
if bad:
    raise SystemExit(f"Expected exactly one generated definition for each MP1 process function; got {bad}")

print("Patched generated MP1 process scheduler functions:", ", ".join(targets))
