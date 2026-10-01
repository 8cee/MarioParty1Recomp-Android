#!/usr/bin/env python3
from pathlib import Path

root = Path(".mp1-build/N64Recomp")
analysis = root / "src/analysis.cpp"
recompilation = root / "src/recompilation.cpp"

if not root.exists():
    raise SystemExit("N64Recomp source tree not found")

# Older N64Recomp revisions aborted when a static overlay target shared VRAM
# with another section. Newer revisions already fall back to runtime lookup
# for ambiguous JAL targets. Patch only the old behavior and positively verify
# that one of those two supported code paths exists.
old = 'throw std::runtime_error("Ambiguous function");'

if analysis.exists():
    text = analysis.read_text()
    if old in text:
        text = text.replace(
            old,
            '/* MP1 overlay: ambiguous static target is resolved at runtime. */\n'
            '                    return std::nullopt;',
            1,
        )
        analysis.write_text(text)
        print("Applied legacy Mario Party N64Recomp overlay compatibility patch")
        raise SystemExit(0)

if recompilation.exists():
    text = recompilation.read_text()
    markers = (
        "JalResolutionResult::Ambiguous",
        "falling back to function lookup",
        "use_lookup_for_all_function_calls",
    )
    if all(marker in text for marker in markers):
        print("N64Recomp already supports ambiguous MP1 overlay JAL fallback")
        raise SystemExit(0)

raise SystemExit(
    "Unsupported N64Recomp revision: could not find either the legacy "
    "ambiguous-function abort or the modern runtime-lookup fallback"
)
