/* Firmware for Mechanical Split-Flap Display
 *
 * This is the Arduino firmware for esp32Core_board_v2 (ESP32 DevKitC)
 *
 * Malcolm Yeoman (2024)
 *
 * Blog post: https://tinkerwithtech.net/split-flap-display-brings-back-memories
 * Github:
 *
 * Note1: Implemented so that on the MCP23017 GPA7 and GPB7 are not used as inputs
 *        https://microchipsupport.force.com/s/article/GPA7---GPB7-Cannot-Be-Used-as-Inputs-In-MCP23017
 *
*/

#include <Arduino.h>
#include <Wire.h>
#include <MCP23017.h>
#include <time.h>
#include "system.h"
#include "unit.h"
#if __has_include(<config-private.h>)
    #include "config-private.h"
#else
    #include "config.h"
#endif

// Function headers
void print_test_menu ();
bool setExternalPin(uint8_t pin, uint8_t value);
// boolean calibrate_all_units();
void recalibrate_units();
void IRAM_ATTR sensor_ISR();
void updateHallSensors();
void displayString(String display);
boolean diplayStillMoving();
// void intToBinary(int num, char* binaryStr);
// void debugUnitFlags(String prefix);

// Global vars
uint32_t previousMillis = 0;
uint32_t displayLastStoppedMillis;
uint32_t nextWordAPIMillis = 0;
MCP23017 mcp_en_steppers = MCP23017(0x20);
MCP23017 mcp_sensor = MCP23017(0x21);
Unit *splitFlap[UNITCOUNT];
volatile bool sensortriggered = false;
uint16_t counter = 0;
uint8_t active_menu_unit = 0;
boolean getting_first_word = true;
const char* ssid = WIFI_SSID;
const char* password = WIFI_PWD;
time_t now; // this is the epoch
tm timeinfo;  // structure tm holds time information in a more convenient way
String test_command_previous = "";
uint8_t charSeq = 40;
char save_display[13];
char previous_display[13];
uint8_t reboot_count;
String localIP;
FastAccelStepperEngine engine;
extern WebServer server;
uint8_t word_updates_per_hour = WORDUPDATESPERHOUR; //store config value in variable to prevent div by zero compiler warnings

// RTC memory structure - for persisting data between reboots
typedef struct {
  uint32_t magic;
  char previous_display[13];
  uint8_t reboot_count;
} RTC;
RTC_NOINIT_ATTR RTC nvmem;


// SETUP
void setup() {

#if DEBUG == 1
  Serial.begin(115200);
#endif
  
  WiFi.begin(ssid, password);
  debugln(TXT_BLUE "Starting" TXT_RST);

  // Future expansion possibility: Set up external I2C to chain multiple controller boards
  // Wire2.begin(32,33); //(SDA=32, SCL=33);

  // Configure I2C for MCP23017 port expanders
  Wire.begin(21, 22, 800000); // SDA=21, SCL=22, 800kHz

  mcp_en_steppers.init();
  mcp_en_steppers.portMode(MCP23017Port::A, 0);          //Port A as output
  mcp_en_steppers.portMode(MCP23017Port::B, 0);          //Port B as output
  mcp_en_steppers.writeRegister(MCP23017Register::GPIO_A, 0x00);  //Reset port A 
  mcp_en_steppers.writeRegister(MCP23017Register::GPIO_B, 0x00);  //Reset port B

  mcp_sensor.init();
  mcp_sensor.portMode(MCP23017Port::A, 0b01111111); //Port A 7 bits as input
  mcp_sensor.portMode(MCP23017Port::B, 0b01111111); //Port B 7 bits as input
  mcp_sensor.writeRegister(MCP23017Register::IPOL_A, 0x00);
  mcp_sensor.writeRegister(MCP23017Register::IPOL_B, 0x00);
  mcp_sensor.writeRegister(MCP23017Register::GPIO_A, 0xFF);
  mcp_sensor.writeRegister(MCP23017Register::GPIO_B, 0xFF);

  // Set up fast stepper engine
  engine = FastAccelStepperEngine();
  engine.init();
  engine.setExternalCallForPin(setExternalPin);

  // Initialise split-flap display units
  for (uint8_t unit = 0; unit < UNITCOUNT; unit++) {
    splitFlap[unit] = new Unit(engine, unit);
  }

  // Enable Sensor interrupts
  mcp_sensor.interruptMode(MCP23017InterruptMode::Or); //Both ports logically ORed to same interrupt pin     
  mcp_sensor.interrupt(MCP23017Port::A, CHANGE);
  mcp_sensor.interrupt(MCP23017Port::B, CHANGE);
  mcp_sensor.clearInterrupts();
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), sensor_ISR, FALLING);

  // attempt to connect to Wifi network
  debug("Attempting to connect to SSID: ");
  debugln(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    debug(".");
    // wait 1 second between retries
    delay(1000);
  }

  debug("\nConnected to " TXT_BLUE);
  debugln(ssid);
  localIP = WiFi.localIP().toString();
  debug(localIP);
  debugln(TXT_RST);

  disableCertificates(); 

  // Lookup NTP time
  configTzTime(MY_TZ, MY_NTP_SERVER);
  now = time(nullptr); //start time sync in background

  // Set up REST API
  setup_routing();

  // Read hall sensors to get current values
  updateHallSensors();

  // Read any previous display before last reboot
  if (nvmem.magic == RTC_MAGIC) {
    strncpy(previous_display, nvmem.previous_display, 13);
    previous_display[12]='\0';
    reboot_count = nvmem.reboot_count;
    debugf(TXT_YELLOW "previous_display: [%s], reboots: %d\n" TXT_RST, previous_display, reboot_count);
  }
  else {
    previous_display[0] = '\0';
    reboot_count = 0;
  }

  delay(101); // Delay to avoid initially false triggering the glitch detection

  // Try up to 3 times to get NTP
  uint8_t getntpcount = 0;
  while (getntpcount++ < 3 && !getNTP(now, timeinfo)) {
    delay(1000);
    debugln("Retrying NTP...");
  }

  displayLastStoppedMillis = 0;

#if DEBUG == 1
  print_test_menu();
#endif
}

void loop() {
  String test_command;
  uint16_t test_num;

  ////////////////////////
  // HIGH LEVEL EVENT LOOP
  ////////////////////////

  // Read hall sensors if change detected via interrupt
  if (sensortriggered) {
    sensortriggered = false;
    updateHallSensors();
  }

  // Calibrate any units that require it
  recalibrate_units();

  // Move any units pending due to drum passing origin (after they have recalibrated)
  for (uint8_t unit = 0; unit < UNITCOUNT; unit++) {
    if (splitFlap[unit]->pendingLetter > 0 && splitFlap[unit]->calibrationComplete) {
      splitFlap[unit]->moveSteppertoLetter(splitFlap[unit]->pendingLetter);
    }
  }

  // Lower priority events below
  if ((uint32_t)millis() - previousMillis >= 500) {
    previousMillis = millis();

    // Rest API server
    server.handleClient();

    //If display not moving, check if anything new to display
    if (!diplayStillMoving()) {
      displayLastStoppedMillis = millis();

      // Check if need to redisplay after reboot
      if (previous_display[0] != '\0') {
        // don't allow more than 3 reboots per period - to avoid continous running in the event of fault
        if (reboot_count++ > 3) {
          debugln(TXT_RED "HALTING due to reboot count." TXT_RST);
          while (true);
        }
        debugf("Display previous string: [%s], reboots: %d\n", previous_display, reboot_count);
        displayString(String(previous_display));
        previous_display[0] = '\0';
        getting_first_word = false;
      }

      else if (getting_first_word) {
        if (word_updates_per_hour > 0) {
          String word = wordOfTheDay();
          displayLastStoppedMillis = millis();
          debugf("Word, %02d:%02d, [%s]\n", timeinfo.tm_hour, timeinfo.tm_min, word);
          displayString(word);    
          nextWordAPIMillis = millis() + 60000; //dont check again until this minute passed
        }
        getting_first_word = false;
      }

      // Display random word according to WORDUPDATESPERHOUR
      else if (word_updates_per_hour > 0 && ((uint32_t)millis() > nextWordAPIMillis)) {
        // Only update during daytime hours
        if (getNTP(now, timeinfo)) {
          if ((timeinfo.tm_min % (60 / word_updates_per_hour) == 0) && (timeinfo.tm_hour >= 8 && timeinfo.tm_hour <= 19)) {
            reboot_count = 0;
            nextWordAPIMillis = (millis() + (3600 / word_updates_per_hour) * 1000) - 3000; //dont check again until nearly next word update time
            String word = wordOfTheDay();
            displayLastStoppedMillis = millis();
            debugf("Word, %02d:%02d, [%s]\n", timeinfo.tm_hour, timeinfo.tm_min, word);
            displayString(word);

            // For testing only (changes all characters and requires a drum rotation + calibration each time)
            // nextWordAPIMillis = (millis() + (3600 / word_updates_per_hour) * 1000) - 3000; //dont check again until nearly next word update time
            // String thisSeq = "@@@@@@@@@@@@";
            // charSeq--;
            // if (charSeq == 0) {
            //   charSeq = 39;
            // }
            // thisSeq.replace("@",String(letters[charSeq]));
            // displayString(thisSeq);

          }
        }
      }

      // Handle Button Press Code Here
      // else if (digitalRead(button1Pin) == 0) {
      //   do something;
      // }
    }
    //If display has been moving for more than 20 seconds, must be an error condition
    else if (millis() - displayLastStoppedMillis > 20000) {
      debugln(TXT_RED "Moving > 20 secs - RESTARTING!!!" TXT_RST);
      nvmem.magic = RTC_MAGIC;
      strncpy(nvmem.previous_display,save_display,13);
      nvmem.reboot_count = reboot_count;
      ESP.restart();
    }

    // Handle interactive serial commands over USB (used for debugging)
#if DEBUG == 1
    if (Serial.available()) {
      test_command = Serial.readStringUntil('\n');
      test_command.replace("\r","");
      test_command.toUpperCase();

      // if only return was pressed then repeat last command
      if (test_command.length() == 0) { 
        test_command = test_command_previous;
      }

      if (test_command.charAt(0) == char(92)) { //backslash
        print_test_menu();
      }
      else if (test_command.charAt(0) == ']') {
        test_num = test_command.substring(1,3).toInt();
        if (test_num >= 0) {
          active_menu_unit = test_num;
          debugf("Active unit set to %d\n", active_menu_unit);
        }
      }
      else if (test_command.charAt(0) == '>') {
        test_num = test_command.substring(1,5).toInt();
        if (test_num > 0) {
          debugf("Move %d flaps\n", test_num);
          splitFlap[active_menu_unit]->moveStepperbyFlap(test_num);
        }
      }
      else if (test_command.charAt(0) == '}') {
        test_num = test_command.substring(1,5).toInt();
        if (test_num > 0) {
            debugf("Move all flaps by %d\n", test_num);
            for (uint8_t unit = 0; unit < UNITCOUNT; unit++) {
              splitFlap[unit]->moveStepperbyFlap(test_num);
            }
        }
      }
      // Move stepper by a raw number of steps
      else if (test_command.charAt(0) == '~') {
        test_num = test_command.substring(1,5).toInt();
        if (test_num > 0) {
          debugf("Move %d steps\n", test_num);
          splitFlap[active_menu_unit]->moveStepperbyStep(test_num);
        }
      }
      else if (test_command.charAt(0) == '|') {
        ESP.restart();
      }
      else if (test_command.charAt(0) == '%') {
        String word = wordOfTheDay();
        debugf("Word, %02d:%02d, [%s]\n", timeinfo.tm_hour, timeinfo.tm_min, word);
        displayString(word);
      }   
      else if (test_command.charAt(0) == '+') {
        String thisSeq = "@@@@@@@@@@@@";
        // String thisSeq = "       @ ";
        charSeq--;
        if (charSeq == 0) {
          charSeq = 39;
        }
        thisSeq.replace("@",String(letters[charSeq]));
        displayString(thisSeq);
      }  
      else if (test_command.charAt(0) == '<') {
        debugln("Put ESP to sleep until power reset");
        esp_deep_sleep_start();
      }      
      else {
        test_command.toUpperCase();
        debugf("Display %s\n", test_command);
        displayString(test_command);
      }
      test_command_previous = test_command;
    }
#endif
  }

}

void print_test_menu() {
  getNTP(now, timeinfo);

  debugln(TXT_GREEN "----------------------------------");
  debugf ("     %s", asctime(&timeinfo));
  debugln("Enter a command followed by return");
  debugln("----------------------------------");
  debugln("\\   : Show this menu");
  debugln("}   : Move ALL forward number of flaps");
  debugf ("]   : Set active unit for this menu [%d]\n", active_menu_unit);
  debugln(">   : Move forward number of flaps");
  debugln("~   : Move forward number steps");
  debugln("|   : Reset Display");
  debugln("%   : Display a random word");
  debugln("+   : Test: countdown of all flaps");
  debugln("<   : Idle");
  debugln("any : display given text");
  debugln("----------------------------------" TXT_RST);
}

// Callback routine that actions the enable on or off triggered by setAutoEnable
bool setExternalPin(uint8_t pin, uint8_t value) {
  pin = pin & ~PIN_EXTERNAL_FLAG;

  // When using SLEEP instead of /ENABLE on the A4988 to save idle power, need to invert
  // debugf("mcp en pin %d set to %d\n", pin, value ^ 0x01);
  mcp_en_steppers.digitalWrite(pin, value ^ 0x01);

  return value;
}

void recalibrate_units() {
  uint8_t unitsCalibrating;
  int8_t calibrationResult;

  unitsCalibrating = 0;
  for (uint8_t unit = 0; unit < UNITCOUNT; unit++) {
    if (!splitFlap[unit]->calibrationComplete) {
      if (!splitFlap[unit]->calibrationStarted) {
        splitFlap[unit]->calibrateStart();
        unitsCalibrating++;
      }
      else {
        calibrationResult = splitFlap[unit]->calibrate();
        if (calibrationResult == 0) {
          unitsCalibrating++;
        }
        else if (calibrationResult < 0) {
          debugf("Calibration failed for unit %d\n", unit);
          debugln(TXT_RED "RESTARTING!!!" TXT_RST);
          nvmem.magic = RTC_MAGIC;
          strncpy(nvmem.previous_display,save_display,13);
          nvmem.reboot_count = reboot_count;
          ESP.restart();
        }
      }
    }
  }
}

void IRAM_ATTR sensor_ISR() {
    sensortriggered = true;
}

void updateHallSensors() {
  uint8_t newvalue;

  mcp_sensor.clearInterrupts();
  uint8_t sensor_port_current_a = mcp_sensor.readPort(MCP23017Port::A);
  uint8_t sensor_port_current_b = mcp_sensor.readPort(MCP23017Port::B);

  for (uint8_t unit = 0; unit < UNITCOUNT; unit++) {
    if (sensorPort[unit] == 'A') {
        newvalue = ((~sensor_port_current_a & sensorPortBit[unit]) == 0);
        if (!splitFlap[unit]->updateHallValue(newvalue)) {
          splitFlap[unit]->moveStepperbyFlap(1);
          splitFlap[unit]->calibrationComplete = false;
          splitFlap[unit]->calibrationStarted = false;
          splitFlap[unit]->pendingLetter = splitFlap[unit]->destinationLetter;
        }
    }
    else {
        newvalue = ((~sensor_port_current_b & sensorPortBit[unit]) == 0);
        if (!splitFlap[unit]->updateHallValue(newvalue)) {
          splitFlap[unit]->moveStepperbyFlap(1);
          splitFlap[unit]->calibrationComplete = false;
          splitFlap[unit]->calibrationStarted = false;
          splitFlap[unit]->pendingLetter = splitFlap[unit]->destinationLetter;
        }
    }
  }
}

void displayString(String display) {
  uint8_t test_length;
  char display_char;

  display.toUpperCase();
  strncpy(save_display, display.c_str(), 13); // save display in case of reboot

  test_length = display.length();
  if (test_length > UNITCOUNT) {
    test_length = UNITCOUNT;
  }
  for (uint8_t char_pos = 0; char_pos < test_length; char_pos++) {
    display_char = display.charAt(char_pos);
    splitFlap[char_pos]->moveSteppertoLetter(display_char);
  }
}

boolean diplayStillMoving () {
  boolean display_busy = true;
  display_busy = false;
  for (uint8_t unit = 0; unit < UNITCOUNT; unit++) {
    if (splitFlap[unit]->checkIfRunning()) {
      display_busy = true;
    }
  }
  return display_busy;
}

// void intToBinary(int num, char* binaryStr) {
//     for (int i = 7; i >= 0; i--) {
//         int bit = (num >> i) & 1;
//         binaryStr[7 - i] = bit + '0'; // Convert the bit to '0' or '1'
//     }
//     binaryStr[8] = '\0'; // Null-terminate the string
// }

// void debugUnitFlags(String prefix) {
//   for (uint8_t unit = 0; unit < UNITCOUNT; unit++) {
//     debugf("%s,%02d,%c,%d,%d\n", prefix, unit, (splitFlap[unit]->pendingLetter == 0) ? 95 : splitFlap[unit]->pendingLetter, splitFlap[unit]->calibrationStarted, splitFlap[unit]->calibrationComplete);
//   }
// }
