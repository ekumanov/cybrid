#!/usr/bin/env bash
# Generates everything that is derived from the V1 KiCad sources, using kicad-cli (KiCad 10+):
#
#   build/<board>/<board>-schematic.pdf    schematic
#   build/<board>/<board>-bom.csv          bill of materials, grouped by value and footprint
#   build/<board>/<board>-{top,bottom,iso}.png   3D renders
#   build/<board>/fab/                     Gerbers + Excellon drill files (what a PCB fab needs)
#   build/<board>/<board>-fab.zip          the same, zipped for upload
#   build/<board>/<board>-erc.rpt, -drc.rpt   electrical and design rule check reports
#
# Usage:  hardware/v1/export.sh [board ...]       (default: all three boards)
#         KICAD_CLI=/path/to/kicad-cli hardware/v1/export.sh
#         DOCS=1 hardware/v1/export.sh            also refresh the schematics and renders used by the docs
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
OUT="$HERE/build"

if [ -z "${KICAD_CLI:-}" ]; then
  if command -v kicad-cli > /dev/null; then
    KICAD_CLI=kicad-cli
  elif [ -x /Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli ]; then
    KICAD_CLI=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
  else
    echo "kicad-cli not found: install KiCad 10 or set KICAD_CLI" >&2
    exit 1
  fi
fi

boards=("$@")
[ ${#boards[@]} -gt 0 ] || boards=(teensy-board note-board sensor-board)

render() { # render <pcb> <png> [kicad-cli render options...]; a failed render only warns
  local pcb="$1" png="$2"; shift 2
  "$KICAD_CLI" pcb render --quality high --use-board-stackup-colors --background opaque \
    -w 1600 -h 1200 "$@" -o "$png" "$pcb" > /dev/null \
    || echo "warning: could not render $(basename "$png")" >&2
}

for b in "${boards[@]}"; do
  sch="$HERE/$b/$b.kicad_sch"
  pcb="$HERE/$b/$b.kicad_pcb"
  dir="$OUT/$b"
  echo "=== $b"
  rm -rf "$dir"
  mkdir -p "$dir/fab"

  "$KICAD_CLI" sch export pdf -o "$dir/$b-schematic.pdf" "$sch" > /dev/null
  "$KICAD_CLI" sch export bom -o "$dir/$b-bom.csv" \
    --fields 'Reference,Value,Footprint,${QUANTITY}' --labels 'Refs,Value,Footprint,Qty' \
    --group-by 'Value,Footprint' --sort-field Reference --exclude-dnp "$sch" > /dev/null

  # Gerbers for every layer used by the board, plus drill files and a drill map.
  "$KICAD_CLI" pcb export gerbers --no-protel-ext --subtract-soldermask \
    -l F.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts \
    -o "$dir/fab/" "$pcb" > /dev/null
  "$KICAD_CLI" pcb export drill --format excellon --excellon-separate-th \
    --generate-map --map-format pdf -o "$dir/fab/" "$pcb" > /dev/null
  (cd "$dir/fab" && python3 -m zipfile -c "../$b-fab.zip" *)

  render "$pcb" "$dir/$b-top.png" --side top
  render "$pcb" "$dir/$b-bottom.png" --side bottom
  render "$pcb" "$dir/$b-iso.png" --rotate '-40,0,-20' --perspective --zoom 0.9

  "$KICAD_CLI" sch erc -o "$dir/$b-erc.rpt" "$sch" | grep -E '^Found' || true
  "$KICAD_CLI" pcb drc --schematic-parity -o "$dir/$b-drc.rpt" "$pcb" | grep -E '^Found' || true

  if [ "${DOCS:-0}" = 1 ]; then
    cp "$dir/$b-schematic.pdf" "$HERE/$b/"
    mkdir -p "$REPO/docs/images/v1/boards"
    for v in top bottom iso; do
      # Docs images are downscaled JPEGs to keep the repository small.
      sips -s format jpeg -s formatOptions 82 -Z 1200 "$dir/$b-$v.png" \
        --out "$REPO/docs/images/v1/boards/$b-$v.jpg" > /dev/null 2>&1 \
        || python3 -c "import sys; from PIL import Image; im=Image.open(sys.argv[1]).convert('RGB'); im.thumbnail((1200,1200)); im.save(sys.argv[2], quality=82)" \
             "$dir/$b-$v.png" "$REPO/docs/images/v1/boards/$b-$v.jpg"
    done
  fi
done

echo "Outputs in ${OUT#"$REPO/"}/"
