/**
 * CyberGene's DIY Hybrid Piano Controller
 * For Teensy 3.6 @ 256 MHz
 *
 * This is a calibration program for setting the proper voltage for
 * sensor 1 (the closest to the rail where velocity measurement ends)
 * It is based on an old version of the main code and is rather suboptimal
 * but still works for just a calibration
 *
 * Author: Evgeni Kumanov (CyberGene), cybergene@me.com
 * 2019, 2020
 */
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
#define NOTE_D1 26
#define NOTE_G1 31
#define NOTE_C2 36
#define NOTE_F2 41
#define NOTE_Bb2 46
#define NOTE_Eb3 51
#define NOTE_Ab3 56
#define NOTE_Db4 61
#define NOTE_Gb4 66
#define NOTE_B4 71
#define NOTE_E5 76
#define NOTE_A5 81
#define NOTE_D6 86
#define NOTE_G6 91
#define NOTE_C7 96
#define NOTE_F7 101
#define NOTE_A7 105

boolean activated[127];
int currentActive = 0;

void setup() {
  for (int i = 0; i < 127; i++) {
    activated[i] = false;
  }
  Serial.begin(9600);
  pinMode(note1_1, INPUT_PULLUP);
  pinMode(note1_2, INPUT_PULLUP);
  pinMode(note1_3, INPUT_PULLUP);
  pinMode(note2_1, INPUT_PULLUP);
  pinMode(note2_2, INPUT_PULLUP);
  pinMode(note2_3, INPUT_PULLUP);
  pinMode(note3_1, INPUT_PULLUP);
  pinMode(note3_2, INPUT_PULLUP);
  pinMode(note3_3, INPUT_PULLUP);
  pinMode(note4_1, INPUT_PULLUP);
  pinMode(note4_2, INPUT_PULLUP);
  pinMode(note4_3, INPUT_PULLUP);
  pinMode(note5_1, INPUT_PULLUP);
  pinMode(note5_2, INPUT_PULLUP);
  pinMode(note5_3, INPUT_PULLUP);

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
}

void loop() {
  digitalWriteFast(GROUP01, LOW);
  checkNotes(NOTE_A0);
  digitalWriteFast(GROUP01, HIGH);

  digitalWriteFast(GROUP02, LOW);
  checkNotes(NOTE_D1);
  digitalWriteFast(GROUP02, HIGH);

  digitalWriteFast(GROUP03, LOW);
  checkNotes(NOTE_G1);
  digitalWriteFast(GROUP03, HIGH);

  digitalWriteFast(GROUP04, LOW);
  checkNotes(NOTE_C2);
  digitalWriteFast(GROUP04, HIGH);

  digitalWriteFast(GROUP05, LOW);
  checkNotes(NOTE_F2);
  digitalWriteFast(GROUP05, HIGH);

  digitalWriteFast(GROUP06, LOW);
  checkNotes(NOTE_Bb2);
  digitalWriteFast(GROUP06, HIGH);

  digitalWriteFast(GROUP07, LOW);
  checkNotes(NOTE_Eb3);
  digitalWriteFast(GROUP07, HIGH);

  digitalWriteFast(GROUP08, LOW);
  checkNotes(NOTE_Ab3);
  digitalWriteFast(GROUP08, HIGH);

  digitalWriteFast(GROUP09, LOW);
  checkNotes(NOTE_Db4);
  digitalWriteFast(GROUP09, HIGH);

  digitalWriteFast(GROUP10, LOW);
  checkNotes(NOTE_Gb4);
  digitalWriteFast(GROUP10, HIGH);

  digitalWriteFast(GROUP11, LOW);
  checkNotes(NOTE_B4);
  digitalWriteFast(GROUP11, HIGH);

  digitalWriteFast(GROUP12, LOW);
  checkNotes(NOTE_E5);
  digitalWriteFast(GROUP12, HIGH);

  digitalWriteFast(GROUP13, LOW);
  checkNotes(NOTE_A5);
  digitalWriteFast(GROUP13, HIGH);

  digitalWriteFast(GROUP14, LOW);
  checkNotes(NOTE_D6);
  digitalWriteFast(GROUP14, HIGH);

  digitalWriteFast(GROUP15, LOW);
  checkNotes(NOTE_G6);
  digitalWriteFast(GROUP15, HIGH);

  // DISABLE THIS GROUP DUE TO FAULTY IDC CABLE!!! SHORTED WITH GROUP07
  //digitalWriteFast(GROUP16, LOW);
  //checkNotes(NOTE_C7);
  //digitalWriteFast(GROUP16, HIGH);

  digitalWriteFast(GROUP17, LOW);
  checkNotes(NOTE_C7);
  digitalWriteFast(GROUP17, HIGH);

  digitalWriteFast(GROUP18, LOW);
  checkNotes(NOTE_F7);
  digitalWriteFast(GROUP18, HIGH);
}

void checkNotes(int midiNote) {
  delayMicroseconds(2);

  checkNote(midiNote, digitalReadFast(note1_1) == LOW);
  checkNote(midiNote + 1, digitalReadFast(note2_1) == LOW);
  checkNote(midiNote + 2, digitalReadFast(note3_1) == LOW);
  checkNote(midiNote + 3, digitalReadFast(note4_1) == LOW);
  checkNote(midiNote + 4, digitalReadFast(note5_1) == LOW);
}

void checkNote(int midiNote, boolean active) {
  if (active) {
    if (currentActive == 0) {
      digitalWrite(LED, HIGH);
      currentActive = midiNote;
    }
    if (!activated[midiNote]) {
      activated[midiNote] = true;
      Serial.println(midiNote);
    }
  } else {
    if (midiNote == currentActive) {
      digitalWrite(LED, LOW);
      currentActive = 0;
    }
    if (activated[midiNote]) {
      activated[midiNote] = false;
      Serial.print("*");
      Serial.println(midiNote);
    }
  }
}
