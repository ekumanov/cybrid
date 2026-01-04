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
 * PERFORMANCE METRICS (Empirical Data from CyberGene):
 * -------------------------------------------------------------
 * - Single Group Scan Duration: ~0.5µs
 * - Full Keyboard Scan (85-88 keys): ~8.9µs
 * - Scan Rate: ~112 kHz
 * - Velocity Error @ MIDI 127: < 1 velocity step
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
 * The time to wait after pulling a Group Line LOW (activating the comparators)
 * before reading the sensor outputs. This accounts for:
 * 1. Cable capacitance (rise/fall time of the signal).
 * 2. Comparator response time.
 * * TUNING: Start at 500ns. Lower it until "Ghost Notes" appear, then double it.
 *
 * CyberGene's measurements confirm that the RC time constant is <100ns.
 * * 100ns is a safe, high-speed default for Teensy 4.1.
 * * If "Ghost Notes" appear, increase this to 200 or 500 and do the above tuning.
 */
#define MATRIX_SETTLING_DELAY_NS 100

// PEDAL SCAN INTERVAL:
// 20ms = 50Hz. Prevents high-density MIDI CC data from clogging
// older DAWs or software piano engines.
#define PEDAL_SCAN_INTERVAL_MS 20

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
 * - 0.4 means we apply a multiplier of ~1.2 to Bass keys (boosting their signal)
 * and ~0.8 to Treble keys (attenuating their signal).
 * - This ensures a consistent "feel" across the keyboard.
 */
#define VELOCITY_MAP_STRETCH 0.4

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
byte velocityMap[LOGICAL_GROUPS_NEEDED][VEL_MAP_LENGTH];
byte pedalCcTable[256];
int groupLineMap[LOGICAL_GROUPS_NEEDED]; // Logic Group -> Physical Pin Map

// Loop State Variables
byte noteA, noteB, noteC, noteD, noteE;
byte activeMidiNote;
int activeMapGroup;

// Pedal Throttling
elapsedMillis pedalUpdateTimer;
byte lastPedalMidiValue = 0;
byte lastPedalVoltage = 0;

ADC *adc = new ADC();

void setup() {
  pinMode(LED, OUTPUT);

  // Initialize Comparator Input Pins
  int checkPointPins[] = {32,31,30,29,27,28,25,24,26,34,33,9,10,11,12};
  for(int p : checkPointPins) pinMode(p, INPUT);
  pinMode(HALF_PEDAL, INPUT);

  // Initialize Group Lines & Build Faulty Line Map
  int physicalIdx = 0;
  for (int i = 0; i < 18; i++) {
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
  for (int j = 0; j < LOGICAL_GROUPS_NEEDED; j++) {

    // Grading Logic: Calculate multiplier based on position (Bass vs Treble)
    double grade = ((double)j / (double)(LOGICAL_GROUPS_NEEDED - 1));
    double multiplier = (1.0 + (VELOCITY_MAP_STRETCH / 2.0)) - (grade * VELOCITY_MAP_STRETCH);

    int lastV = 127;
    for (int i = 0; i < VEL_MAP_LENGTH; i++) {
      if (i < 200) { velocityMap[j][i] = 127; continue; } // Clamp noise/impossibly fast hits

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
  // Simple linear interpolation for the Kawai 10H pot.
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
  int startNote = 21; // Piano starts at A0

  // --- MAIN SCANNING LOOP ---
  for (int g = 0; g < LOGICAL_GROUPS_NEEDED; g++) {
    int linePin = groupLineMap[g];

    // 1. Activate Group Comparators (Active LOW)
    digitalWriteFast(linePin, LOW);

    // 2. Wait for voltage to stabilize (Cable Capacitance)
    delayNanoseconds(MATRIX_SETTLING_DELAY_NS);

    // 3. Define MIDI Notes for this Group
    noteA = startNote; noteB = startNote + 1; noteC = startNote + 2;
    if (PIANO_SIZE == 88 && g == 17) { noteD = 0; noteE = 0; }
    else { noteD = startNote + 3; noteE = startNote + 4; }

    activeMapGroup = g;
    scanGroup();

    // 4. Deactivate Group (High Impedance / HIGH)
    digitalWriteFast(linePin, HIGH);
    startNote += 5;
  }

  // --- THROTTLED PEDAL LOGIC ---
  if (pedalUpdateTimer >= PEDAL_SCAN_INTERVAL_MS) {
    processPedal();
    pedalUpdateTimer = 0;
  }

  usbMIDI.send_now();
}

/**
 * Scans the 5 hammers in the active group.
 */
void scanGroup() {
  activeMidiNote = noteA; if (activeMidiNote != 0) checkHammerState(N1_STRIKE, N1_ESCAP, N1_DAMPER);
  activeMidiNote = noteB; if (activeMidiNote != 0) checkHammerState(N2_STRIKE, N2_ESCAP, N2_DAMPER);
  activeMidiNote = noteC; if (activeMidiNote != 0) checkHammerState(N3_STRIKE, N3_ESCAP, N3_DAMPER);
  activeMidiNote = noteD; if (activeMidiNote != 0) checkHammerState(N4_STRIKE, N4_ESCAP, N4_DAMPER);
  activeMidiNote = noteE; if (activeMidiNote != 0) checkHammerState(N5_STRIKE, N5_ESCAP, N5_DAMPER);
}

/**
 * LOGIC: Determines Hammer State based on Proximity Thresholds.
 * We scan from ClOSEST (Strike) to FURTHEST (Damper).
 */
void checkHammerState(int strikePin, int escapPin, int damperPin) {

  // 1. STRIKE ZONE (Closest Proximity)
  // Logic: The hammer is at the peak of travel, closest to the sensor.
  // Comparator Output: LOW
  if (digitalReadFast(strikePin) == LOW) {
    if (!strikeDetected[activeMidiNote]) {
      // Calculate final velocity based on flight time
      unsigned long t = hammerTimer[activeMidiNote];
      byte vel = (t < VEL_MAP_LENGTH) ? velocityMap[activeMapGroup][t] : 1;

      usbMIDI.sendNoteOn(activeMidiNote, vel, 1);

      strikeDetected[activeMidiNote] = true;
      noteIsActive[activeMidiNote] = true;
    }
  }

  // 2. ESCAPEMENT ZONE (Medium Proximity)
  // Logic: Hammer is close enough to trigger Escapement comparator,
  // but NOT close enough to trigger Strike. It is "in flight".
  else if (digitalReadFast(escapPin) == LOW) {
    if (!isMeasuringVelocity[activeMidiNote]) {
      // Hammer just entered the Flight Zone from below. Start Timer.
      hammerTimer[activeMidiNote] = 0;
      isMeasuringVelocity[activeMidiNote] = true;
      strikeDetected[activeMidiNote] = false; // Reset latch
    }
  }

  // 3. DAMPER ZONE (Far Proximity)
  // Logic: Hammer is detected by the furthest sensor (Damper),
  // but hasn't reached Escapement. It is likely rising slowly or hovering.
  else if (digitalReadFast(damperPin) == LOW) {
    // If we drop back down to here from above, abort velocity measurement.
    isMeasuringVelocity[activeMidiNote] = false;
  }

  // 4. OUT OF RANGE (Rest)
  // Logic: Hammer is too far for any sensor to detect (All HIGH).
  else {
    if (noteIsActive[activeMidiNote]) {
      // Hammer has fallen all the way back to rest.
      usbMIDI.sendNoteOn(activeMidiNote, 0, 1); // Note Off
      noteIsActive[activeMidiNote] = false;
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