#include "unit.h"

Unit::Unit(FastAccelStepperEngine& engine, uint8_t unit) {
  unitNum = unit;
  stepper = engine.stepperConnectToPin(unitStepPin[unitNum]);

  stepper->setEnablePin(UnitEnablePin[unitNum] | PIN_EXTERNAL_FLAG);
  stepper->setAutoEnable(true);
  stepper->setDelayToEnable(1000); // microseconds
  stepper->setDelayToDisable(2); // milliseconds
  // debugf("Unit %d Step pin set to %d\n", unitNum, unitStepPin[unitNum]);

  stepper->setSpeedInUs(rotationSpeeduS);  // the parameter is us/step
  stepper->setAcceleration(4000);

  missedSteps = 0; 
  currentLetterPosition = 0;
  destinationLetter = 0;
  pendingLetter = 0;
  calibrationStarted = false;
  calibrationComplete = false;
  currentHallValue = 1;
  lastHallUpdateTime = 0;
  }

// calc number of steps to rotate based on cumulative step error
int16_t Unit::stepsToRotate (float steps) {
    int16_t roundedStep = (int16_t)steps;
    missedSteps = missedSteps + ((float)steps - (float)roundedStep);
    if (missedSteps > 1) {
        roundedStep = roundedStep + 1;
        missedSteps--;
    }
  return roundedStep;
}

// translates char to letter position
uint8_t Unit::translateLettertoInt(char letterchar) {
  for (int i = 0; i < 45; i++) {
    if (letterchar == letters[i]) {
      return i;
    }
  }
  return 0;
}

// calc flaps to rotate to get to a specified letter
uint8_t Unit::flapsToRotateToLetter(char letterchar, boolean *recalibrate) {
    int8_t newLetterPosition;
    int8_t deltaFlapPosition;

    newLetterPosition = translateLettertoInt(letterchar);
    deltaFlapPosition = newLetterPosition - currentLetterPosition;

    if (deltaFlapPosition < 0) {
      deltaFlapPosition = 45 + deltaFlapPosition;
      *recalibrate = true;
    }
    else {
      currentLetterPosition = newLetterPosition;
    }

    return deltaFlapPosition;
}

// calc steps to rotate forward a specified number of flaps
uint16_t Unit::stepsToRotateFlaps(uint16_t flaps) {
  float preciseStep = (float)flaps * FlapStep[unitNum];
  return stepsToRotate (preciseStep);
}

// only for testing: Move stepper by a raw number of steps
void Unit::moveStepperbyStep(int16_t steps) {
    stepper->move(steps);
}

void Unit::moveStepperbyFlap(uint16_t flaps) {
    stepper->move(stepsToRotateFlaps(flaps));
}

void Unit::moveSteppertoLetter(char toLetter) {
  boolean recalibrate = false;

  destinationLetter = toLetter;

  uint8_t flapsToMove = flapsToRotateToLetter(toLetter, &recalibrate);
  debugf("Unit %02d flapsToMove %d\n", unitNum, flapsToMove);

  if (recalibrate) {
    calibrationComplete = false;
    pendingLetter = toLetter;
    debugf("Unit %02d pendingLetter '%c'\n", unitNum, pendingLetter);
    debugf("Pending,%02d,'%c'\n", unitNum, pendingLetter);
  }
  else {
    debugf("Unit %02d move to '%c'\n", unitNum, toLetter);
    moveStepperbyFlap(flapsToMove);
    pendingLetter = 0;
  }
}

// start calibration of the unit using the hall sensor
void Unit::calibrateStart() {
  calibrationComplete = false;
  calibrationStarted = true;
  calibrationStartTime = millis();

  stepper->runForward();

  // if starting within range of the sensor, need to move outside range before doing calibration
  if (currentHallValue == 0) {
      debugf("preInitialise started for Unit %d\n", unitNum);
      preInitialise = true;
  }
  else {
      preInitialise = false;
  }

  debugf("Calibration started for Unit %d\n", unitNum);
  
  stepper->runForward();
}

// continue calibration of the unit using the hall sensor
int8_t Unit::calibrate() {
  if (!calibrationComplete) {

    // If taking too long, there must be a problem
    if (millis() - calibrationStartTime > 12000) {
      currentLetterPosition = 0;
      calibrationComplete = true;
      calibrationStarted= false;
      debugf("calibration for Unit %d failed\n", unitNum);
      stepper->forceStop();
      return -1;
    }

    // if reached end of preinitialisation phase
    if (preInitialise == true && currentHallValue == 1) {
      preInitialise = false;
      debugf("preInitialise completed for Unit %d\n", unitNum);
    }
    //if still in preinitialising phase, keep moving
    else if (preInitialise == true) {
      return 0;
    }
    // if sensor reached, do calibration
    else if (currentHallValue == 0) {
      // reached marker, go to calibrated offset position
      debugf("Calb,%02d\n", unitNum);
      stepper->forceStopAndNewPosition(0);
      delay(1); // attempt to fix rare hangup
      stepper->move(calOffsetUnit[unitNum]);
      currentLetterPosition = 0;
      missedSteps = 0;
      calibrationComplete = true;
      calibrationStarted = false;
      // Reset speed
      // stepper->setSpeedInUs(rotationSpeeduS);  // the parameter is us/step
      debugf("Unit %d calibrated\n", unitNum);
      return 1;
      }

    return 0;
  }

  // just return if calibration already completed
  return 1;
}

boolean Unit::checkIfRunning() {
  return stepper->isRunning();
}

boolean Unit::updateHallValue(uint8_t updatedHallValue) {
  uint32_t timedelta;
  if (updatedHallValue != currentHallValue) {
    timedelta = millis() - lastHallUpdateTime;

    currentHallValue = updatedHallValue;
    lastHallUpdateTime = millis();
    // debugf("Unit: %d, Hall: %d, Delta: %d\n", unitNum, currentHallValue, timedelta);
    // debugf("Hall,%02d,%d,%lu,%d,%d,%d,'%c'\n", unitNum, currentHallValue, timedelta, preInitialise, calibrationStarted, calibrationComplete, pendingLetter);

    // If occasional glitch occurrs, start calibration again
    if (timedelta <= 100) {
      debugf(TXT_RED "GLITCH,%02d,%d,%d,'%c'\n" TXT_RST, unitNum, updatedHallValue, timedelta, destinationLetter);
      return false;
    }
  }

  return true;
}