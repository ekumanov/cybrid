// Host-side stand-in for the Teensy/Arduino APIs used by the Cybrid sketches.
//
// Instead of real hardware it models the V1 electronics as traced from the KiCad netlists:
// 18 active-low group lines on the IDC cable (with an optional line shorted to line 7, as in
// CyberGene's cable), one 5-key note board per line, and the 15 shared comparator inputs.
// Hammer positions come from per-key scripted timelines (see sim.cpp).
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

typedef uint8_t byte;
typedef bool boolean;

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define A16 40

// Hardware configuration of the simulated instrument.
#ifndef SIM_PIANO_KEYS
#define SIM_PIANO_KEYS 85
#endif
#ifndef SIM_SHORTED_LINE
#define SIM_SHORTED_LINE 16 // physical line shorted to line 7 in the cable; 0 = healthy cable
#endif

namespace sim {

// Simulated time. The harness advances it once per loop() call, so every firmware
// version sees exactly the same hammer states and the outputs are comparable.
extern uint64_t nowUs;

// Hammer zones, from furthest to closest to the stop rail.
enum Zone { REST = 0, DAMPER = 1, ESCAP = 2, STRIKE = 3 };
Zone zoneOf(int midiNote);

// Physical cable line (1-18) -> Teensy pin, from the Teensy PCB netlist.
const int LINE_PIN[19] = {-1, 35, 36, 37, 38, 39, 14, 15, 16, 17, 8, 7, 6, 5, 4, 3, 2, 1, 0};

// Teensy input pin -> (key slot 0-4 within a group, zone that pulls it LOW), traced from
// sensor connector through comparator, transceiver and IDC cable on the PCBs.
struct PinRole { int slot; Zone zone; };
inline bool pinRole(int pin, PinRole &r) {
  switch (pin) {
    case 32: r = {0, STRIKE}; return true; case 31: r = {0, ESCAP}; return true; case 30: r = {0, DAMPER}; return true;
    case 29: r = {1, STRIKE}; return true; case 27: r = {1, ESCAP}; return true; case 28: r = {1, DAMPER}; return true;
    case 25: r = {2, STRIKE}; return true; case 24: r = {2, ESCAP}; return true; case 26: r = {2, DAMPER}; return true;
    case 34: r = {3, STRIKE}; return true; case 33: r = {3, ESCAP}; return true; case  9: r = {3, DAMPER}; return true;
    case 10: r = {4, STRIKE}; return true; case 11: r = {4, ESCAP}; return true; case 12: r = {4, DAMPER}; return true;
  }
  return false;
}

// Musical group (0-17) jumpered on the note board of each physical line, -1 = no board.
// Boards skip the shorted line, exactly as the V1.2 firmware expects.
inline int boardGroupOnLine(int line) {
  int g = 0;
  for (int l = 1; l <= 18; l++) {
    if (l == SIM_SHORTED_LINE) { if (l == line) return -1; continue; }
    if (g * 5 >= SIM_PIANO_KEYS) return -1;
    if (l == line) return g;
    g++;
  }
  return -1;
}

extern int pinMode_[64];
extern int pinLevel[64];
extern long lineFights;   // two Teensy outputs driving the same wire to different levels
extern long busFights;    // more than one note board enabled on the shared input bus

// Level of a physical wire: a line and the line shorted to it are the same wire.
inline int wireLevel(int line) {
  int lines[2] = {line, 0};
  if (SIM_SHORTED_LINE && line == 7) lines[1] = SIM_SHORTED_LINE;
  if (SIM_SHORTED_LINE && line == SIM_SHORTED_LINE) lines[1] = 7;
  int driven = -1;
  for (int l : lines) {
    if (!l) continue;
    int p = LINE_PIN[l];
    if (pinMode_[p] != OUTPUT) continue;
    if (driven >= 0 && driven != pinLevel[p]) { lineFights++; return LOW; }
    driven = pinLevel[p];
  }
  return driven < 0 ? HIGH : driven; // undriven: pulled HIGH by the boards' OE pull-ups
}

inline int readInput(int pin) {
  PinRole r;
  if (!pinRole(pin, r)) return HIGH;
  // Every enabled note board drives this bus line; several boards are only harmless
  // (and readable) while they all drive the same level.
  int level = -1;
  for (int l = 1; l <= 18; l++) {
    int g = boardGroupOnLine(l);
    if (g < 0 || wireLevel(l) != LOW) continue;
    int note = 21 + g * 5 + r.slot;
    int out = (note > 20 + SIM_PIANO_KEYS) ? HIGH              // unpopulated slot: pull-up
              : (zoneOf(note) >= r.zone ? LOW : HIGH);         // comparators are active LOW
    if (level >= 0 && level != out) { busFights++; return LOW; }
    level = out;
  }
  return level < 0 ? HIGH : level; // bus idle: INPUT_PULLUP
}

struct MidiEvent { uint64_t t; int type; int a; int b; }; // type 0 = note, 1 = CC
extern std::vector<MidiEvent> events;

} // namespace sim

// --- Arduino / Teensy API ---------------------------------------------------

inline void pinMode(int pin, int mode) { sim::pinMode_[pin] = mode == OUTPUT ? OUTPUT : INPUT; }
inline void digitalWrite(int pin, int v) { sim::pinLevel[pin] = v ? HIGH : LOW; }
inline void digitalWriteFast(int pin, int v) { sim::pinLevel[pin] = v ? HIGH : LOW; }
inline int digitalReadFast(int pin) { return sim::readInput(pin); }
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned long) {}
inline void delayNanoseconds(unsigned long) {}

struct elapsedMicros {
  uint64_t start = sim::nowUs;
  operator unsigned long() const { return (unsigned long)(sim::nowUs - start); }
  elapsedMicros &operator=(unsigned long v) { start = sim::nowUs - v; return *this; }
};
struct elapsedMillis {
  uint64_t start = sim::nowUs;
  operator unsigned long() const { return (unsigned long)((sim::nowUs - start) / 1000); }
  elapsedMillis &operator=(unsigned long v) { start = sim::nowUs - v * 1000; return *this; }
};

struct UsbMidi {
  void sendNoteOn(int note, int vel, int) { sim::events.push_back({sim::nowUs, 0, note, vel}); }
  void sendControlChange(int cc, int val, int) { sim::events.push_back({sim::nowUs, 1, cc, val}); }
  void send_now() {}
};
extern UsbMidi usbMIDI;

struct SerialStub { template <class T> void print(T) {} template <class T> void println(T) {} void println() {} void begin(long) {} };
extern SerialStub Serial;
