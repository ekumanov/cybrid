// Stand-in for the Teensy ADC library: the pedal input reads a constant "pedal up" value.
#pragma once
#include "../sim_arduino.h"

enum class ADC_CONVERSION_SPEED { VERY_LOW_SPEED };
enum class ADC_SAMPLING_SPEED { VERY_LOW_SPEED };

struct ADC_Module {
  void setAveraging(int) {}
  void setResolution(int) {}
  void setConversionSpeed(ADC_CONVERSION_SPEED) {}
  void setSamplingSpeed(ADC_SAMPLING_SPEED) {}
  bool isComplete() { return true; }
  int analogReadContinuous() { return 200; }
};

struct ADC {
  ADC_Module module0;
  ADC_Module *adc0 = &module0;
  void startContinuous(int) {}
};
