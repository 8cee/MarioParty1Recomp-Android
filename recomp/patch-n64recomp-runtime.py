#!/usr/bin/env python3
from pathlib import Path
root=Path(".mp1-build/N64Recomp")
files=list((root/"src").glob("*.cpp"))+list((root/"src").glob("*.h"))
changed=[]
for p in files:
    t=p.read_text()
    u=t
    # Older N64Recomp revisions used by MP1 need longjmp treated as a runtime
    # function. Insert it next to setjmp when the entry is absent.
    if '"setjmp"' in u and '"longjmp"' not in u:
        u=u.replace('"setjmp"', '"setjmp",\n        "longjmp"', 1)
    if u!=t:
        p.write_text(u); changed.append(str(p))
print("MP1 N64Recomp runtime patch:", ", ".join(changed) if changed else "already compatible")
