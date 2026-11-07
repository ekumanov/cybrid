#!/usr/bin/env python3
"""Record the as-built V1 board colours (white solder mask, black silkscreen) in the board stackup.

The V1 boards were ordered with white solder mask and black silkscreen, but KiCad 5 had no
stackup, so 3D views and renders showed the default green. This adds a standard 2-layer 1.6 mm
stackup with those colours. It does not change copper, mask openings or fabrication outputs.

Usage: python3 set_stackup_colors.py <board>.kicad_pcb [...]
"""
import sys

STACKUP = """\t\t(stackup
\t\t\t(layer "F.SilkS" (type "Top Silk Screen") (color "Black"))
\t\t\t(layer "F.Paste" (type "Top Solder Paste"))
\t\t\t(layer "F.Mask" (type "Top Solder Mask") (color "White") (thickness 0.01))
\t\t\t(layer "F.Cu" (type "copper") (thickness 0.035))
\t\t\t(layer "dielectric 1" (type "core") (color "FR4 natural") (thickness 1.51) (material "FR4") (epsilon_r 4.5) (loss_tangent 0.02))
\t\t\t(layer "B.Cu" (type "copper") (thickness 0.035))
\t\t\t(layer "B.Mask" (type "Bottom Solder Mask") (color "White") (thickness 0.01))
\t\t\t(layer "B.Paste" (type "Bottom Solder Paste"))
\t\t\t(layer "B.SilkS" (type "Bottom Silk Screen") (color "Black"))
\t\t\t(dielectric_constraints no)
\t\t)
"""

for path in sys.argv[1:]:
    text = open(path, encoding="utf-8").read()
    if "(stackup" in text:
        print(f"{path}: already has a stackup, left unchanged")
        continue
    text = text.replace("\t(setup\n", "\t(setup\n" + STACKUP, 1)
    open(path, "w", encoding="utf-8").write(text)
    print(f"{path}: stackup added")
