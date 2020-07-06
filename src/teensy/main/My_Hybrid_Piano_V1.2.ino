/**
 * CyberGene's DIY Hybrid Piano Controller
 * For Teensy 3.6 @ 256 MHz
 * Author: Evgeni Kumanov (CyberGene), cybergene@me.com
 * 2019, 2020
 */
#include <ADC.h>

#define note1_1 32
#define note1_2 31
#define note1_3 30
#define note2_1 29
#define note2_2 27
#define note2_3 28
#define note3_1 25
#define note3_2 24
#define note3_3 26
#define note4_1 34
#define note4_2 33
#define note4_3 9
#define note5_1 10
#define note5_2 11
#define note5_3 12

#define HALF_PEDAL A21

#define GROUP01 35
#define GROUP02 36
#define GROUP03 37
#define GROUP04 38
#define GROUP05 39
#define GROUP06 14
#define GROUP07 15
#define GROUP08 16
#define GROUP09 17
#define GROUP10 8
#define GROUP11 7
#define GROUP12 6
#define GROUP13 5
#define GROUP14 4
#define GROUP15 3
#define GROUP16 2
#define GROUP17 1
#define GROUP18 0
#define LED 13

#define NOTE_A0 21
#define NOTE_Bb0 22
#define NOTE_B0 23
#define NOTE_C1 24
#define NOTE_Db1 25
#define NOTE_D1 26
#define NOTE_Eb1 27
#define NOTE_E1 28
#define NOTE_F1 29
#define NOTE_Gb1 30
#define NOTE_G1 31
#define NOTE_Ab1 32
#define NOTE_A1 33
#define NOTE_Bb1 34
#define NOTE_B1 35
#define NOTE_C2 36
#define NOTE_Db2 37
#define NOTE_D2 38
#define NOTE_Eb2 39
#define NOTE_E2 40
#define NOTE_F2 41
#define NOTE_Gb2 42
#define NOTE_G2 43
#define NOTE_Ab2 44
#define NOTE_A2 45
#define NOTE_Bb2 46
#define NOTE_B2 47
#define NOTE_C3 48
#define NOTE_Db3 49
#define NOTE_D3 50
#define NOTE_Eb3 51
#define NOTE_E3 52
#define NOTE_F3 53
#define NOTE_Gb3 54
#define NOTE_G3 55
#define NOTE_Ab3 56
#define NOTE_A3 57
#define NOTE_Bb3 58
#define NOTE_B3 59
#define NOTE_C4 60
#define NOTE_Db4 61
#define NOTE_D4 62
#define NOTE_Eb4 63
#define NOTE_E4 64
#define NOTE_F4 65
#define NOTE_Gb4 66
#define NOTE_G4 67
#define NOTE_Ab4 68
#define NOTE_A4 69
#define NOTE_Bb4 70
#define NOTE_B4 71
#define NOTE_C5 72
#define NOTE_Db5 73
#define NOTE_D5 74
#define NOTE_Eb5 75
#define NOTE_E5 76
#define NOTE_F5 77
#define NOTE_Gb5 78
#define NOTE_G5 79
#define NOTE_Ab5 80
#define NOTE_A5 81
#define NOTE_Bb5 82
#define NOTE_B5 83
#define NOTE_C6 84
#define NOTE_Db6 85
#define NOTE_D6 86
#define NOTE_Eb6 87
#define NOTE_E6 88
#define NOTE_F6 89
#define NOTE_Gb6 90
#define NOTE_G6 91
#define NOTE_Ab6 92
#define NOTE_A6 93
#define NOTE_Bb6 94
#define NOTE_B6 95
#define NOTE_C7 96
#define NOTE_Db7 97
#define NOTE_D7 98
#define NOTE_Eb7 99
#define NOTE_E7 100
#define NOTE_F7 101
#define NOTE_Gb7 102
#define NOTE_G7 103
#define NOTE_Ab7 104
#define NOTE_A7 105

#define VEL_MAP_LENGTH 12187
#define VEL_DISTANCE_FACTOR 1500.0
#define VEL_LOG_MULTIPLIER 100.0
#define VEL_ADDITION 57.96
#define NUMBER_OF_GROUPS 17
#define VELOCITY_MAP_STRETCH 0.4

// where it starts generating values on pedal down
#define HIGH_PEDAL_LIMIT 192

// where it reaches value 127
#define LOW_PEDAL_LIMIT 85

elapsedMicros duration[127];

boolean started[127];
boolean hit[127];
boolean played[127];

byte velocity;
byte lastSustainValue;
byte lastSustainVoltage;

byte velocityMap[NUMBER_OF_GROUPS][VEL_MAP_LENGTH];
byte ccValue[256];

byte currentMidiNote1;
byte currentMidiNote2;
byte currentMidiNote3;
byte currentMidiNote4;
byte currentMidiNote5;

byte currentMidiNote;
int currentVelocityMapGroup;


ADC *adc = new ADC();

void setup() {
  //Serial.begin(9600);
  pinMode(note1_1, INPUT);
  pinMode(note1_2, INPUT);
  pinMode(note1_3, INPUT);
  pinMode(note2_1, INPUT);
  pinMode(note2_2, INPUT);
  pinMode(note2_3, INPUT);
  pinMode(note3_1, INPUT);
  pinMode(note3_2, INPUT);
  pinMode(note3_3, INPUT);
  pinMode(note4_1, INPUT);
  pinMode(note4_2, INPUT);
  pinMode(note4_3, INPUT);
  pinMode(note5_1, INPUT);
  pinMode(note5_2, INPUT);
  pinMode(note5_3, INPUT);

  pinMode(HALF_PEDAL, INPUT); // Kawai F10H half-pedal

  pinMode(GROUP01, OUTPUT);
  pinMode(GROUP02, OUTPUT);
  pinMode(GROUP03, OUTPUT);
  pinMode(GROUP04, OUTPUT);
  pinMode(GROUP05, OUTPUT);
  pinMode(GROUP06, OUTPUT);
  pinMode(GROUP07, OUTPUT);
  pinMode(GROUP08, OUTPUT);
  pinMode(GROUP09, OUTPUT);
  pinMode(GROUP10, OUTPUT);
  pinMode(GROUP11, OUTPUT);
  pinMode(GROUP12, OUTPUT);
  pinMode(GROUP13, OUTPUT);
  pinMode(GROUP14, OUTPUT);
  pinMode(GROUP15, OUTPUT);
  //pinMode(GROUP16, OUTPUT);
  pinMode(GROUP17, OUTPUT);
  pinMode(GROUP18, OUTPUT);
  pinMode(LED, OUTPUT);

  for (byte i = 0; i < 127; i++) {
    started[i] = false;
    hit[i] = false;
    played[i] = false;
  }
  //delay(2000);
  int v = 0;
  int midiValue = 127;
  for (int j = 0; j < NUMBER_OF_GROUPS; j++) {
    double multiplier = (1 + (VELOCITY_MAP_STRETCH / 2))
                        - (((double)j / (NUMBER_OF_GROUPS - 1)) * VELOCITY_MAP_STRETCH);
    //Serial.println(multiplier);
    for (int i = 0; i < VEL_MAP_LENGTH; i++) {
      //if (i < 300) {
      //  velocityMap[i] = 127;
      //  continue;
      //}
      double d = VEL_LOG_MULTIPLIER * log(VEL_DISTANCE_FACTOR / i) / log(10.0);
      v = multiplier * (VEL_ADDITION + d);
      if (v > 127) {
        v = 127;
      }
      if (v < 1) {
        v = 1;
      }
      if (v < midiValue) {
        midiValue = v;
        //Serial.print(i);
        //Serial.print(" -> ");
        //Serial.println(midiValue);
      }
      velocityMap[j][i] = (byte) v;
    }
  }

  double pedalSegment = 127.0 / (HIGH_PEDAL_LIMIT - LOW_PEDAL_LIMIT);

  for (int i = 0; i < 256; i++) {
    if (i < LOW_PEDAL_LIMIT) {
      ccValue[i] = 127;
    } else if (i > HIGH_PEDAL_LIMIT) {
      ccValue[i] = 0;
    } else {
      byte value = (byte) (127 - (pedalSegment * (i - LOW_PEDAL_LIMIT)) + 0.5);
      ccValue[i] = value < 0 ? 0 : value;
    }
  }

  lastSustainValue = 0;
  lastSustainVoltage = 0;

  adc->adc0->setAveraging(16);
  adc->adc0->setResolution(8);
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_LOW_SPEED);
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_LOW_SPEED);
  adc->startContinuous(HALF_PEDAL);

  digitalWrite(LED, HIGH);
  delay(100);
  digitalWrite(LED, LOW);
  delay(100);
  digitalWrite(LED, HIGH);
  delay(100);
  digitalWrite(LED, LOW);
  delay(100);
  digitalWrite(LED, HIGH);
  delay(100);
  digitalWrite(LED, LOW);
}

void loop() {
  digitalWriteFast(GROUP01, LOW);
  currentMidiNote1 = NOTE_A0;
  currentMidiNote2 = NOTE_Bb0;
  currentMidiNote3 = NOTE_B0;
  currentMidiNote4 = NOTE_C1;
  currentMidiNote5 = NOTE_Db1;
  currentVelocityMapGroup = 4;
  checkNotes();
  digitalWriteFast(GROUP01, HIGH);

  digitalWriteFast(GROUP02, LOW);
  currentMidiNote1 = NOTE_D1;
  currentMidiNote2 = NOTE_Eb1;
  currentMidiNote3 = NOTE_E1;
  currentMidiNote4 = NOTE_F1;
  currentMidiNote5 = NOTE_Gb1;
  currentVelocityMapGroup = 4;
  checkNotes();
  digitalWriteFast(GROUP02, HIGH);

  digitalWriteFast(GROUP03, LOW);
  currentMidiNote1 = NOTE_G1;
  currentMidiNote2 = NOTE_Ab1;
  currentMidiNote3 = NOTE_A1;
  currentMidiNote4 = NOTE_Bb1;
  currentMidiNote5 = NOTE_B1;
  currentVelocityMapGroup = 5;
  checkNotes();
  digitalWriteFast(GROUP03, HIGH);

  digitalWriteFast(GROUP04, LOW);
  currentMidiNote1 = NOTE_C2;
  currentMidiNote2 = NOTE_Db2;
  currentMidiNote3 = NOTE_D2;
  currentMidiNote4 = NOTE_Eb2;
  currentMidiNote5 = NOTE_E2;
  currentVelocityMapGroup = 6;
  checkNotes();
  digitalWriteFast(GROUP04, HIGH);

  digitalWriteFast(GROUP05, LOW);
  currentMidiNote1 = NOTE_F2;
  currentMidiNote2 = NOTE_Gb2;
  currentMidiNote3 = NOTE_G2;
  currentMidiNote4 = NOTE_Ab2;
  currentMidiNote5 = NOTE_A2;
  currentVelocityMapGroup = 6;
  checkNotes();
  digitalWriteFast(GROUP05, HIGH);

  digitalWriteFast(GROUP06, LOW);
  currentMidiNote1 = NOTE_Bb2;
  currentMidiNote2 = NOTE_B2;
  currentMidiNote3 = NOTE_C3;
  currentMidiNote4 = NOTE_Db3;
  currentMidiNote5 = NOTE_D3;
  currentVelocityMapGroup = 6;
  checkNotes();
  digitalWriteFast(GROUP06, HIGH);

  digitalWriteFast(GROUP07, LOW);
  currentMidiNote1 = NOTE_Eb3;
  currentMidiNote2 = NOTE_E3;
  currentMidiNote3 = NOTE_F3;
  currentMidiNote4 = NOTE_Gb3;
  currentMidiNote5 = NOTE_G3;
  currentVelocityMapGroup = 6;
  checkNotes();
  digitalWriteFast(GROUP07, HIGH);

  digitalWriteFast(GROUP08, LOW);
  currentMidiNote1 = NOTE_Ab3;
  currentMidiNote2 = NOTE_A3;
  currentMidiNote3 = NOTE_Bb3;
  currentMidiNote4 = NOTE_B3;
  currentMidiNote5 = NOTE_C4;
  currentVelocityMapGroup = 7;
  checkNotes();
  digitalWriteFast(GROUP08, HIGH);

  digitalWriteFast(GROUP09, LOW);
  currentMidiNote1 = NOTE_Db4;
  currentMidiNote2 = NOTE_D4;
  currentMidiNote3 = NOTE_Eb4;
  currentMidiNote4 = NOTE_E4;
  currentMidiNote5 = NOTE_F4;
  currentVelocityMapGroup = 8;
  checkNotes();
  digitalWriteFast(GROUP09, HIGH);

  digitalWriteFast(GROUP10, LOW);
  currentMidiNote1 = NOTE_Gb4;
  currentMidiNote2 = NOTE_G4;
  currentMidiNote3 = NOTE_Ab4;
  currentMidiNote4 = NOTE_A4;
  currentMidiNote5 = NOTE_Bb4;
  currentVelocityMapGroup = 9;
  checkNotes();
  digitalWriteFast(GROUP10, HIGH);

  digitalWriteFast(GROUP11, LOW);
  currentMidiNote1 = NOTE_B4;
  currentMidiNote2 = NOTE_C5;
  currentMidiNote3 = NOTE_Db5;
  currentMidiNote4 = NOTE_D5;
  currentMidiNote5 = NOTE_Eb5;
  currentVelocityMapGroup = 10;
  checkNotes();
  digitalWriteFast(GROUP11, HIGH);

  digitalWriteFast(GROUP12, LOW);
  currentMidiNote1 = NOTE_E5;
  currentMidiNote2 = NOTE_F5;
  currentMidiNote3 = NOTE_Gb5;
  currentMidiNote4 = NOTE_G5;
  currentMidiNote5 = NOTE_Ab5;
  currentVelocityMapGroup = 11;
  checkNotes();
  digitalWriteFast(GROUP12, HIGH);

  digitalWriteFast(GROUP13, LOW);
  currentMidiNote1 = NOTE_A5;
  currentMidiNote2 = NOTE_Bb5;
  currentMidiNote3 = NOTE_B5;
  currentMidiNote4 = NOTE_C6;
  currentMidiNote5 = NOTE_Db6;
  currentVelocityMapGroup = 12;
  checkNotes();
  digitalWriteFast(GROUP13, HIGH);

  digitalWriteFast(GROUP14, LOW);
  currentMidiNote1 = NOTE_D6;
  currentMidiNote2 = NOTE_Eb6;
  currentMidiNote3 = NOTE_E6;
  currentMidiNote4 = NOTE_F6;
  currentMidiNote5 = NOTE_Gb6;
  currentVelocityMapGroup = 13;
  checkNotes();
  digitalWriteFast(GROUP14, HIGH);

  digitalWriteFast(GROUP15, LOW);
  currentMidiNote1 = NOTE_G6;
  currentMidiNote2 = NOTE_Ab6;
  currentMidiNote3 = NOTE_A6;
  currentMidiNote4 = NOTE_Bb6;
  currentMidiNote5 = NOTE_B6;
  currentVelocityMapGroup = 14;
  checkNotes();
  digitalWriteFast(GROUP15, HIGH);

  // DISABLE THIS GROUP DUE TO FAULTY IDC CABLE!!! SHORTED WITH GROUP07
  //digitalWriteFast(GROUP16, LOW);
  //currentMidiNote1 = NOTE_C7;
  //currentMidiNote2 = NOTE_Db7;
  //currentMidiNote3 = NOTE_D7;
  //currentMidiNote4 = NOTE_Eb7;
  //currentMidiNote5 = NOTE_E7;
  //currentVelocityMapGroup = 15;
  //checkNotes();
  //digitalWriteFast(GROUP16, HIGH);

  digitalWriteFast(GROUP17, LOW);
  currentMidiNote1 = NOTE_C7;
  currentMidiNote2 = NOTE_Db7;
  currentMidiNote3 = NOTE_D7;
  currentMidiNote4 = NOTE_Eb7;
  currentMidiNote5 = NOTE_E7;
  currentVelocityMapGroup = 15;
  checkNotes();
  digitalWriteFast(GROUP17, HIGH);

  digitalWriteFast(GROUP18, LOW);
  currentMidiNote1 = NOTE_F7;
  currentMidiNote2 = NOTE_Gb7;
  currentMidiNote3 = NOTE_G7;
  currentMidiNote4 = NOTE_Ab7;
  currentMidiNote5 = NOTE_A7;
  currentVelocityMapGroup = 16;
  checkNotes();
  digitalWriteFast(GROUP18, HIGH);

  checkSustainPedal();

  usbMIDI.send_now();
}

void checkNotes() {
  // use a redundant digitalReadFast to introduce a slight delay for the pull-up resistors
  digitalReadFast(note1_1);

  currentMidiNote = currentMidiNote1;
  if (digitalReadFast(note1_1) == LOW) {
    processFirstSensor();
  } else if (digitalReadFast(note1_2) == LOW) {
    processSecondSensor();
  } else if (digitalReadFast(note1_3) == LOW) {
    processThirdSensor();
  } else {
    processNoSensor();
  }

  currentMidiNote = currentMidiNote2;
  if (digitalReadFast(note2_1) == LOW) {
    processFirstSensor();
  } else if (digitalReadFast(note2_2) == LOW) {
    processSecondSensor();
  } else if (digitalReadFast(note2_3) == LOW) {
    processThirdSensor();
  } else {
    processNoSensor();
  }

  currentMidiNote = currentMidiNote3;
  if (digitalReadFast(note3_1) == LOW) {
    processFirstSensor();
  } else if (digitalReadFast(note3_2) == LOW) {
    processSecondSensor();
  } else if (digitalReadFast(note3_3) == LOW) {
    processThirdSensor();
  } else {
    processNoSensor();
  }

  currentMidiNote = currentMidiNote4;
  if (digitalReadFast(note4_1) == LOW) {
    processFirstSensor();
  } else if (digitalReadFast(note4_2) == LOW) {
    processSecondSensor();
  } else if (digitalReadFast(note4_3) == LOW) {
    processThirdSensor();
  } else {
    processNoSensor();
  }

  currentMidiNote = currentMidiNote5;
  if (digitalReadFast(note5_1) == LOW) {
    processFirstSensor();
  } else if (digitalReadFast(note5_2) == LOW) {
    processSecondSensor();
  } else if (digitalReadFast(note5_3) == LOW) {
    processThirdSensor();
  } else {
    processNoSensor();
  }

  //  delayMicroseconds(2);
}

void processFirstSensor() {
  if (!hit[currentMidiNote]) {
    int dur = duration[currentMidiNote];
    //Serial.println(dur);
    if (dur >= VEL_MAP_LENGTH) {
      velocity = 1;
    } else {
      velocity = velocityMap[currentVelocityMapGroup][dur];
    }

    usbMIDI.sendNoteOn(currentMidiNote, velocity, 1);
    //    usbMIDI.send_now();
    hit[currentMidiNote] = true;
    played[currentMidiNote] = true;
  }
}

void processSecondSensor() {
  if (!started[currentMidiNote]) {
    duration[currentMidiNote] = 0;
    started[currentMidiNote] = true;
    hit[currentMidiNote] = false;
  }
}

void processThirdSensor() {
  started[currentMidiNote] = false;
}

void processNoSensor() {
  if (played[currentMidiNote]) {
    usbMIDI.sendNoteOn(currentMidiNote, 0, 1);
    //    usbMIDI.send_now();
    played[currentMidiNote] = false;
    //digitalWriteFast(LED, LOW);
  }
}

byte voltage;
byte currentPedalValue;

void checkSustainPedal() {
  voltage = (byte)adc->adc0->analogReadContinuous();
  if (voltage != lastSustainVoltage) {
    lastSustainVoltage = voltage;
    currentPedalValue = ccValue[voltage];
    if (currentPedalValue != lastSustainValue) {
      lastSustainValue = currentPedalValue;
      usbMIDI.sendControlChange(64, currentPedalValue, 1);
    }
  }
}
