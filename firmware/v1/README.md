# Cybrid V1 firmware

Teensy firmware for the V1 hardware: it scans the 18 note boards, turns hammer flight times into MIDI
velocities and sends USB MIDI. How the scan works is explained in
[How it works](../../docs/v1/how-it-works.md#scanning-algorithm).

| Sketch | Folder | Status |
|---|---|---|
| **V1.3** main firmware | [`src/main/My_Hybrid_Piano_V1.3`](src/main/My_Hybrid_Piano_V1.3) | Current. Faster scan; identical MIDI output to V1.2 in simulation. **Not yet play-tested on the instrument.** |
| **V1.2** main firmware | [`src/main/My_Hybrid_Piano_V1.2`](src/main/My_Hybrid_Piano_V1.2) | The version the instrument has been played and voiced with. |
| Calibration, point 1/2/3 | [`src/calibration/Calibrate_Sensor_N`](src/calibration) | Helpers for setting the trimpots, see [Calibration](../../docs/v1/calibration.md). |

Each sketch is a plain Arduino sketch in its own folder, so it opens directly in the Arduino IDE.

## Configure before flashing

The defaults match **my** instrument. In the main sketch, check:

- `PIANO_SIZE`: number of keys (mine: 85).
- `FAULTY_LINE`: works around a shorted line in my ribbon cable (16). Set it to `0` for a healthy
  cable.
- The half-pedal pin and calibration limits (`HALF_PEDAL`): A16 on a Teensy 4.1, A21 on a Teensy
  3.6.

## Building and flashing

**Arduino IDE** with [Teensyduino](https://www.pjrc.com/teensy/td_download.html): open the sketch's
`.ino`, select your Teensy, and for the main firmware set *Tools > USB Type* to *MIDI*.

**[PlatformIO](https://platformio.org/)** (all commands from this folder, `firmware/v1`):

```bash
pio run                      # build all sketches for Teensy 4.1
pio run -e v1_3 -t upload    # build and flash the main firmware
pio run -e v1_3_teensy36     # V1.3 for a Teensy 3.6
pio run -e calibrate_1 -t upload
pio device monitor           # serial output of the calibration sketches
```

Prebuilt `.hex` files for every sketch are attached to each GitHub Actions run
([Firmware workflow](../../.github/workflows/firmware.yml)); flash them with the
[Teensy Loader](https://www.pjrc.com/teensy/loader.html).

## Tests

`test/host/run.sh` builds the firmware on your computer against a simulation of the V1 hardware
(traced from the board netlists: group lines, the shared bus, the shorted cable line, the pin and port
mapping) and plays scripted hammer motions through it. It checks that:

- no two outputs ever drive the cable or the bus against each other;
- the firmware behaves like a grand piano action: one note per stroke, a faster hammer is never
  softer, repetition with the key held, aborted presses, rail rebounds;
- the MIDI output matches a reviewed reference (`test/host/expected/v1_3.txt`), so any change in
  behaviour shows up as a diff. Accept an intended change with
  `UPDATE_EXPECTED=1 test/host/run.sh` and commit the new reference.

```bash
test/host/run.sh             # needs a C++17 compiler
```

GitHub Actions runs the tests and builds every sketch on each push.
