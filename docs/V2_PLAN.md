# Cybrid V2 — Project Review & Redesign Plan

*A review of Cybrid V1 (hardware as built, firmware V1.2 / V1.3) and a rough approach for a future major
redesign. Revised after a netlist-level review of the PCBs, and again after a scan-timing analysis of the
compiled firmware and the V1.3 rework; see [Revision notes](#9-revision-notes).*

**TL;DR:** The sensing concept — optical hammer-shank detection with time-of-flight velocity — is excellent
and should stay. The three things a V2 should eliminate are the 255–264 hand-tuned trimpots, the fixed
three-point binary sensing, and the hand-made 18-drop IDC cable. All three disappear with one
architectural shift: sample the sensors *continuously* with cheap modern MCUs and do the thresholding
in **software** instead of hardware comparators. The hard problem in V2 is not sampling speed but
**signal-to-noise and power**: the sensor sees only a fraction of a millimetre of motion around the
strike point, and the V1 sensor LEDs alone draw ~7 W. Phase 1 exists to measure exactly that. Around
this, the repo needs a round of professionalization (license, BOM, structure, CI) that is worth doing
even before any redesign.

---

## 1. What V1 is

A grand piano action turned into a MIDI controller. One CNY70 reflective optical sensor per hammer,
mounted on a rail above the hammer shank near its flange; each sensor feeds three LM339 comparators
whose reference voltages (set by trimpots) define three distance gates: damper point, escapement
point, strike point. A Teensy scans 18 groups of 5 notes through tri-state bus transceivers over a
shared ribbon cable, measures the hammer's flight time between the escapement and strike gates, and
maps it to MIDI velocity through a precomputed logarithmic table with per-group grading. A continuous
half-pedal is read via ADC.

**Scan speed.** The recorded figure is a full-keyboard scan of ~8.9 µs (~112 kHz, ~0.5 µs per group),
measured with V1.2. Because that measurement is uncertain, the scan time was also derived from the
compiled firmware: one idle `loop()` was run in an ARM emulator (Unicorn) to count every instruction and
GPIO access exactly, and those counts were priced with published GPIO access times (Teensy 3.6: ~8
cycles per pin read; Teensy 4.1: fast GPIO on the 600 MHz AHB bus, a few cycles). Estimates for one
85-key scan, fast / **typical** / slow:

| Firmware | Teensy 4.1 (600 MHz) | Teensy 3.6 (256 MHz) | Settling per group |
|---|---|---|---|
| V1.2 | 3.1 / **5.1** / 8.1 µs | 15.3 / **19.3** / 24.2 µs | ~40–110 ns (4 dummy reads) |
| V1.3 as first committed | 8.4 / **11.8** / 16.0 µs | 22.2 / **27.1** / 33.6 µs | 100 ns |
| V1.3 now (pipelined, port snapshots) | 2.8 / **3.6** / 5.0 µs | 9.6 / **11.6** / 14.5 µs | ≥100 ns + 20 ns dead time |

The 8.9 µs figure fits only a Teensy 4.1; a Teensy 3.6 cannot scan that fast. At fortissimo (~300 µs
flight time, ~0.14 velocity steps per µs) a 5 µs scan means at most ±0.7 steps of timing jitter —
below what the ear resolves. That is competitive with commercial scanners, and a direct measurement
with a cycle-counter benchmark build is still to be done on the instrument.

## 2. What's good — keep these ideas

- **The core trick is clever and proven.** Hardware comparators turn a slow analog problem into a
  fast digital one; a 9 µs binary scan of 88 keys beats what naive ADC multiplexing could do in 2020.
  Latency and velocity resolution are excellent, and the instrument demonstrably works (video, daily
  playing, derivatives inspired by it).
- **It senses the right thing.** Hammer shank motion at the end of free flight is what an acoustic
  piano "measures", not key position. This is the reason the result feels right, and it must not be
  lost in a redesign.
- **Zero heavy math in the hot path.** Velocity lookup is O(1); event logic is a small, correct state
  machine (strike / escapement / damper / rest, with latches preventing double-triggering).
- **The detection logic matches a grand action.** Checked against piano mechanics rather than taken
  on trust: velocity linear in log(flight time) is linear in log(hammer speed), i.e. in dB — the right
  shape; a hammer caught by the backcheck rests below the escapement point, so repetition re-arms
  without a full release; a rebound off the rail counts as one note; an aborted press (escapement
  reached, no strike) is re-timed from the next approach. These are now executable checks (see §3,
  Firmware).
- **Sound low-level details.** Active-low logic with open-collector comparators pulled up to 3.3 V,
  transceiver outputs enabled one group at a time, a settling wait after each group switch, and a
  deadbanded pedal CC output.
- **Graded velocity.** The per-group velocity curves emulating graded hammer weight are a real musical
  feature — and in V1.2 they were hand-voiced on the instrument (the bass groups deliberately share
  flatter curves). That voicing is data worth preserving into V2.
- **Modular hardware.** Tiny sensor satellites decoupled from logic boards accommodate the uneven
  hammer spacing at action brackets; all boards are 2-layer and hand-solderable.
- **Honest documentation.** Known flaws, empirical constants, part links, and a written calibration
  procedure that encodes real regulation experience. That procedure is domain knowledge V2 should
  turn into software.

## 3. What's holding it back

### Hardware
- **255–264 trimpots.** Three per key, each set by hand with a wooden block, a plastic strip, and an
  LED. Analog thresholds drift with temperature, LED aging, supply voltage, and mechanical settling.
  This is the #1 objection from people who considered reproducing the project — and the single
  biggest win available to a V2.
- **Binary three-point sensing.** Velocity comes from one time interval between two fixed points.
  No trajectory data, no note-off velocity, no bounce rejection beyond a latch, and the "voicing"
  of the instrument is physically frozen into trimpot positions.
- **Comparator operating limits.** The LM339s run from 5 V, so their input common-mode range ends at
  about 3.5 V (VCC − 1.5 V, less when hot). The trimpots span 0–5 V and the sensor emitter output
  (4.7 kΩ load) can approach 5 V with the hammer close. If both the reference and the sensor are above
  ~3.5 V the output is not guaranteed — most likely at the strike point. There is also no hysteresis:
  a slowly moving hammer can make an output chatter, and each chatter at the escapement gate restarts
  the flight timer (slightly too-loud soft notes).
- **The IDC-34 cable.** One hand-crimped ribbon with 18 connectors and a single ground wire for
  fast-switching signals over ~1.5 m. It is a single point of failure and was, in practice: a line
  shorted to group 7 permanently disabled group 16, and the workaround (`FAULTY_LINE 16`) ships as the
  *default* configuration of the reference firmware. It is also what limits the scan: after a group is
  enabled the cable needs time to settle (reflections from 17 connector stubs) — empirically about
  40–110 ns, since V1.2 on a Teensy 4.1 needed four dummy reads before reading a group. V1.2 also
  enabled the next group immediately after disabling the previous one, with no dead time, so two
  transceivers could briefly drive the bus together (V1.3 now adds 20 ns), and on the first scan after
  power-up it enabled all groups at once (harmless with the hammers at rest; V1.3 idles the lines HIGH
  first).
- **Power.** By the schematic values each CNY70 LED draws ~16 mA ((5 V − ~1.15 V) / 240 Ω), so 85 sensors
  take ~1.4 A — **about 7 W total**, not the ~2 W the README states (worth measuring). This runs through
  JST-XH connectors and solid-core wire daisy-chained over 17–18 boards, with only 100 nF ceramics on
  the boards and no bulk capacitance anywhere; the USB-B and barrel-jack 5 V inputs are tied together
  without a diode, and 3.3 V comes from an improvised regulator on a breadboard. The README lists
  ripple as a known problem; this is why.
- **Power sequencing.** The Teensy is powered from the computer's USB, everything else from a charger.
  With the charger on and the Teensy unpowered, the transceiver outputs and the 10 kΩ enable pull-ups
  back-feed the Teensy through its I/O protection diodes, and the group enables float, so several
  groups can drive the shared bus at once.
- **Assorted friction.** Manual group addressing via jumpers; the ribbon connector is placed rotated
  (90° on the Teensy board vs 270° on the note boards), which forces the Teensy board to be mounted
  upside down; sensor connectors are numbered J1, J2, J5, J3, J4 left to right (so the RV5x trimpots
  serve the *third* key of a group) and this is documented nowhere; SN74LVC245ADWR sourcing; 0603
  hand-soldering as an entry barrier.

### Firmware
- **Two coexisting firmwares, the newer one unplayed.** `My_Hybrid_Piano_V1.2.ino` (the author's first
  firmware, played daily, heavily unrolled copy-paste) and `My_Hybrid_Piano_V1.3.ino` (an AI-assisted
  rewrite, never run on the instrument). V1.3 as first committed silently changed behaviour in three
  ways — it drove the shorted faulty line against group 7, dropped V1.2's hand-voiced bass curves (bass
  up to 11 velocity steps louder), and added a clamp that made the top treble jump by up to 11 steps —
  and it was over twice as slow as V1.2, because GCC did not inline the per-key function, so every
  input read went through a runtime pin lookup table. All of this is fixed, and V1.3 was then reworked:
  each group's 15 inputs are captured with 4 GPIO port-register reads and processed while the next group
  settles; the settling (≥100 ns) and dead-time (20 ns) waits are compile-time cycle counts; a build-time
  check stops the build if an input pin is not on a snapshotted port; pedal updates every 5 ms instead of
  20 ms. Its MIDI output is identical to V1.2 in simulation and it is estimated faster (see §1), but it
  still needs a session on the instrument before it can replace V1.2.
  Lessons for V2: behaviour-preserving changes need a test that compares outputs, and performance
  claims have to be checked against the compiled code, not the source.
- **Personal workarounds as defaults.** `PIANO_SIZE 85`, `FAULTY_LINE 16`, the author's specific
  pedal limits — a stranger flashing the reference firmware gets CyberGene's broken cable map.
- **Calibration requires re-flashing.** Three separate sketches, uploaded in sequence, plus manual
  trimpot work. Nothing is configurable at runtime and nothing is stored on the device.
- **An oversized lookup table.** 17 × 12187 bytes (~207 KB, most of a Teensy 3.6's 256 KB RAM). The
  curve reaches velocity 1 at ~5.6 ms, so more than half of every table is the constant 1. The table
  avoids a double-precision `log()` at note-on, which on a Teensy 3.6 is done in software (several µs,
  stalling the scan); on a Teensy 4.1 (double-precision FPU) or with `log10f` it is well under 1 µs.
  A ~5.6k-entry table, a small table with interpolation, or direct single-precision computation all
  work.
- **Magic constants with no recorded derivation.** `VEL_DISTANCE_FACTOR 1500.0` and `VEL_ADDITION 57.96`
  are redundant: the formula collapses to `v = m · (375.6 − 100 · log10(t_µs))`, i.e. one intercept and
  one slope. Nobody can adapt them to a different action geometry except by trial and error.
- **Engineering scaffolding — now partly in place.** There is a PlatformIO project (each sketch in its
  own folder, one environment per sketch), a GitHub Actions workflow that builds everything and uploads
  the `.hex` files, and host tests (`test/host`) that run the firmware against a simulation of the V1
  hardware as traced from the netlists (group lines, the shorted cable line, the shared bus, the pin and
  port mapping). The tests check the hardware (no two outputs or boards fighting), the piano behaviour
  above (one note per stroke, faster hammer never softer, repetition with the key held, aborted
  presses, rail rebounds, creeping presses), and a reviewed reference of V1.3's MIDI output, updated
  deliberately. Still missing: a changelog, releases, and a measurement of the real scan time.

### Repository & professionalism
- **No LICENSE file.** The README says "open-source", but legally the project isn't — nobody can
  safely reuse or fork it. This is a five-minute fix and the most important single item here.
- **53 MB of photos in git**, IDE config (`.idea/`) committed, KiCad autosave/backup files and
  generated gerbers committed alongside sources, file names with spaces and parentheses. (Moving the
  photos only stops the growth; shrinking the clone needs a history rewrite, which is optional.)
- **The README does five jobs at once** — concept, build guide, calibration manual, parts list,
  and raw email dumps — with `TBD` markers unchanged since 2020 and no BOM.
- **KiCad 5.0 (2018 format)** sources; current KiCad is several major versions ahead.
- **Build tooling quirk.** With the Arduino IDE / arduino-cli on Apple Silicon, the sketch prototype
  generator (`ctags`) is an Intel binary and needs Rosetta; the PlatformIO build does not.

## 4. The V2 decision: continuous sensing, software thresholds

V1's architecture is a smart workaround for one 2020 constraint: ADCs were too slow to scan 88 keys.
Nearly every V2-worthy pain — trimpots, fixed gates, drift, the giant calibration effort — is a
consequence of doing thresholding in *hardware*. Modern microcontrollers remove the constraint:
a $1–2 MCU samples a dozen or more analog channels at tens of kHz each. So:

> **Keep the CNY70-on-hammer-shank concept. Replace the comparator matrix with continuous per-key
> sampling and do detection, calibration, and voicing in software.**

**Timing check.** A fortissimo hammer head (~5 m/s) crosses the last ~2 mm in ~400 µs. The sensor sits
near the shank flange, so it sees the same *time* window but only a fraction of the *distance* (the
lever ratio, roughly 1/4–1/5, is to be measured). Sampling each key at ≥20 kHz gives 8+ samples in that
window; at 50–100 kHz with hardware oversampling (easy on the MCUs below) there are 20–40. Fitting a
line or curve through those samples should match V1's two-edge timing (9 µs quantization) — **if** the
signal-to-noise ratio is good enough.

**The real constraint is SNR, not speed.** Around the strike point the shank moves a few tenths of a
millimetre in front of the sensor. The CNY70 output rises steeply as the target approaches, but peaks at
a fraction of a millimetre and *falls* again closer in, so the mounting height must keep the strike
position on the monotonic side of the curve. The phototransistor's response time (tens of µs, slower
with larger load resistors) acts as a low-pass filter: a consistent lag is harmless for velocity, but it
has to be characterized. All of this is measurable in Phase 1 and decides the load resistor, ADC range,
and sample rate.

| V1 pain | V2 consequence of continuous sensing |
|---|---|
| 255–264 trimpots, manual calibration | Per-key auto-calibration: learn each key's rest/max reflection, store in flash; thresholds become percentages of the learned range |
| Drift (temperature, LED aging, supply) | Continuous re-baselining while the key is at rest |
| Fixed strike/escapement/damper gates | Arbitrary, per-key, runtime-adjustable thresholds; "voicing" from an app instead of a screwdriver |
| Velocity from 2 edges | Velocity from a fitted trajectory; bounce rejection; note-off (release) velocity; configurable curves per key |
| Comparator limits, no hysteresis | Gone — hysteresis and debouncing are software parameters |
| 18-drop IDC analog-ish bus | Short local analog runs + a simple digital bus |

## 5. Proposed V2 architecture (rough)

**Sensors — unchanged in spirit.** Keep the CNY70 (proven on this exact mechanical problem, cheap,
large sensing face) on tiny 3-wire satellite boards, outputting the raw phototransistor voltage. The LED
can stay on 5 V; only the emitter-load output must stay below the MCU's 3.3 V ADC reference, which is a
choice of load resistor. QRE1113/ITR8307 are fallbacks only if size, speed, or current demands it.

**Power budget — a first-class design item.** At V1's ~16 mA per LED, 88 sensors need ~1.4 A at 5 V.
A computer's USB port cannot supply that (500 mA for USB 2.0, 900 mA for USB 3.x; USB-C gives 1.5 A or
3 A only if the host advertises it). Options, to be chosen in Phase 1:
- lower LED current (e.g. 5 mA with a larger load resistor), if SNR allows;
- strobe each LED only around its own sample window (limited by the phototransistor's response time);
- a dedicated 5 V / 3 A supply (USB-C PD or DC jack) separate from the MIDI USB connection.

Either way: bulk capacitance per board, reverse-current protection between power inputs, and a design
where no board can back-feed an unpowered neighbour.

**Scanner boards — the new "note boards".** ~8 boards × 11 keys (instead of 18 × 5), each carrying a
small MCU that samples its 11 sensors, runs detection and velocity computation locally, and emits
compact events. The MCU needs **≥11 ADC inputs**, which rules out the RP2040 (4 ADC inputs) and RP2350
(4–8) unless external muxes are added. The **STM32G0 family** fits well (e.g. STM32G071: up to 16
external channels, 12-bit at 2.5 MS/s, hardware oversampling, and a ROM bootloader) and is cheap and supported by
assembly services. Analog paths shrink from "across the piano" to <10 cm.

**Interconnect.** Half-duplex RS-485 (or CAN-FD with an STM32G0B1/G4). Because each board computes its
own velocities, **no clock synchronization is needed**: the hub timestamps events on arrival, and a
polling interval of ~100 µs is far below audible latency. Position-based auto-addressing needs a
daisy-chained line (ADDR_IN → ADDR_OUT) in addition to the bus pair, so plan a **6-wire** locking cable
(A, B, ADDR, 5 V, GND, GND) instead of 4 — or keep jumper/DIP addressing. Run power on wires sized for
the LED current, not on the signal cable's thinnest conductors. An event is ~8 bytes {key, type,
velocity, timestamp}; even a glissando is trivial bandwidth.

**Latency budget (target).** Key-to-MIDI under ~2 ms end to end: detection (<0.5 ms after the strike
point), bus (<0.2 ms), hub processing, and USB-MIDI (1 ms frames at full speed).

**Hub board.** Teensy 4.1 (most mature USB-MIDI stack) or an STM32/RP2040 with TinyUSB. USB-C for
MIDI; power as above; one clean 5 V rail on the bus with a local 3.3 V LDO per board (this replaces the
breadboard regulator and the ripple problem); three pedal jacks with configurable curves; status LED;
optional DIN-5 MIDI out.

**Firmware.** One PlatformIO monorepo: `firmware/hub`, `firmware/scanner`, and a shared protocol +
velocity/calibration library written as portable C++ so the math runs under unit tests on the host.
GitHub Actions builds every commit; releases ship flashable artifacts. All configuration lives in
flash and is exposed over SysEx — flashing is never part of calibration again.

The V1 test approach carries over and gets stronger: the piano-behaviour checks become the
specification of the detection code, and in V2 the inputs are *recorded hammer trajectories* from the
Phase 1 rig instead of scripted zones, replayed through the scanner firmware on the host. Every
firmware image also includes a diagnostic that reports scan/sample timing measured with the CPU cycle
counter, so timing is measured on the device rather than estimated.

**Companion app.** Browser-based (WebMIDI SysEx — zero install; WebSerial only for firmware update,
since it is Chromium-only and Safari has no WebMIDI): live per-key position waveform, an auto-calibration
wizard that encodes the README's manual procedure, per-key velocity curve editor seeded with V1.2's
voicing, MIDI monitor, firmware update. This plays directly to the author's software-engineering
strengths and is the piece that will make V2 feel professional to other builders.

**MIDI features.** Note-on velocity (per-key + global curves), note-off velocity, CC64 half-pedal plus
two more pedals, channel configuration. Stretch: high-resolution velocity (CC#88) or MIDI 2.0 once
host support is worth it.

### Alternatives considered

- **Central ADC + analog muxes** (one Teensy, passive note boards): simpler, one firmware image, but it
  reintroduces long analog runs across 1.2 m of piano — exactly what V1's comparators were designed to
  avoid.
- **"V1.5": keep V1, replace trimpots with digitally set thresholds** (a multi-channel DAC or filtered
  PWM per note board). This removes manual trimpot work and enables software calibration with the
  proven timing path unchanged. But it keeps the binary three-point sensing, the IDC cable, the
  comparator limits and 72 comparator chips, and it needs a control bus to each board anyway. It is the
  lower-risk option if Phase 1 shows the CNY70 SNR is marginal for continuous sensing.

The distributed design is the honest fix. Cost is likely *lower* than V1, not just neutral: 264
multi-turn trimpots cost more than eight small MCUs, and ~72 comparators, ~36 transceivers and the
jumper headers disappear.

## 6. What deliberately stays

Hammer-shank optical sensing; CNY70 satellites; modular boards sized around action-bracket gaps;
85- and 88-key support; the calibration *procedure* (as software); the graded velocity concept and
V1.2's voicing (as per-key curves).

## 7. Roadmap

**Phase 0 — make V1 presentable (no redesign; roughly a weekend).**
*Done:* V1.3 regressions fixed and scan reworked; PlatformIO build; sketches in their own folders; host
simulation tests with piano-behaviour checks and a reviewed reference output; GitHub Actions building
all sketches and publishing `.hex` artifacts.
*Remaining:* Add a license (suggestion: CERN-OHL-P for hardware, MIT for firmware, CC-BY-4.0 for
docs/photos) — without this nothing is actually open source. Tag the tested state (V1.2) as `v1.0` and
create a GitHub release. On the instrument: play-test V1.3 against V1.2 and measure the real scan time
with a cycle-counter benchmark build (raise `MATRIX_SETTLING_DELAY_NS` if ghost notes appear); then
promote V1.3 or keep it marked experimental. Make its defaults generic (`FAULTY_LINE 0`, 88 keys) with
the author's settings in a separate config block. Restructure: `hardware/` (KiCad sources only; gerbers
become release artifacts), `firmware/`, `docs/` (README split into concept / build / calibration
pages), photos resized into `docs/images/`. Write the BOM that has been "TBD" since 2020. Document the
J1-J2-J5-J3-J4 connector order and the power findings above. Optionally, measure the real supply
current and the strike-point trimpot voltages.

**Phase 1 — feasibility rig (one octave).**
Prototype one scanner board (MCU + 11 CNY70 channels) mounted beside the existing V1 electronics on
the same action. Record raw trajectories and measure: displacement at the sensor around the strike
point, SNR vs LED current and load resistor, phototransistor response time, and the CNY70 peak
position relative to the strike point. Validate velocity extraction against V1's output on the same
strikes and against a repeatable reference (e.g. a weight dropped from fixed heights onto the key),
and prove auto-calibration. Pick the MCU, ADC arrangement, LED current and sampling rate from this data
rather than from guesses. Exit criteria: repeatability on the reference within ±1 MIDI step, no
systematic bias against V1 beyond what a curve fit removes, a key that calibrates itself, and a power
figure per key that fits the chosen supply.

**Phase 2 — full-system alpha.**
Eight scanner boards + hub (rev A), bus protocol with auto-addressing, calibration in flash, minimal web
app (live view, auto-cal wizard, curve editor). Replace V1 on the instrument and play it daily; iterate
on the data.

**Phase 3 — community release.**
KiCad 8/9 sources, fab + assembly-service outputs (gerbers, BOM, CPL for e.g. JLCPCB) so nobody has
to hand-solder 0603 parts, interactive BOM, mounting drawings (DXF/STEP) for the sensor rail,
assembly & calibration guide, `v2.0` release. Stretch: note-off velocity voicing, MIDI 2.0, optional
damper/key sensing add-ons.

## 8. Risks & open questions

- **SNR at the strike point** — the biggest technical risk (see §4). Mitigations: optimise mounting
  height and load resistor, oversampling, or fall back to the V1.5 alternative.
- **CNY70 spread and nonlinearity** — handled by per-key normalization and a per-key curve learned in
  calibration.
- **Power** — ~1.4 A of LED current at V1's settings; must be solved by design, not by the host's USB
  port.
- **Optical crosstalk / ambient IR** — V1's always-on LEDs worked inside the piano, so likely a
  non-issue; modulated LED drive is the fallback if not.
- **Firmware updates across 9 MCUs** — needs a bus bootloader or hub-proxied update (the STM32 ROM
  bootloader helps); decide in Phase 2, design the protocol for it in Phase 1.
- **Simulation vs. the real bus** — V1.3's timing and behaviour are verified only in simulation and by
  estimate; the real cable's settling time is known only indirectly (V1.2's four dummy reads). The
  play-test and on-device measurement in Phase 0 close this gap.
- **Scope discipline** — each phase ends in something usable (a clean V1 repo, a validated octave, a
  playable alpha, a shareable release), so the project can pause at any phase without being wasted.

## 9. Revision notes

**Second revision** — after deriving the scan time from the compiled firmware and reworking V1.3:

- **Scan speed:** added the emulator-based estimates for V1.2 and V1.3 on both boards; the recorded
  8.9 µs fits only a Teensy 4.1; timing jitter is shown to be inaudible either way.
- **Firmware:** V1.3 was 2× slower than V1.2 because GCC did not inline the per-key function; it is now
  pipelined with port snapshots, compile-time waits and a dead time, estimated at 3.6 µs typical
  (V1.2: 5.1 µs). Pedal updates every 5 ms instead of 20 ms.
- **V1.2 as reference:** V1.2 is the author's first firmware, not a specification. Its detection logic
  was checked against grand-action mechanics (added to §2); tests now check that behaviour and a
  reviewed reference output instead of "identical to V1.2". The bass voicing is kept because the
  trimpots were calibrated by ear together with it.
- **Cable:** empirical settling time (~40–110 ns), the missing dead time and the all-groups-enabled
  first scan added to §3.
- **Tooling:** PlatformIO, CI and host tests are in place (Phase 0 progress); V2 firmware inherits the
  test approach with recorded trajectories and an on-device timing diagnostic.

**First revision** — after tracing the KiCad netlists and PCB copper and comparing the two firmware
versions line by line:

- **Power:** corrected "~2 W" to ~7 W (from 240 Ω LED resistors); added the USB power limits, missing bulk
  capacitance, unprotected dual 5 V inputs, and back-feeding when the Teensy is unpowered.
- **New V1 hardware issues:** LM339 common-mode limit vs 0–5 V trimpots, no comparator hysteresis, a
  single ground in the ribbon, and the undocumented J1-J2-J5-J3-J4 order. The connector is *rotated*,
  not mirrored; pin numbers map 1:1.
- **Firmware:** the V1.3 regressions (faulty-line contention, lost bass voicing, treble clamp) are listed
  and are now fixed; "monotonicity enforcement" is dropped from the strengths (the curve is already
  monotonic, so it was dead code); the log()-cost claim is corrected for the Teensy 3.6; the velocity
  constants are shown to be redundant; the table is shown to be over half constant; the performance
  figures are attributed to V1.2.
- **V2 architecture:** the sensor sits near the shank flange, so the design limit is SNR and CNY70
  curve shape, not sample rate. The RP2040 is replaced (only 4 ADC inputs) by the STM32G0. Clock sync is
  removed (velocity is computed on the scanner board). Auto-addressing needs an extra daisy-chain line,
  so the bus is 6 wires. Added a latency budget, WebMIDI/WebSerial browser limits, a "V1.5" alternative,
  and a reference-based Phase 1 exit criterion in place of "±1 step of V1".
