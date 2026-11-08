/**
 * CyberGene's DIY Hybrid Piano Controller - FIRMWARE v1.3 (MASTER REFERENCE)
 * TARGET: Teensy 4.1 @ 600 MHz
 * -------------------------------------------------------------
 * COMPILER OPTIMIZATION FLAGS (PlatformIO/Arduino IDE):
 * -------------------------------------------------------------
 * Add these to platformio.ini or Arduino IDE compiler settings:
 *
 * build_flags =
 *     -O3                    ; Maximum optimization (speed priority)
 *     -mcpu=cortex-m7        ; Target ARM Cortex-M7 architecture
 *     -mfloat-abi=hard       ; Use hardware FPU for floating-point ops
 *
 * These flags enable:
 * - Aggressive inlining and loop optimization
 * - SIMD instructions where applicable
 * - Hardware floating-point for velocity calculations
 * - Branch prediction optimization
 *
 * -------------------------------------------------------------
 * PERFORMANCE METRICS:
 * -------------------------------------------------------------
 * - Measured by CyberGene with V1.2: full keyboard scan ~8.9µs
 *   (~0.5µs per group, ~112 kHz scan rate), velocity error at
 *   MIDI 127 under 1 step.
 * - This version (not yet measured): estimated ~3-5µs per full
 *   scan on Teensy 4.1 (~3.6µs typical), ~10-15µs on Teensy 3.6
 *   @ 256 MHz, from emulating the compiled loop() and published
 *   GPIO access times. Each group still gets >= 100ns to settle.
 * -------------------------------------------------------------
 * PHYSICS & DETECTION PRINCIPLE (CNY70 + Comparators):
 * -------------------------------------------------------------
 * This controller detects hammer position using reflective optical
 * proximity sensors (CNY70) paired with voltage comparators.
 *
 * * THE SENSOR LOGIC (Active LOW):
 * 1. As the hammer rises, it reflects IR light back into the CNY70.
 * 2. When the hammer gets CLOSER than a specific distance (set by a
 * trimpot threshold on the PCB), the comparator switches output to LOW.
 * 3. Therefore:
 * - HIGH (3.3V) = Hammer is FAR (Reflector out of range).
 * - LOW  (0V)   = Hammer is NEAR (Reflector within range).
 *
 * * THE CHECK POINTS (Sequential Activation):
 * As the hammer travels upward from Rest to Strike, it sequentially
 * enters the detection range of three sensors. We rely on this sequence
 * to determine direction and velocity.
 *
 * 1. DAMPER POINT (Mid-Travel):
 * - Physical: The point where the damper would lift off the string.
 * - Logic: First sensor to trigger (LOW) as hammer rises.
 * Last sensor to release (HIGH) as hammer falls.
 *
 * 2. ESCAPEMENT POINT (Upper Travel):
 * - Physical: Approx 2-3mm below the stop rail.
 * - Logic: Activates second. This is our "Start Line" for velocity.
 *
 * 3. STRIKE POINT (Top / Velocity End):
 * - Physical: Slightly below the stop rail (e.g. 0.5mm gap).
 * We place it slightly below to ensure the hardware detects the
 * hammer *before* it physically stops dead against the felt.
 * - Logic: Activates last. This is our "Finish Line".
 * -------------------------------------------------------------
 */

#include <ADC.h>

// Forced inline: the pin numbers must reach digitalReadFast() as compile-time constants
// (otherwise every read goes through a pin lookup table, doubling the scan time), and
// the per-key logic must not pay a function call per key.
struct InputSnapshot;
static inline __attribute__((always_inline)) void readGroupInputs(InputSnapshot &s);
static inline __attribute__((always_inline)) void processGroup(int g, const InputSnapshot &s);
static inline __attribute__((always_inline)) void checkHammerState(byte note, byte curve, bool atStrike, bool atEscap, bool atDamper);
static inline __attribute__((always_inline)) void waitCyclesSince(uint32_t start, uint32_t cycles);

// =============================================================
// --- USER CONFIGURATION --------------------------------------
// =============================================================

// PIANO SIZE: 85 (A0-A7) or 88 (A0-C8)
#define PIANO_SIZE 85

/**
 * FAULTY CABLE LINE WORKAROUND:
 * -------------------------------------------------------------
 * A hardware abstraction to skip ONE specific faulty line on the ribbon cable.
 * * IMPORTANT CONSTRAINTS:
 * 1. This is ONLY available for 85-key pianos (which leave 1 line unused).
 * 2. It can only handle ONE faulty line.
 * * SETTING:
 * * 0   : NORMAL OPERATION (All lines functional).
 * * 1-18: The physical index of the faulty line to skip.
 * (e.g., 16 = Skip physical Line 16).
 */
#define FAULTY_LINE 16

/**
 * MATRIX SETTLING TIME (Nanoseconds):
 * -------------------------------------------------------------
 * Wait between ENABLING a group and reading its 15 lines.
 * * HARDWARE CONTEXT:
 * - Groups use SN74LVC245A Transceivers (Active Push-Pull).
 * - Shared 1.5m IDC ribbon cable with 17-18 connectors (stubs).
 * * WHY THE DELAY IS NEEDED:
 * The newly enabled transceiver must drive the long cable to a clean level;
 * reflections from the connector stubs take several round trips to die out.
 * Reading too early gives stale or wrong levels ("ghost notes").
 * * TUNING: V1.2 worked with ~40-110ns (4 dummy reads); 100ns adds margin.
 * Increase if ghosting occurs.
 * * COST: usually none. The previous group is processed while this one settles
 * (see loop()); the wait only covers whatever part of this time is left over.
 */
#define MATRIX_SETTLING_DELAY_NS 100

/**
 * MATRIX DEAD TIME (Nanoseconds):
 * -------------------------------------------------------------
 * Gap between DISABLING one group and ENABLING the next, so two transceivers
 * never drive the shared lines at the same time. The SN74LVC245A needs up to
 * ~8ns to release its outputs (tPHZ/tPLZ at 3.3V); 20ns covers that plus skew.
 */
#define MATRIX_DEAD_TIME_NS 20

// Both waits in CPU cycles, computed at compile time (F_CPU is the configured CPU clock).
#define NS_TO_CYCLES(ns) ((uint32_t)(((uint64_t)F_CPU * (ns) + 999999999ULL) / 1000000000ULL))
#define MATRIX_SETTLING_CYCLES NS_TO_CYCLES(MATRIX_SETTLING_DELAY_NS)
#define MATRIX_DEAD_TIME_CYCLES NS_TO_CYCLES(MATRIX_DEAD_TIME_NS)

// PEDAL SCAN INTERVAL:
// 5ms = 200Hz at most, and only while the pedal moves (see the noise gate in processPedal).
// Limits CC64 traffic while keeping pedal latency low: pedalling is timed against
// key releases, so e.g. a 20ms interval (10ms average delay) is noticeable in legato pedalling.
#define PEDAL_SCAN_INTERVAL_MS 5

// =============================================================

// --- CHECK POINT PIN DEFINITIONS ---
// Naming corresponds to the physical location of the Check Point.
#define N1_STRIKE   32  // Point 1: Closest proximity (Top)
#define N1_ESCAP    31  // Point 2: Medium proximity
#define N1_DAMPER   30  // Point 3: Far proximity (Middle)

#define N2_STRIKE   29
#define N2_ESCAP    27
#define N2_DAMPER   28

#define N3_STRIKE   25
#define N3_ESCAP    24
#define N3_DAMPER   26

#define N4_STRIKE   34
#define N4_ESCAP    33
#define N4_DAMPER   9

#define N5_STRIKE   10
#define N5_ESCAP    11
#define N5_DAMPER   12

// --- INPUT PORT SNAPSHOT ---
// The 15 check point pins sit on a few 32-bit GPIO port registers (which ones is taken
// from the Teensy core's CORE_PINn_PINREG / CORE_PINn_BITMASK). Reading those registers
// once per group captures the whole group at one instant with a few reads instead of 15,
// so it can be processed later, while the next group settles (see loop()).
#if defined(__IMXRT1062__)
  #define INPUT_PORTS(X) X(GPIO6_PSR) X(GPIO7_PSR) X(GPIO8_PSR) X(GPIO9_PSR)
  #define INPUT_PORT_COUNT 4
#else
  #define INPUT_PORTS(X) X(GPIOA_PDIR) X(GPIOB_PDIR) X(GPIOC_PDIR) X(GPIOD_PDIR) X(GPIOE_PDIR)
  #define INPUT_PORT_COUNT 5
#endif

struct InputSnapshot { uint32_t port[INPUT_PORT_COUNT]; };

#define PINREG_ADDR(pin) PINREG_ADDR_(pin)
#define PINREG_ADDR_(pin) ((const volatile void *)&CORE_PIN##pin##_PINREG)
#define PIN_BITMASK(pin) PIN_BITMASK_(pin)
#define PIN_BITMASK_(pin) (CORE_PIN##pin##_BITMASK)

// All address comparisons below are between constants and fold away at compile time.
static inline __attribute__((always_inline)) bool isCheckPointPort(const volatile void *reg) {
  return reg == PINREG_ADDR(N1_STRIKE) || reg == PINREG_ADDR(N1_ESCAP) || reg == PINREG_ADDR(N1_DAMPER)
      || reg == PINREG_ADDR(N2_STRIKE) || reg == PINREG_ADDR(N2_ESCAP) || reg == PINREG_ADDR(N2_DAMPER)
      || reg == PINREG_ADDR(N3_STRIKE) || reg == PINREG_ADDR(N3_ESCAP) || reg == PINREG_ADDR(N3_DAMPER)
      || reg == PINREG_ADDR(N4_STRIKE) || reg == PINREG_ADDR(N4_ESCAP) || reg == PINREG_ADDR(N4_DAMPER)
      || reg == PINREG_ADDR(N5_STRIKE) || reg == PINREG_ADDR(N5_ESCAP) || reg == PINREG_ADDR(N5_DAMPER);
}

// Index of a port register in the snapshot, or -1 if it is not one of INPUT_PORTS.
static inline __attribute__((always_inline)) int snapshotIndex(const volatile void *reg) {
  int i = 0;
  #define SNAPSHOT_INDEX(r) if (reg == (const volatile void *)&(r)) return i; i++;
  INPUT_PORTS(SNAPSHOT_INDEX)
  #undef SNAPSHOT_INDEX
  return -1;
}

// Build-time check: every check point pin must be on a snapshotted port, otherwise its
// input would silently read as "hammer at rest". If this fails, the build stops with an
// undefined reference to the function below; add the missing port to INPUT_PORTS.
void ERROR_check_point_pin_not_on_an_INPUT_PORTS_register();
#define CHECK_ON_INPUT_PORT(pin) \
  if (snapshotIndex(PINREG_ADDR(pin)) < 0) ERROR_check_point_pin_not_on_an_INPUT_PORTS_register();

// True when the hammer is within check point `pin` (comparators are active LOW).
#define AT_POINT(s, pin) (((s).port[snapshotIndex(PINREG_ADDR(pin))] & PIN_BITMASK(pin)) == 0)

// --- PERIPHERALS ---
#define HALF_PEDAL  A16 // Connected to Kawai 10H (Pin 40 on T4.1)
#define LED         13

// =============================================================
// --- VELOCITY CURVE & PHYSICS CONSTANTS ----------------------
// =============================================================

/**
 * VEL_MAP_LENGTH:
 * The maximum time (in microseconds) we allow for the hammer to travel
 * between Escapement and Strike.
 * - 12187us (12ms) is roughly the slowest a hammer can move and still
 * create a sound. Anything slower is considered a silent press.
 */
#define VEL_MAP_LENGTH 12187

/**
 * VEL_DISTANCE_FACTOR:
 * A physics constant representing the effective distance between the
 * Escapement Point and Strike Point (approx 2mm), normalized for the
 * logarithmic formula. It scales the raw time (t) into a dimensionless
 * velocity ratio.
 */
#define VEL_DISTANCE_FACTOR 1500.0

/**
 * VEL_LOG_MULTIPLIER:
 * This scales the result of the log() function to fit the MIDI 1-127 range.
 * Formula: MIDI = Multiplier * log(Distance / Time)
 * Increasing this makes the velocity curve "steeper" (more dynamic range).
 */
#define VEL_LOG_MULTIPLIER 100.0

/**
 * VEL_ADDITION:
 * An offset added to the result to shift the curve up or down.
 * This ensures that even the lightest possible touch (max time) still
 * maps to at least MIDI velocity 1, rather than 0 or negative.
 */
#define VEL_ADDITION 57.96

/**
 * VELOCITY_MAP_STRETCH (Grading Factor):
 * Simulates "Graded Hammer Action" found in acoustic grands.
 * - In a real piano, bass hammers are heavier/slower than treble hammers
 * for the same input force.
 * - 0.4 spreads the VEL_GRADE_STEPS curves from a multiplier of 1.2
 * (curve 0, boosting the signal) down to 0.8 (last curve, attenuating it).
 * - Which curve each group uses is set by GROUP_VELOCITY_CURVE below.
 * - This ensures a consistent "feel" across the keyboard.
 */
#define VELOCITY_MAP_STRETCH 0.4

// Number of graded velocity curves (the multiplier range above is split into this many steps).
#define VEL_GRADE_STEPS 17

/**
 * GROUP_VELOCITY_CURVE:
 * Velocity curve used by each logical group (A0, D1, G1, ... C8), hand-voiced
 * on the instrument in V1.2. The lowest groups deliberately share curves 4-6
 * (multiplier 1.1-1.05) instead of 0-3, which made the bass too loud.
 * The 18th entry is only used by the C8 group of an 88-key piano.
 */
const byte GROUP_VELOCITY_CURVE[18] = {4, 4, 5, 6, 6, 6, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 16};

// PEDAL CALIBRATION (Analog 0-255 range)
#define HIGH_PEDAL_LIMIT 192 // Pedal fully UP
#define LOW_PEDAL_LIMIT 85   // Pedal fully DOWN

// =============================================================

// --- HARDWARE ABSTRACTION ---
const int ALL_CABLE_LINES[18] = {35, 36, 37, 38, 39, 14, 15, 16, 17, 8, 7, 6, 5, 4, 3, 2, 1, 0};

#if PIANO_SIZE == 85
  #define LOGICAL_GROUPS_NEEDED 17
#else
  #define LOGICAL_GROUPS_NEEDED 18
#endif

// Configuration Safety Check
#if (PIANO_SIZE == 88) && (FAULTY_LINE > 0)
  #error "CONFIGURATION ERROR: You cannot use FAULTY_LINE with an 88-key piano. All 18 lines are required."
#endif

// --- STATE MANAGEMENT ARRAYS ---
elapsedMicros hammerTimer[128];        // Measures flight time (Escapement -> Strike)
bool isMeasuringVelocity[128];         // State: Hammer is in the "Flight Zone"
bool strikeDetected[128];              // State: Hammer has hit rail (Debounce latch)
bool noteIsActive[128];                // State: Note is currently sounding

// Lookup Tables
byte velocityMap[VEL_GRADE_STEPS][VEL_MAP_LENGTH];
byte pedalCcTable[256];
int groupLineMap[LOGICAL_GROUPS_NEEDED]; // Logic Group -> Physical Pin Map

// Pedal Throttling
elapsedMillis pedalUpdateTimer;
byte lastPedalMidiValue = 0;
byte lastPedalVoltage = 0;

ADC *adc = new ADC();

void setup() {
  pinMode(LED, OUTPUT);

  CHECK_ON_INPUT_PORT(N1_STRIKE) CHECK_ON_INPUT_PORT(N1_ESCAP) CHECK_ON_INPUT_PORT(N1_DAMPER)
  CHECK_ON_INPUT_PORT(N2_STRIKE) CHECK_ON_INPUT_PORT(N2_ESCAP) CHECK_ON_INPUT_PORT(N2_DAMPER)
  CHECK_ON_INPUT_PORT(N3_STRIKE) CHECK_ON_INPUT_PORT(N3_ESCAP) CHECK_ON_INPUT_PORT(N3_DAMPER)
  CHECK_ON_INPUT_PORT(N4_STRIKE) CHECK_ON_INPUT_PORT(N4_ESCAP) CHECK_ON_INPUT_PORT(N4_DAMPER)
  CHECK_ON_INPUT_PORT(N5_STRIKE) CHECK_ON_INPUT_PORT(N5_ESCAP) CHECK_ON_INPUT_PORT(N5_DAMPER)

  // The matrix timing uses the CPU cycle counter (already running on Teensy 4.x).
  ARM_DEMCR |= ARM_DEMCR_TRCENA;
  ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;

  // Initialize Comparator Input Pins
  int checkPointPins[] = {32,31,30,29,27,28,25,24,26,34,33,9,10,11,12};
  for(int p : checkPointPins) pinMode(p, INPUT_PULLUP);

  pinMode(HALF_PEDAL, INPUT);

  // Initialize Group Lines & Build Faulty Line Map
  int physicalIdx = 0;
  for (int i = 0; i < 18; i++) {
    // The faulty line is shorted to another line in the cable, so it must never be
    // driven: leave it as a high-impedance input, otherwise the two pins fight.
    if ((i + 1) == FAULTY_LINE) { pinMode(ALL_CABLE_LINES[i], INPUT); continue; }
    pinMode(ALL_CABLE_LINES[i], OUTPUT);
    digitalWriteFast(ALL_CABLE_LINES[i], HIGH); // Idle = HIGH (Active Low)
  }

  // Map Logical Groups (Music) to Physical Pins (Hardware)
  // Logic: "Jump over" the pin index defined by FAULTY_LINE
  for (int g = 0; g < LOGICAL_GROUPS_NEEDED; g++) {
    if ((physicalIdx + 1) == FAULTY_LINE) physicalIdx++;
    groupLineMap[g] = ALL_CABLE_LINES[physicalIdx++];
  }

  // Explicit state array initialization (embedded safety)
  memset(isMeasuringVelocity, 0, sizeof(isMeasuringVelocity));
  memset(strikeDetected, 0, sizeof(strikeDetected));
  memset(noteIsActive, 0, sizeof(noteIsActive));

  // --- VELOCITY MAP GENERATION ---
  // Calculates the lookup table with Logarithmic Curve + Linear Grading.
  // The mapping is: duration -> MIDI velocity
  // Each curve has its own multiplier to provide grading (see GROUP_VELOCITY_CURVE).
  for (int j = 0; j < VEL_GRADE_STEPS; j++) {

    // Grading Logic: Calculate multiplier based on curve index (Bass vs Treble)
    double grade = ((double)j / (double)(VEL_GRADE_STEPS - 1));
    double multiplier = (1.0 + (VELOCITY_MAP_STRETCH / 2.0)) - (grade * VELOCITY_MAP_STRETCH);

    int lastV = 127;
    for (int i = 0; i < VEL_MAP_LENGTH; i++) {
      // log(Dist / 0) is undefined; a zero flight time is the fastest possible hit.
      // No wider clamp here: the curve saturates at 127 by itself, and clamping
      // e.g. everything below 200us would create a jump in the treble curves.
      if (i == 0) { velocityMap[j][i] = 127; continue; }

      // The Physics Formula: v = Multiplier * (Offset + Log(Dist / Time))
      double calc = VEL_LOG_MULTIPLIER * log(VEL_DISTANCE_FACTOR / (double)i) / log(10.0);
      int finalV = (int)(multiplier * (VEL_ADDITION + calc));

      // Clamping to MIDI limits
      if (finalV > 127) finalV = 127;
      if (finalV < 1) finalV = 1;

      // Monotonicity Enforcement:
      // Ensure velocity never increases as flight time increases (physics violation).
      if (finalV > lastV) finalV = lastV;
      velocityMap[j][i] = (byte)finalV;
      lastV = finalV;
    }
  }

  // --- PEDAL TABLE GENERATION ---
  // Simple linear interpolation for the Kawai 10H continuous pedal.
  double step = 127.0 / (HIGH_PEDAL_LIMIT - LOW_PEDAL_LIMIT);
  for (int i = 0; i < 256; i++) {
    if (i < LOW_PEDAL_LIMIT) pedalCcTable[i] = 127; // Fully Pressed
    else if (i > HIGH_PEDAL_LIMIT) pedalCcTable[i] = 0; // Fully Released
    else pedalCcTable[i] = (byte)(127 - (step * (i - LOW_PEDAL_LIMIT)) + 0.5);
  }

  adc->adc0->setAveraging(16);
  adc->adc0->setResolution(8);
  adc->startContinuous(HALF_PEDAL);

  // Ready signal
  for(int k=0; k<3; k++){ digitalWrite(LED, HIGH); delay(100); digitalWrite(LED, LOW); delay(100); }
}

void loop() {
  // --- MAIN SCANNING LOOP (pipelined) ---
  // Each group's 15 inputs are captured at once, right after its settling time.
  // The captured inputs are processed while the NEXT group is settling, so the
  // settling time is spent on useful work instead of waiting.
  InputSnapshot inputs;

  for (int g = 0; g < LOGICAL_GROUPS_NEEDED; g++) {
    int linePin = groupLineMap[g];

    // 1. Activate Group (Transceivers enabled, Active LOW)
    digitalWriteFast(linePin, LOW);
    uint32_t enabledAt = ARM_DWT_CYCCNT;

    // 2. While this group settles, process the previous group's inputs
    if (g > 0) processGroup(g - 1, inputs);
    waitCyclesSince(enabledAt, MATRIX_SETTLING_CYCLES); // minimum settling time

    // 3. Capture this group's 15 inputs
    readGroupInputs(inputs);

    // 4. Deactivate Group (Transceivers disabled, High Impedance / HIGH)
    digitalWriteFast(linePin, HIGH);

    // 5. Dead Time (the next group must not drive the lines before this one lets go)
    waitCyclesSince(ARM_DWT_CYCCNT, MATRIX_DEAD_TIME_CYCLES);
  }
  processGroup(LOGICAL_GROUPS_NEEDED - 1, inputs);

  // --- THROTTLED PEDAL LOGIC ---
  if (pedalUpdateTimer >= PEDAL_SCAN_INTERVAL_MS) {
    processPedal();
    pedalUpdateTimer = 0;
  }

  usbMIDI.send_now();
}

/**
 * Busy-waits until at least `cycles` CPU cycles have passed since `start`.
 * Unsigned subtraction keeps this correct when the cycle counter wraps around.
 */
static inline void waitCyclesSince(uint32_t start, uint32_t cycles) {
  while ((uint32_t)(ARM_DWT_CYCCNT - start) < cycles) {}
}

/**
 * Captures the 15 check point inputs of the active group (only the port registers
 * that hold check point pins are read).
 */
static inline void readGroupInputs(InputSnapshot &s) {
  int i = 0;
  #define READ_PORT(r) s.port[i++] = isCheckPointPort(&(r)) ? (uint32_t)(r) : 0u;
  INPUT_PORTS(READ_PORT)
  #undef READ_PORT
}

/**
 * Runs the hammer logic for the 5 keys of group g on its captured inputs.
 */
static inline void processGroup(int g, const InputSnapshot &s) {
  byte firstNote = 21 + 5 * g; // Piano starts at A0
  byte curve = GROUP_VELOCITY_CURVE[g];
  checkHammerState(firstNote,     curve, AT_POINT(s, N1_STRIKE), AT_POINT(s, N1_ESCAP), AT_POINT(s, N1_DAMPER));
  checkHammerState(firstNote + 1, curve, AT_POINT(s, N2_STRIKE), AT_POINT(s, N2_ESCAP), AT_POINT(s, N2_DAMPER));
  checkHammerState(firstNote + 2, curve, AT_POINT(s, N3_STRIKE), AT_POINT(s, N3_ESCAP), AT_POINT(s, N3_DAMPER));
  // The last group of an 88-key piano only has 3 keys (A#7-C8).
  if (PIANO_SIZE == 88 && g == 17) return;
  checkHammerState(firstNote + 3, curve, AT_POINT(s, N4_STRIKE), AT_POINT(s, N4_ESCAP), AT_POINT(s, N4_DAMPER));
  checkHammerState(firstNote + 4, curve, AT_POINT(s, N5_STRIKE), AT_POINT(s, N5_ESCAP), AT_POINT(s, N5_DAMPER));
}

/**
 * LOGIC: Determines Hammer State based on Proximity Thresholds.
 * We check from CLOSEST (Strike) to FURTHEST (Damper).
 */
static inline void checkHammerState(byte note, byte curve, bool atStrike, bool atEscap, bool atDamper) {

  // 1. STRIKE ZONE (Closest Proximity)
  // Logic: The hammer is at the peak of travel, closest to the rail.
  if (atStrike) {
    if (!strikeDetected[note]) {
      // Calculate final velocity based on flight time
      unsigned long t = hammerTimer[note];
      byte vel = (t < VEL_MAP_LENGTH) ? velocityMap[curve][t] : 1;

      usbMIDI.sendNoteOn(note, vel, 1);

      strikeDetected[note] = true;
      noteIsActive[note] = true;
    }
  }

  // 2. ESCAPEMENT ZONE (Medium Proximity)
  // Logic: Hammer is close enough to trigger Escapement check point,
  // but NOT close enough to trigger Strike. It is "in flight".
  else if (atEscap) {
    if (!isMeasuringVelocity[note]) {
      // Hammer just entered the Flight Zone from below. Start Timer.
      hammerTimer[note] = 0;
      isMeasuringVelocity[note] = true;
      strikeDetected[note] = false; // Reset latch
    }
  }

  // 3. DAMPER ZONE (Far Proximity)
  // Logic: Hammer is detected by the furthest check point (Damper),
  // but hasn't reached Escapement. It is likely rising slowly or hovering.
  else if (atDamper) {
    // If we drop back down to here from above, abort velocity measurement.
    isMeasuringVelocity[note] = false;
  }

  // 4. OUT OF RANGE (Rest)
  // Logic: Hammer is too far for any check point to get activated (All HIGH).
  else {
    if (noteIsActive[note]) {
      // Hammer has fallen all the way back to rest.
      usbMIDI.sendNoteOn(note, 0, 1); // Note Off
      noteIsActive[note] = false;
    }
  }
}

void processPedal() {
  // Check if ADC has new data ready (prevents stale reads)
  if (adc->adc0->isComplete()) {
    byte voltage = (byte)adc->adc0->analogReadContinuous();

    // Noise Gate: Ignore jitter (optimized without abs() function call)
    int diff = voltage - lastPedalVoltage;
    if (diff > 1 || diff < -1) {
        lastPedalVoltage = voltage;
        byte midiVal = pedalCcTable[voltage];

        if (midiVal != lastPedalMidiValue) {
          lastPedalMidiValue = midiVal;
          usbMIDI.sendControlChange(64, midiVal, 1);
        }
    }
  }
}