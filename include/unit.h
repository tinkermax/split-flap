#pragma once

#include <Arduino.h>
#include "FastAccelStepper.h"
#include "debug.h"

// Customise below for each unit for your build. (Units are numbered left to right 0 - 11)
const uint8_t calOffsetUnit[] = {87, 62, 77, 65, 89, 104, 107, 82, 95, 97, 90, 55};
const float FlapStep[] = {2038.0/45, 2038.0/45, 2038.0/45, 2050.0/45, 2049.0/45, 2049.0/45, 2049.0/45, 2038.0/45, 2051.0/45, 2051.2/45, 2049.0/45, 2038.0/45}; // stepper motor steps per rotation per flap, for each unit motor

// The following are hardware related and won't change unless PCB is changed. Note: Units are numbered from left to right 0 - 11.
const uint8_t unitStepPin[] =  {14,13,5,4,18,17,16,15,26,25,23,19};
const uint8_t UnitEnablePin[] = {3,2,1,0,7,6,5,4,11,10,9,8};
const char sensorPort[] = {'A','A','A','A','B','B','A','A','B','B','B','B'}; //Maps unit sensors 0 - 11 to the MCP23017 ports A or B
const uint8_t sensorPortBit[] = {0b00001000,0b00000100,0b00000010,0b00000001,0b00000010,0b00000001,0b00100000,0b00010000,0b00100000,0b00010000,0b00001000,0b00000100}; //Maps unit sensors 0 - 11 to the bit of the MCP23017 port
const uint8_t interruptPin = 27;
const uint8_t button1Pin = 14;
const uint8_t button2Pin = 6;
const char letters[] = {' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '$', '&', '#', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', '.', '-', '?', '!'};
const uint16_t rotationSpeeduS = 2000;

////////////////////////////////////////
// For JTAG debugging, cannot use pins 13,14,15, so free these up when debugging
// const uint8_t UNITCOUNT = 9;
// const uint8_t unitStepPin[] =  {5,4,18,17,16,26,25,23,19};
// const uint8_t UnitEnablePin[] = {1,0,7,6,5,11,10,9,8};
// const char sensorPort[] = {'A','A','B','B','A','B','B','B','B'}; //Maps unit sensors 0 - 11 to the MCP23017 ports A or B
// const uint8_t sensorPortBit[] = {0b00000010,0b00000001,0b00000010,0b00000001,0b00100000,0b00100000,0b00010000,0b00001000,0b00000100}; //Maps unit sensors 0 - 11 to the bit of the above MCP23017 port
// const uint8_t calOffsetUnit[] = {77, 65, 89, 104, 107, 95, 97, 90, 85};
////////////////////////////////////////

class Unit {
  public:
    boolean calibrationStarted;
    boolean calibrationComplete;
    char pendingLetter;
    uint8_t destinationLetter;

    // Constructor for each Unit object
    Unit(FastAccelStepperEngine& engine, uint8_t unitNum);

    void moveStepperbyStep(int16_t steps);
    void moveSteppertoLetter(char toLetter);
    void moveStepperbyFlap(uint16_t flaps);
    void calibrateStart();
    int8_t calibrate();
    boolean checkIfRunning();
    boolean updateHallValue(uint8_t updatedHallValue);

  private:
    FastAccelStepper* stepper;
    float missedSteps;
    uint8_t unitNum;
    bool preInitialise;
    uint8_t currentLetterPosition;
    uint32_t calibrationStartTime;
    uint8_t currentHallValue;
    uint32_t lastHallUpdateTime;

    int16_t stepsToRotate (float steps);
    uint16_t stepsToRotateFlaps(uint16_t flaps);
    uint8_t flapsToRotateToLetter(char letterchar, boolean *recalibrate);
    uint8_t translateLettertoInt(char letterchar);
  };
