# Changelog

Notable changes to Cybrid. Firmware releases are numbered V1.2, V1.3, and so on.

## Unreleased

### Repository
- Restructured into versioned folders: `hardware/v1`, `firmware/v1`, `docs/v1`.
- Added licenses: CERN-OHL-P-2.0 (hardware), MIT (software), CC-BY-4.0 (docs and photos).
- README split into a project overview plus How it works, Build guide, Calibration, Firmware,
  Hardware and Gallery pages.
- Photos resized, renamed by subject and stripped of metadata (53 MB → 7 MB).
- CI: firmware workflow runs from `firmware/v1`; new hardware workflow exports schematics, BOM,
  Gerbers, renders and ERC/DRC reports with KiCad 10.

### Hardware V1
- KiCad 5.0 projects migrated to KiCad 10 and renamed (`teensy-board`, `note-board`,
  `sensor-board`). Connectivity verified identical to the original design.
- Component values written into the schematics; unfitted Teensy-board resistors marked DNP.
- 3D models and real board colours (white mask, black silk) for 3D views and renders.
- Generated files removed from the repository; `hardware/v1/export.sh` regenerates them.
- Bill of materials written.

## Firmware V1.3 (2026, not yet play-tested)
- Pipelined scan with GPIO port snapshots and compile-time settling and dead-time waits.
- Regressions against V1.2 fixed; MIDI output identical to V1.2 in host simulation.
- PlatformIO build, host simulation tests and GitHub Actions.

## Firmware V1.2 (2020)
- The firmware the instrument has been played and voiced with. Teensy 4.1 support.

## Hardware V1 (2019–2020)
- Sensor, note and Teensy boards designed in KiCad 5 and built into the instrument.
