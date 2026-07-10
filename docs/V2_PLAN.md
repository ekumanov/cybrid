# Cybrid V2 — Project Review & Redesign Plan

*A review of Cybrid V1 (as of firmware V1.3) and a rough approach for a future major redesign.*

**TL;DR:** The sensing concept — optical hammer detection with time-of-flight velocity — is excellent
and should stay. The three things a V2 should eliminate are the 264 hand-tuned trimpots, the fixed
three-point binary sensing, and the hand-made 18-drop IDC cable. All three disappear with one
architectural shift: sample the sensors *continuously* with cheap modern MCUs and do the thresholding
in **software** instead of hardware comparators. Around that, the repo itself needs a round of
professionalization (license, BOM, structure, CI) that is worth doing even before any redesign.

---

## 1. What V1 is

A grand piano action turned into a MIDI controller. One CNY70 reflective optical sensor per hammer;
each sensor feeds three LM339 comparators whose reference voltages (set by trimpots) define three
distance gates: damper point, escapement point, strike point. A Teensy scans 18 groups of 5 notes
through tri-state bus transceivers over a shared ribbon cable, measures the hammer's flight time
between the escapement and strike gates, and maps it to MIDI velocity through a precomputed
logarithmic table with per-group grading. A continuous half-pedal is read via ADC.

Measured performance (from the V1.3 notes): full-keyboard scan in ~9 µs (~112 kHz scan rate),
velocity error at MIDI 127 under one step. That is genuinely competitive with commercial scanners.

## 2. What's good — keep these ideas

- **The core trick is clever and proven.** Hardware comparators turn a slow analog problem into a
  fast digital one; a 9 µs binary scan of 88 keys beats what naive ADC multiplexing could do in 2020.
  Latency and velocity resolution are excellent, and the instrument demonstrably works (video, daily
  playing, derivatives inspired by it).
- **It senses the right thing.** Hammer shank velocity at the strike point is what an acoustic piano
  "measures", not key position. This is the reason the result feels right, and it must not be lost
  in a redesign.
- **Zero heavy math in the hot path.** Velocity lookup is O(1); event logic is a small, correct state
  machine (strike / escapement / damper / rest, with latches preventing double-triggering).
- **Thoughtful low-level details.** Active-low logic, bus-turnaround settling delay, `INPUT_PULLUP`
  idle state, monotonicity enforcement in the velocity map, throttled + deadbanded pedal CC output.
- **Graded velocity.** The per-group multiplier emulating graded hammer weight is a real musical
  feature, not a gimmick.
- **Modular hardware.** Tiny sensor satellites decoupled from logic boards accommodate the uneven
  hammer spacing at action brackets; all boards are 2-layer and hand-solderable.
- **Honest documentation.** Known flaws, empirical constants, part links, and a written calibration
  procedure that encodes real regulation experience. That procedure is domain knowledge V2 should
  turn into software.

## 3. What's holding it back

### Hardware
- **264 trimpots.** Three per key, each set by hand with a wooden block, a plastic strip, and an LED.
  Analog thresholds drift with temperature, LED aging, and mechanical settling. This is the #1
  objection from people who considered reproducing the project — and the single biggest win
  available to a V2.
- **Binary three-point sensing.** Velocity comes from one time interval between two fixed points.
  No trajectory data, no note-off velocity, no bounce rejection beyond a latch, and the "voicing"
  of the instrument is physically frozen into trimpot positions.
- **The IDC-34 cable.** One hand-crimped ribbon with 18 connectors is a single point of failure and
  was, in practice: a shorted line permanently disabled group 16, and the workaround
  (`FAULTY_LINE 16`) ships as the *default* configuration of the reference firmware.
- **Power distribution.** USB-B input, an improvised 3.3 V regulator on a breadboard, and solid-core
  wire daisy-chained board to board. The README itself lists ripple as a known problem.
- **Assorted friction.** Manual group addressing via jumpers; the mirrored IDC footprint forcing the
  Teensy board upside down; SN74LVC245ADWR sourcing; 0603 hand-soldering as an entry barrier.

### Firmware
- **Two coexisting monoliths.** `My_Hybrid_Piano_V1.2.ino` (tested, Teensy 3.6, heavily unrolled
  copy-paste) and `My_Hybrid_Piano_V1.3.ino` (much cleaner, but by its own commit message *never run
  on hardware*, yet labeled "MASTER REFERENCE"). The calibration utilities still carry the old
  unrolled style and the commented-out group 16.
- **Personal workarounds as defaults.** `PIANO_SIZE 85`, `FAULTY_LINE 16`, the author's specific
  pedal limits — a stranger flashing the reference firmware gets CyberGene's broken cable map.
- **Calibration requires re-flashing.** Three separate sketches, uploaded in sequence, plus manual
  trimpot work. Nothing is configurable at runtime and nothing is stored on the device.
- **A ~200 KB lookup table** (17 groups × 12187 bytes) nearly fills a Teensy 3.6's RAM to avoid a
  `log()` call that only ever executes at note-on — a few hundred nanoseconds, once per keystroke.
  A small table with interpolation, or direct computation at note-on, does the same job in ~1 KB.
- **Magic constants** (`VEL_ADDITION 57.96`, `VEL_DISTANCE_FACTOR 1500.0`) with no recorded
  derivation, so nobody can adapt them to a different action geometry except by trial and error.
- **No engineering scaffolding.** No PlatformIO project, no CI, no tests (the velocity math is pure
  and trivially host-testable), no changelog, no releases.

### Repository & professionalism
- **No LICENSE file.** The README says "open-source", but legally the project isn't — nobody can
  safely reuse or fork it. This is a five-minute fix and the most important single item here.
- **53 MB of photos in git**, IDE config (`.idea/`) committed, KiCad autosave/backup files and
  generated gerbers committed alongside sources, file names with spaces and parentheses.
- **The README does five jobs at once** — concept, build guide, calibration manual, parts list,
  and raw email dumps — with `TBD` markers unchanged since 2020 and no BOM.
- **KiCad 5.0 (2018 format)** sources; current KiCad is several major versions ahead.

## 4. The V2 decision: continuous sensing, software thresholds

V1's architecture is a smart workaround for one 2020 constraint: ADCs were too slow to scan 88 keys.
Nearly every V2-worthy pain — trimpots, fixed gates, drift, the giant calibration effort — is a
consequence of doing thresholding in *hardware*. Modern microcontrollers remove the constraint:
a $1–2 MCU samples a dozen analog channels at tens of kHz each. So:

> **Keep the CNY70-on-hammer-shank concept. Replace the comparator matrix with continuous per-key
> sampling and do detection, calibration, and voicing in software.**

Sanity check on speed: a fortissimo hammer (~5 m/s) crosses the final 2 mm in ~400 µs. Sampling each
key at ≥20 kHz yields 8+ position samples through that window; a line/curve fit over those samples
matches or beats V1's two-edge timing (9 µs quantization) — and unlike V1, the full trajectory is
available.

| V1 pain | V2 consequence of continuous sensing |
|---|---|
| 264 trimpots, manual calibration | Per-key auto-calibration: learn each key's rest/max reflection, store in flash; thresholds become percentages of the learned range |
| Drift (temperature, LED aging) | Continuous re-baselining while the key is at rest |
| Fixed strike/escapement/damper gates | Arbitrary, per-key, runtime-adjustable thresholds; "voicing" from an app instead of a screwdriver |
| Velocity from 2 edges | Velocity from a fitted trajectory; bounce rejection; note-off (release) velocity; configurable curves per key |
| 18-drop IDC analog-ish bus | Short local analog runs + a simple 4-wire digital daisy chain |

## 5. Proposed V2 architecture (rough)

**Sensors — unchanged in spirit.** Keep the CNY70 (proven on this exact mechanical problem, cheap,
large sensing face) on tiny 3-wire satellite boards, now outputting the raw phototransistor voltage.
Verify 3.3 V operation with an adjusted load resistor in the prototype phase; QRE1113/ITR8307 are
fallbacks only if size or current demands it.

**Scanner boards — the new "note boards".** ~8 boards × 11 keys (instead of 18 × 5), each carrying a
small MCU (RP2040 or STM32G0 class — both cheap and assembly-service friendly) that samples its 11
sensors at ≥20 kHz/key, runs detection and timestamping locally, and emits compact events. Analog
paths shrink from "across the piano" to <10 cm. Boards auto-address by position in the chain — no
jumpers.

**Interconnect.** Half-duplex RS-485 (or CAN-FD if an STM32 with FDCAN is chosen) daisy-chained over
ordinary locking 4-wire cable (data pair + 5 V + GND). An event is ~8 bytes {key, type, velocity,
timestamp}; even a glissando is trivial bandwidth. The hub distributes a periodic clock-sync message
(sub-100 µs alignment is easy and sufficient).

**Hub board.** Teensy 4.1 (most mature USB-MIDI stack) or RP2040 + TinyUSB. USB-C for both power and
MIDI, one clean 5 V rail on the bus with a local 3.3 V LDO per board (this replaces the breadboard
regulator and the ripple problem), three pedal jacks with configurable curves, status LED, optional
DIN-5 MIDI out.

**Firmware.** One PlatformIO monorepo: `firmware/hub`, `firmware/scanner`, and a shared protocol +
velocity/calibration library written as portable C++ so the math runs under unit tests on the host.
GitHub Actions builds every commit; releases ship flashable artifacts. All configuration lives in
flash and is exposed over SysEx/serial — flashing is never part of calibration again.

**Companion app.** Browser-based (WebMIDI/WebSerial — zero install): live per-key position waveform,
an auto-calibration wizard that encodes the README's manual procedure, per-key velocity curve editor,
MIDI monitor, firmware update. This plays directly to the author's software-engineering strengths and
is the piece that will make V2 feel professional to other builders.

**MIDI features.** Note-on velocity (per-key + global curves), note-off velocity, CC64 half-pedal plus
two more pedals, channel configuration. Stretch: high-resolution velocity (CC#88) or MIDI 2.0 once
host support is worth it.

**Alternative considered — central ADC + analog muxes** (one Teensy, passive note boards): simpler,
one firmware image, but it reintroduces long analog runs across 1.2 m of piano — exactly what V1's
comparators were designed to avoid. The distributed design is the honest fix; the BOM impact is
roughly neutral once ~264 trimpots and ~72 comparator chips disappear.

## 6. What deliberately stays

Hammer-shank optical sensing; CNY70 satellites; modular boards sized around action-bracket gaps;
85- and 88-key support; ~2 W USB power budget; the calibration *procedure* (as software); the graded
velocity concept (as per-key curves).

## 7. Roadmap

**Phase 0 — make V1 presentable (no redesign; roughly a weekend).**
Add a license (suggestion: CERN-OHL-P for hardware, MIT for firmware, CC-BY-4.0 for docs/photos) —
without this nothing is actually open source. Tag the current state `v1.0` and create a GitHub
release. Restructure: `hardware/` (KiCad sources only; gerbers become release artifacts), `firmware/`
(PlatformIO-ified V1.2/V1.3 + calibration sketches), `docs/` (README split into concept / build /
calibration pages), photos resized into `docs/images/`. Write the BOM that has been "TBD" since 2020.
Either test V1.3 on the instrument or mark it experimental and make defaults generic
(`FAULTY_LINE 0`, 88 keys).

**Phase 1 — feasibility rig (one octave).**
Prototype one scanner board (MCU + 11 CNY70 channels) mounted beside the existing V1 electronics on
the same action. Record raw trajectories, validate velocity extraction against V1's output on the
same strikes, and prove auto-calibration. Pick the MCU, ADC arrangement, and sampling rate from this
data rather than from guesses. Exit criteria: velocity repeatability within ±1 MIDI step of V1, and a
key that calibrates itself.

**Phase 2 — full-system alpha.**
Eight scanner boards + hub (rev A), bus protocol with auto-addressing and clock sync, calibration in
flash, minimal web app (live view, auto-cal wizard, curve editor). Replace V1 on the instrument and
play it daily; iterate on the data.

**Phase 3 — community release.**
KiCad 8/9 sources, fab + assembly-service outputs (gerbers, BOM, CPL for e.g. JLCPCB) so nobody has
to hand-solder 0603 parts, interactive BOM, mounting drawings (DXF/STEP) for the sensor rail,
assembly & calibration guide, `v2.0` release. Stretch: note-off velocity voicing, MIDI 2.0, optional
damper/key sensing add-ons.

## 8. Risks & open questions

- **CNY70 spread and nonlinearity** — handled by per-key normalization, but Phase 1 must confirm SNR
  at 3.3 V drive (V1 ran the sensors at 5 V).
- **Optical crosstalk / ambient IR** — V1's always-on LEDs worked inside the piano, so likely a
  non-issue; modulated LED drive is the fallback if not.
- **Firmware updates across 9 MCUs** — needs a bus bootloader or hub-proxied update; decide in
  Phase 2, design the protocol for it in Phase 1.
- **Scope discipline** — each phase ends in something usable (a clean V1 repo, a validated octave, a
  playable alpha, a shareable release), so the project can pause at any phase without being wasted.
