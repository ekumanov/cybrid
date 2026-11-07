#!/usr/bin/env python3
"""One-off fix (2026): write the V1 component values into the KiCad sources.

The 2020 schematics left generic values ("R", "C", "R_POT_TRIM"); the real values were only in
e-mails quoted in the old README. This sets them in the schematic symbols and the matching board
footprints, so the generated BOM is complete. Values only: no connectivity or layout change.
"""
import re
import sys

VALUES = {
    "note-board": {
        **{f"C{i}": "100nF" for i in range(1, 7)},
        **{r: "10k" for r in ["R1"] + [f"R{g}{i}" for g in range(1, 5) for i in range(1, 5)]},
        **{f"RV{g}{i}": "10k" for g in range(1, 6) for i in range(1, 4)},
    },
    "sensor-board": {"R1": "240"},
    "teensy-board": {},
}
# Footprints left empty on the as-built board (optional current limiting for pedal inputs).
DNP = {"teensy-board": {"R1", "R2", "R3"}}


def edit_blocks(text, start, board):
    """Edit each top-level symbol/footprint block that starts with `start` (one tab deep)."""
    pattern = re.compile(r"^\t\(" + start + r"\b.*?^\t\)\n", re.S | re.M)
    changed = []

    def fix(m):
        block = m.group(0)
        ref = re.search(r'\(property "Reference" "([^"]+)"', block)
        if not ref:
            return block
        ref = ref.group(1)
        value = VALUES[board].get(ref)
        if value:
            block = re.sub(r'\(property "Value" "[^"]*"', f'(property "Value" "{value}"', block, count=1)
            changed.append(f"{ref}={value}")
        if ref in DNP.get(board, ()):
            if start == "symbol":
                block = re.sub(r"\(dnp no\)", "(dnp yes)", block, count=1)
            else:
                block = re.sub(r"\(attr ([^)]*)\)",
                               lambda a: a.group(0) if "dnp" in a.group(1) else f"(attr {a.group(1)} dnp)",
                               block, count=1)
            changed.append(f"{ref}=DNP")
        return block

    return pattern.sub(fix, text), changed


for board in sys.argv[1:]:
    for ext, start in (("kicad_sch", "symbol"), ("kicad_pcb", "footprint")):
        path = f"{board}/{board}.{ext}"
        text, changed = edit_blocks(open(path, encoding="utf-8").read(), start, board)
        open(path, "w", encoding="utf-8").write(text)
        print(f"{path}: {len(changed)} edits")
