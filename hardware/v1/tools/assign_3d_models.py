#!/usr/bin/env python3
"""Point the V1 footprints at 3D models from the stock KiCad library.

The boards were drawn in KiCad 5 with footprints whose 3D-model links were either missing or used
the old ${KISYS3DMOD} library layout, so 3D views and renders showed bare pads. This rewrites the
(model ...) entry of each footprint, chosen by footprint library ID. It changes only 3D-view data:
copper, silkscreen, mask and fabrication outputs are untouched.

Usage: python3 assign_3d_models.py <board>.kicad_pcb [...]
"""
import re
import sys

LIB = "${KICAD10_3DMODEL_DIR}"

# footprint lib ID -> (model path, offset xyz mm, rotate xyz deg)
MODELS = {
    "Capacitors_SMD:C_0603_HandSoldering": ("Capacitor_SMD.3dshapes/C_0603_1608Metric.step", (0, 0, 0), (0, 0, 0)),
    "Resistors_SMD:R_0603_HandSoldering": ("Resistor_SMD.3dshapes/R_0603_1608Metric.step", (0, 0, 0), (0, 0, 0)),
    "Connectors_JST:JST_XH_B03B-XH-A_03x2.50mm_Straight": (
        "Connector_JST.3dshapes/JST_XH_B3B-XH-A_1x03_P2.50mm_Vertical.step", (0, 0, 0), (0, 0, 0)),
    "Connectors_Multicomp:Multicomp_MC9A12-3434_2x17x2.54mm_Straight": (
        "Connector_IDC.3dshapes/IDC-Header_2x17_P2.54mm_Vertical.step", (0, 0, 0), (0, 0, -90)),
    "Pin_Headers:Pin_Header_Straight_1x02_Pitch2.54mm": (
        "Connector_PinHeader_2.54mm.3dshapes/PinHeader_1x02_P2.54mm_Vertical.step", (0, 0, 0), (0, 0, 0)),
    "Pin_Headers:Pin_Header_Straight_1x03_Pitch2.54mm": (
        "Connector_PinHeader_2.54mm.3dshapes/PinHeader_1x03_P2.54mm_Vertical.step", (0, 0, 0), (0, 0, 0)),
    "Pin_Headers:Pin_Header_Straight_2x09_Pitch2.54mm": (
        "Connector_PinHeader_2.54mm.3dshapes/PinHeader_2x09_P2.54mm_Vertical.step", (0, 0, 0), (0, 0, 0)),
    "Socket_Strips:Socket_Strip_Straight_1x03_Pitch2.54mm": (
        "Connector_PinHeader_2.54mm.3dshapes/PinHeader_1x03_P2.54mm_Horizontal.step", (0, 0, 0), (0, 0, 0)),
    "Potentiometers:Potentiometer_Trimmer_Bourns_3296W": (
        "Potentiometer_THT.3dshapes/Potentiometer_Bourns_3296W_Vertical.step", (0, 0, 0), (0, 0, 0)),
    "SOIC127P600X175-14N": ("Package_SO.3dshapes/SOIC-14_3.9x8.7mm_P1.27mm.step", (0, 0, 0), (0, 0, 0)),
    "SOIC127P1030X265-20N": ("Package_SO.3dshapes/SOIC-20W_7.5x12.8mm_P1.27mm.step", (0, 0, 0), (0, 0, 0)),
    "DIPS254W50P254L710H600Q4": ("OptoDevice.3dshapes/Vishay_CNY70.step", (-1.4, 1.27, 0), (0, 0, 0)),
    "Connectors:BARREL_JACK": ("Connector_BarrelJack.3dshapes/BarrelJack_Horizontal.step", (0, 0, 0), (0, 0, 0)),
}


def model_block(path, offset, rotate):
    xyz = lambda v: " ".join(f"{c:g}" for c in v)
    return (f'\t\t(model "{LIB}/{path}"\n'
            f'\t\t\t(offset\n\t\t\t\t(xyz {xyz(offset)})\n\t\t\t)\n'
            f'\t\t\t(scale\n\t\t\t\t(xyz 1 1 1)\n\t\t\t)\n'
            f'\t\t\t(rotate\n\t\t\t\t(xyz {xyz(rotate)})\n\t\t\t)\n'
            f'\t\t)\n')


def strip_models(block):
    """Remove every (model ...) child of a footprint block (KiCad writes them at two tabs)."""
    out, depth, skipping = [], 0, False
    for line in block.splitlines(keepends=True):
        if not skipping and line.startswith("\t\t(model "):
            skipping, depth = True, 0
        if skipping:
            depth += line.count("(") - line.count(")")
            if depth <= 0:
                skipping = False
            continue
        out.append(line)
    return "".join(out)


def process(path):
    text = open(path, encoding="utf-8").read()
    # Footprint blocks start with "\t(footprint" and end with the next line that is exactly "\t)".
    pattern = re.compile(r'^\t\(footprint "([^"]+)".*?^\t\)\n', re.S | re.M)
    counts = {}

    def fix(m):
        lib_id = m.group(1)
        if lib_id not in MODELS:
            return m.group(0)
        block = strip_models(m.group(0))
        counts[lib_id] = counts.get(lib_id, 0) + 1
        return block[: -len("\t)\n")] + model_block(*MODELS[lib_id]) + "\t)\n"

    text = pattern.sub(fix, text)
    open(path, "w", encoding="utf-8").write(text)
    for lib_id, n in sorted(counts.items()):
        print(f"{path}: {n} x {lib_id}")


if __name__ == "__main__":
    for p in sys.argv[1:]:
        process(p)
