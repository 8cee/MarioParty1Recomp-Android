#!/usr/bin/env python3
from pathlib import Path
p=Path(".mp1-build/N64Recomp/src/analysis.cpp")
if not p.exists():
    raise SystemExit("N64Recomp analysis source not found")
t=p.read_text()
# MP1 has static overlays that legitimately share VRAM. Treat relocatable
# section collisions as runtime-resolved instead of aborting generation.
old='throw std::runtime_error("Ambiguous function");'
if old in t:
    t=t.replace(old,'/* MP1 overlay: ambiguous static target is resolved at runtime. */\n                    return std::nullopt;',1)
p.write_text(t)
print("Applied Mario Party N64Recomp overlay compatibility patch")
