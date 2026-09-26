#!/usr/bin/env bash
# Host tests for the Cybrid firmware: builds each sketch against the hardware simulation in
# sim_arduino.h (85 keys, CyberGene's shorted cable line) and checks that V1.3
#   1. passes the hardware and piano-behaviour checks built into sim.cpp;
#   2. produces exactly the reviewed reference output in expected/v1_3.txt, so every change in
#      behaviour shows up as a diff of that file (accept one with UPDATE_EXPECTED=1);
#   3. configured for 88 keys and a healthy cable, behaves the same on the first 85 keys and plays
#      the 3 extra keys.
# V1.2 is also run and compared, for information only: it is the first firmware and the one the
# instrument was voiced with, not a specification.
# Usage: test/host/run.sh                     (needs a C++17 compiler; set CXX to override)
#        UPDATE_EXPECTED=1 test/host/run.sh   accept V1.3's current output as the new reference
#        V13=path/to/other.ino test/host/run.sh   test another sketch instead of V1.3
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="$ROOT/.pio/host-test"
CXX="${CXX:-c++}"
mkdir -p "$OUT"

V12="${V12:-$ROOT/src/teensy/main/My_Hybrid_Piano_V1.2/My_Hybrid_Piano_V1.2.ino}"
V13="${V13:-$ROOT/src/teensy/main/My_Hybrid_Piano_V1.3/My_Hybrid_Piano_V1.3.ino}"

# build <name> <sketch> [extra compiler flags...]
build() {
  local name="$1" sketch="$2"; shift 2
  mkdir -p "$OUT/$name"
  # The Arduino IDE generates prototypes automatically; do the same for top-level functions.
  sed -nE 's/^(void|int|byte|bool|boolean) ([A-Za-z_][A-Za-z0-9_]*\([^)]*\)) *\{.*/\1 \2;/p' "$sketch" > "$OUT/$name/protos.h"
  "$CXX" -std=c++17 -O2 -Wall -Wno-unused-variable -I"$HERE" -I"$HERE/stubs" -I"$OUT/$name" \
    -DSKETCH="\"$sketch\"" "$@" "$HERE/sim.cpp" -o "$OUT/$name/sim"
}

# run <name>: runs the simulation, keeping MIDI output in <name>.txt
run() {
  echo "--- $1"
  "$OUT/$1/sim" > "$OUT/$1.txt"
}

status=0

EXPECTED="$HERE/expected/v1_3.txt"

build v1_3 "$V13"
run v1_3 || status=1

if [ "${UPDATE_EXPECTED:-0}" = 1 ]; then
  mkdir -p "$(dirname "$EXPECTED")"
  cp "$OUT/v1_3.txt" "$EXPECTED"
  echo "UPDATED: $EXPECTED ($(wc -l < "$EXPECTED" | tr -d ' ') MIDI events) - review and commit the diff"
elif diff -q "$EXPECTED" "$OUT/v1_3.txt" > /dev/null; then
  echo "OK: V1.3 output matches the reference ($(wc -l < "$EXPECTED" | tr -d ' ') MIDI events)"
else
  echo "FAIL: V1.3 output differs from the reference (first differences below)."
  echo "      If the change is intended, run UPDATE_EXPECTED=1 test/host/run.sh and commit the new reference."
  diff "$EXPECTED" "$OUT/v1_3.txt" | head -20
  status=1
fi

# Information only: how V1.3 compares with V1.2.
build v1_2 "$V12"
if run v1_2 2> "$OUT/v1_2.err"; then
  v12_checks="passes the behaviour checks"
else
  v12_checks="fails $(grep -c '^FAIL' "$OUT/v1_2.err") behaviour check(s)"
fi
v12_diff=$(diff "$OUT/v1_2.txt" "$OUT/v1_3.txt" | grep -c '^[<>]' || true)
echo "INFO: V1.2 $v12_checks; $v12_diff line(s) of MIDI output differ from V1.3"

# V1.3 configured for an 88-key piano with a healthy cable.
sed -e 's/^#define PIANO_SIZE 85/#define PIANO_SIZE 88/' -e 's/^#define FAULTY_LINE 16/#define FAULTY_LINE 0/' \
  "$V13" > "$OUT/My_Hybrid_Piano_V1.3_88.ino"
build v1_3_88 "$OUT/My_Hybrid_Piano_V1.3_88.ino" -DSIM_PIANO_KEYS=88 -DSIM_SHORTED_LINE=0
run v1_3_88 || status=1
# The extra keys add scenario time, so compare the velocity-sweep note-ons per key rather than timestamps.
sweep() { awk '$2 == "note" && $4 > 0 && $3 <= 105 { print $3, $4 }' "$1" | head -n $((85 * 11)) | sort -n -s -k1,1; }
if diff -q <(sweep "$OUT/v1_3.txt") <(sweep "$OUT/v1_3_88.txt") > /dev/null; then
  echo "OK: 88-key V1.3 matches 85-key V1.3 on A0-A7"
else
  echo "FAIL: 88-key V1.3 differs from 85-key V1.3 on A0-A7"
  status=1
fi
if [ "$(awk '$2 == "note" && $4 > 0 && $3 >= 106 { n++ } END { print n + 0 }' "$OUT/v1_3_88.txt")" -eq 0 ]; then
  echo "FAIL: 88-key V1.3 never played A#7-C8"
  status=1
fi

exit $status
