/*
 * ============================================================================
 * Embedded Dual-Mode Automatic Door Control and Security System
 * ============================================================================
 *
 * Project type : Embedded control system
 * Platform     : Arduino UNO R3
 * Motor driver : TB6612FNG
 * User input   : 4x4 matrix keypad
 * Sensing      : HC-SR04 ultrasonic sensor + door-position limit switches
 * User output  : 16x2 I2C LCD + passive buzzer
 *
 * System overview
 * --------------
 * This project implements an embedded door control system with two
 * operating modes:
 *
 *   1. AUTO MODE
 *      - Uses the HC-SR04 ultrasonic sensor to detect approaching objects.
 *      - Opens the door automatically when an object is detected.
 *      - Keeps the door open while an object remains in the detection zone.
 *      - Closes the door after the configured absence timeout.
 *      - Performs an emergency stop and re-opening when an obstacle is
 *        detected during normal closing.
 *
 *   2. SECURITY MODE
 *      - Requires password authentication for controlled door access.
 *      - Uses a timed slow-closing procedure with an audible warning.
 *      - Activates an alarm/lockdown sequence after repeated failed
 *        authentication attempts.
 *
 *   Both operating modes support authenticated switching between modes.
 *
 * Architectural principles
 * -------------------------
 * The firmware is organized around explicit system states and enumerations
 * rather than relying on a single monolithic control routine. Different
 * aspects of system behavior, including door operation, keypad interaction,
 * authentication purpose, motor control, and input conditions, are modeled
 * separately to keep the control logic structured and maintainable.
 *
 * Important note
 * --------------
 * This firmware is an academic/engineering prototype. The password is stored
 * directly in the firmware source, and no cryptographic authentication is provided.
 * Additional hardware safety mechanisms should be used for deployment in a
 * real access-control or industrial environment.
 * ============================================================================
 */

#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// ============================================================================
// STATE DEFINITIONS USING ENUMERATIONS
// ============================================================================

// Operating mode of the embedded door control system.
enum SYSTEM_MODE {
  AUTO_MODE,      // Fully automatic door operation.
  SECURITY_MODE   // Password-protected door operation and authenticated mode switching.
};

// Current interaction state of the keypad interface.
enum KEYPAD_ACTION {
  KEYPRESS_AWAITING,    // Waiting for the user to select a password-protected operation, such as door access in Security Mode or operating-mode switching.
  PASSWORD_ENTERING,    // Accepting password digits from the user.
  SWITCH_CONFIRMATION   // Waiting for confirmation before changing the operating mode.
};

// Purpose for which password authentication is being requested.
enum ENTERING_PURPOSE {
  OPEN_DOOR,            // Request to open the door.
  SWITCH_SYSTEM_MODE,   // Authenticate a request to change the system operating mode.
  NONE                  // No password operation is currently selected.
};

// High-level behavioral state of the door.
enum DOOR_BEHAVIOR {
  OPENED_COMPLETELY,    // Door has reached the fully-open position.
  OPENING_NORMALLY,     // Door is currently opening at normal speed.
  CLOSING_NORMALLY,     // Door is currently closing at normal speed.
  CLOSING_SLOWLY,       // Door is closing slowly while the warning buzzer is active.
  CLOSED_COMPLETELY,    // Door has reached the fully-closed position.
  EMERGENCY_STOPPED     // Door motion has been stopped before reaching an end position.
};

// Motor control command used by the TB6612FNG driver.
enum MOTOR_DIRECTION {
  CLOCKWISE,
  COUNTER_CLOCKWISE,
  BRAKE                 // Stop motor motion immediately.
};

// Limit-switch state when the Arduino internal pull-up resistor is used.
enum LIMIT_SWITCH_STATE {
  IS_PRESSED,      // Switch is pressed; the input is LOW.
  IS_NOT_PRESSED   // Switch is released; the input is HIGH.
};

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

void displaySystemMode();                 // Display the current operating mode on the LCD.
void clearPasswordBuffer();               // Clear the password input buffer and reset its cursor.
void enterPassword(char);                 // Process a keypad input during password entry.
void requestPassword();                   // Display the password-entry prompt.
bool isObjectDetected();                  // Measure distance with the HC-SR04 and determine whether an object is present.
void activateAlarm();                     // Activate the alarm after repeated password-authentication failures.
void verifyPassword();                    // Validate the entered password against the configured default password.
void requestConfirm();                    // Display the confirmation prompt for changing the operating mode.
void confirmModeSwitch(char);             // Process the user's confirmation or cancellation of a mode change.
void setMotorDirection(MOTOR_DIRECTION);  // Apply the requested motor direction or brake command.
void emergencyStop();                     // Immediately stop door motion and update the door state.
void openDoorNormally();                  // Open the door at the normal motor speed.
void closeDoorNormally();                 // Close the door at the normal motor speed.
void closeDoorSlowly();                   // Close the door slowly while generating an audible warning.
void keyPressFeedback(char);              // Provide audible feedback for a keypad button press.
void processSecurityMode(char);           // Execute the high-level control logic for Security Mode.
void processAutoMode(char);               // Execute the high-level control logic for Auto Mode.

// ============================================================================
// 4x4 KEYPAD INTERFACE
// ============================================================================

const int ROW_NUM = 4;    // Number of keypad rows.
const int COLUMN_NUM = 4; // Number of keypad columns.

// Password-entry control keys:
// A: Delete the most recently entered digit.
// D: Submit the entered password.

// Keys used to select the purpose of password authentication:
// B: Request door access while Security Mode is active.
// C: Request a switch between Auto Mode and Security Mode.

// Keys used to confirm or cancel a operating-mode change:
// *: Confirm the mode change.
// #: Cancel the mode change.
char keys[ROW_NUM][COLUMN_NUM] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Arduino pin assignments for keypad rows and columns.
byte pin_rows[ROW_NUM] = {14, 12, 9, 8};
byte pin_column[COLUMN_NUM] = {13, 11, 2, 7};

Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM );

// ============================================================================
// LCD USER INTERFACE
// ============================================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ============================================================================
// PASSWORD AUTHENTICATION VARIABLES
// ============================================================================

const char defaultPassword[] = "1234";
int attempts = 0;
int passwordCursor = 0;
char password[5] = {' ', ' ', ' ', ' ', '\0'};

// ============================================================================
// ARDUINO UNO R3 PIN ASSIGNMENTS
// ============================================================================

// Passive buzzer module
const int buzzerSignal = 6;        // PWM-capable output used to drive the passive buzzer.

// HC-SR04 ultrasonic distance sensor.
const int trigPin = 15;            // Trigger signal
const int echoPin = 16;            // Echo signal

// Limit switch
const int doorOpenSwitch = 3;       // Limit switch indicating the fully-open door position.
const int doorCloseSwitch = 4;      // Limit switch indicating the fully-closed door position.

// TB6612FNG motor-driver control signals.
const int motorInputCtrlPin1 = 17;  // Motor direction control input.
const int motorInputCtrlPin2 = 10;  // Motor direction control input.
const int motorSpeedCtrl = 5;       // PWM output used to control motor speed.

// ============================================================================
// RUNTIME SYSTEM STATE
// ============================================================================

SYSTEM_MODE systemMode = AUTO_MODE;               // System starts in Auto Mode.
KEYPAD_ACTION keypadAction = KEYPRESS_AWAITING;   // Keypad starts in the idle state, waiting for a user command.
ENTERING_PURPOSE enteringPurpose = NONE;          // No password-related operation is active initially.
DOOR_BEHAVIOR doorBehavior = CLOSED_COMPLETELY;              // The system assumes the door is initially fully closed.
bool isMotorSpinning = false;
bool isSlowClosingWarning = false;    // Indicates whether the slow-closing warning is active.
bool isWarningBuzzerOn = false;       // Tracks whether the warning buzzer is currently sounding.
bool isUltrasonicEnabled = true;         // Ultrasonic sensing is enabled by default.

// Timing variables used to manage timed state transitions.
unsigned long motorStartTime = 0;   // Timestamp recorded when motor motion begins.
unsigned long warningBuzzerOnStartTime = 0; // Timestamp recorded when the warning buzzer starts sounding.
unsigned long warningBuzzerOffStartTime = 0;  // Timestamp recorded when the warning buzzer stops sounding.
unsigned long doorOpenedTime = 0;   // Timestamp used to determine how long the door has remained open.

void displaySystemMode() {
  lcd.setCursor(0, 0);
  if(systemMode == AUTO_MODE) {
    lcd.print("AUTO MODE       ");
  }
  else {
    lcd.print("SECURITY MODE   ");
  }
}

void clearPasswordBuffer() {
  for(int i = 0; i < 4; i++) {
    password[i] = ' ';
  }
  passwordCursor = 0;
}

void enterPassword(char key) {
  if(key == 'A') {
    if(passwordCursor > 0) {
      password[--passwordCursor] = ' ';
      lcd.setCursor(passwordCursor, 1);
      lcd.print(' ');
      lcd.setCursor(passwordCursor, 1);
    }
  }
  else if(key == 'D') {
    verifyPassword();
  }
  else if('0' <= key && key <= '9') {
    if(passwordCursor > 3) return;
    password[passwordCursor] = key;
    lcd.print(key);
    delay(100);

    lcd.setCursor(passwordCursor, 1);
    lcd.print("*");

    ++passwordCursor;
  }
}

void activateAlarm() {
  // Run the alarm sequence for approximately 10 seconds.  
  for(int i = 0; i < 10; i++) {
    lcd.setCursor(0, 0);
    lcd.print("SYSTEM ALARM !! ");
    lcd.setCursor(0, 1);
    lcd.print("LOCKDOWN        ");

    // Sound the alarm at approximately 1.5 kHz.
    tone(buzzerSignal, 1500);

    delay(800);

    lcd.clear();

    noTone(buzzerSignal);

    delay(200);
  }
}

void confirmModeSwitch(char key) {
  // Continue only when one of the two confirmation keys is pressed.
  if(key == '*' || key == '#') {
    lcd.clear();

    // Confirm the requested system-mode change.
    if(key == '*') {
      lcd.print("SWITCHING");
      delay(500);
      for(int i = 0; i < 3; i++) {
        lcd.print(".");
        delay(500);        
      }

      lcd.clear();
      delay(500);

      lcd.setCursor(0, 0);
      lcd.print("SWITCH COMPLETE");
      delay(300);

      lcd.setCursor(0, 1);
      if(systemMode == AUTO_MODE) {
        systemMode = SECURITY_MODE;
        lcd.print("SYSTEM SECURED");
      }
      else {
        systemMode = AUTO_MODE;
        lcd.print("SYSTEM RESTORED");
      }

      delay(1000);
    }
    
    // Cancel the requested system-mode change.
    else {
      lcd.print("SWITCH CANCELLED");
      delay(1000);
    }

    lcd.clear();
    delay(500);
    
    // Synchronize ultrasonic sensing with the newly selected operating mode.
    if(systemMode == AUTO_MODE) {
      isUltrasonicEnabled = true; // Auto Mode requires ultrasonic sensing to remain enabled.
    }
    else {
      isUltrasonicEnabled = false; // Security Mode disables automatic ultrasonic activation while the door is closed.
    }
    keypadAction = KEYPRESS_AWAITING;  // Return the keypad interface to its idle state.
    enteringPurpose = NONE;            // Clear the current password-entry purpose.
  }
}

void requestConfirm() {
  lcd.setCursor(0, 0);
  lcd.print("CONFIRM SWITCH  ");
  delay(1000);

  lcd.clear();
  delay(500);

  lcd.print("*: YES   #: NO  ");
  lcd.setCursor(0, 1);
}

void verifyPassword() {
  ++attempts;
  
  bool isCorrectPass = true;
  for(int i = 0; i < 4; i++) {
    if(defaultPassword[i] != password[i]) {
      isCorrectPass = false;
      break;
    }
  }

  lcd.clear();
  clearPasswordBuffer();
  
  if(isCorrectPass) {
    lcd.print("ACCESS GRANTED");
    attempts = 0;

    // Provide positive audible feedback for successful authentication.
    tone(buzzerSignal, 3000);
    delay(1000);
    noTone(buzzerSignal);
    
    if(enteringPurpose == SWITCH_SYSTEM_MODE) {
      keypadAction = SWITCH_CONFIRMATION; // Transition to mode-change confirmation after successful authentication.
      requestConfirm();
    }
    else {
      // In Security Mode, a valid password may open a fully closed door.
      if(doorBehavior == CLOSED_COMPLETELY && systemMode == SECURITY_MODE) {
        openDoorNormally();
      }

      keypadAction = KEYPRESS_AWAITING;   // Return the keypad interface to its idle state.
      enteringPurpose = NONE;             // Clear the current password-entry purpose.
    }
  }
  else {
    if(attempts < 4) {
      lcd.print("ACCESS DENIED");
      
      // Provide three short beeps to indicate failed authentication.
      for(int i = 0; i < 3; i++) {
        tone(buzzerSignal, 3000);
        delay(100);
        noTone(buzzerSignal);
        delay(100);
      }
    }
    else {
      activateAlarm();	// Start the alarm/lockdown sequence after repeated failed attempts.
      attempts = 0;		// Reset the failed-attempt counter after the alarm sequence.
    }

    keypadAction = PASSWORD_ENTERING;
    requestPassword();	// Prompt the user to enter the password again.
  }
}

void requestPassword() {
  lcd.setCursor(0, 0);
  lcd.print("ENTER PASSWORD  ");
  lcd.setCursor(0, 1);
}

bool isObjectDetected() {
  unsigned long duration;
  double distance;

  // Ensure the trigger line is LOW before starting a new ultrasonic measurement.
  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);

  // Send the 10 µs trigger pulse required by the HC-SR04.
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read the echoPin, pulseIn() returns the duration (length of the pulse) in microseconds
  // Wait for up to 30 ms; pulseIn() returns 0 when no echo is received.
  duration = pulseIn(echoPin, HIGH, 30000);

  // Convert echo duration to distance in centimeters using the speed of sound.
  distance = duration * 0.034 / 2;

  if(0 < distance && distance < 17) {
    return true;  // An object is within the configured detection range.
  }
  return false;   // No object is detected within the configured range.
}

void setMotorDirection(MOTOR_DIRECTION motorDirection) {
  // Motor direction is defined by the physical installation of the mechanism.
  // Command clockwise motor rotation.
  if(motorDirection == CLOCKWISE) {
    digitalWrite(motorInputCtrlPin1, HIGH);
    digitalWrite(motorInputCtrlPin2, LOW);
  }
  // Command counter-clockwise motor rotation.
  else if (motorDirection == COUNTER_CLOCKWISE) {
    digitalWrite(motorInputCtrlPin1, LOW);
    digitalWrite(motorInputCtrlPin2, HIGH);
  }
  // Apply the brake/stop command.
  else {
    digitalWrite(motorInputCtrlPin1, LOW);
    digitalWrite(motorInputCtrlPin2, LOW);
  }
}

void emergencyStop() {
  // Stop motor motion immediately.
  setMotorDirection(BRAKE);
  analogWrite(motorSpeedCtrl, 0);
  isMotorSpinning = false;

  if(doorBehavior == CLOSING_SLOWLY) {
    isSlowClosingWarning = false;   // Disable the slow-closing warning when motion is interrupted.
  }

  // Record an intermediate stop as an emergency-stopped door state.
  if(doorBehavior == OPENING_NORMALLY || doorBehavior == CLOSING_NORMALLY || doorBehavior == CLOSING_SLOWLY) {
    doorBehavior = EMERGENCY_STOPPED;
  }
}

void openDoorNormally() {
  LIMIT_SWITCH_STATE doorOpenSwitState = (digitalRead(doorOpenSwitch) == LOW) ? IS_PRESSED : IS_NOT_PRESSED;
  LIMIT_SWITCH_STATE doorCloseSwitState = (digitalRead(doorCloseSwitch) == LOW) ? IS_PRESSED : IS_NOT_PRESSED;

  if(isMotorSpinning == false) {
      // Update the door behavioral state before starting motion.
      doorBehavior = OPENING_NORMALLY;
      
      setMotorDirection(COUNTER_CLOCKWISE);
      analogWrite(motorSpeedCtrl, 250);

      isMotorSpinning = true;
      motorStartTime = millis(); // Record the start time of the opening motion.
  }
  else {
    if(doorOpenSwitState == IS_PRESSED && doorCloseSwitState == IS_NOT_PRESSED) {
      setMotorDirection(BRAKE);
      analogWrite(motorSpeedCtrl, 0);
      isMotorSpinning = false;    // The open limit has been reached; stop the motor and update the state.
      doorBehavior = OPENED_COMPLETELY;

      doorOpenedTime = millis();  // Record when the door entered the fully-open state.
    }
  }
}

void closeDoorNormally() {
  LIMIT_SWITCH_STATE doorOpenSwitState = (digitalRead(doorOpenSwitch) == LOW) ? IS_PRESSED : IS_NOT_PRESSED;
  LIMIT_SWITCH_STATE doorCloseSwitState = (digitalRead(doorCloseSwitch) == LOW) ? IS_PRESSED : IS_NOT_PRESSED;

  if(isMotorSpinning == false) {
      doorBehavior = CLOSING_NORMALLY;
      
      setMotorDirection(CLOCKWISE);
      analogWrite(motorSpeedCtrl, 250);

      isMotorSpinning = true;
      motorStartTime = millis(); // Record the start time of the closing motion.
  }
  else {
    if(doorOpenSwitState == IS_NOT_PRESSED && doorCloseSwitState == IS_PRESSED) {
      setMotorDirection(BRAKE);
      analogWrite(motorSpeedCtrl, 0);
      isMotorSpinning = false;  // The closed limit has been reached; stop the motor and update the state.
      doorBehavior = CLOSED_COMPLETELY;
    }
  }
}

void closeDoorSlowly() {
  // Close the door at reduced speed while issuing a warning after the configured
// Security Mode timeout has elapsed without detecting an object.
  // Warning pattern: BEEP -> SILENCE -> BEEP -> SILENCE.

  LIMIT_SWITCH_STATE doorOpenSwitState = (digitalRead(doorOpenSwitch) == LOW) ? IS_PRESSED : IS_NOT_PRESSED;
  LIMIT_SWITCH_STATE doorCloseSwitState = (digitalRead(doorCloseSwitch) == LOW) ? IS_PRESSED : IS_NOT_PRESSED;

  if(isMotorSpinning == false) {
      doorBehavior = CLOSING_SLOWLY;
      
      setMotorDirection(CLOCKWISE);
      analogWrite(motorSpeedCtrl, 90);

      isMotorSpinning = true;
      motorStartTime = millis(); // Record the start time of the closing motion.

      isSlowClosingWarning = true;               // Enable the slow-closing warning.

      warningBuzzerOnStartTime = millis();       // Record the beginning of the warning-buzzer interval.
      tone(buzzerSignal, 2700);
      isWarningBuzzerOn = true;                  // Warning buzzer is currently active.
  }
  else {
    if(doorOpenSwitState == IS_NOT_PRESSED && doorCloseSwitState == IS_PRESSED) {
      setMotorDirection(BRAKE);
      analogWrite(motorSpeedCtrl, 0);
      isMotorSpinning = false;          // The closed limit has been reached; stop the motor and update the state.
      doorBehavior = CLOSED_COMPLETELY;
      
      isSlowClosingWarning = false;     // Disable the slow-closing warning.
    }
  }

  // Generate the periodic slow-closing warning pattern.
  if(isSlowClosingWarning == true) {
    if(isWarningBuzzerOn == true) {
      // Stop the buzzer after 500 ms of sound.
      if(millis() - warningBuzzerOnStartTime >= 500) {
        noTone(buzzerSignal);
        warningBuzzerOffStartTime = millis(); // Record the beginning of the silent interval.
        isWarningBuzzerOn = false;            // Warning buzzer is currently silent.
      }
      else {
        // Maintain the current warning-buzzer interval.
      }
    }
    else {
      // Restart the buzzer after 500 ms of silence.
      if(millis() - warningBuzzerOffStartTime >= 500) {
        tone(buzzerSignal, 2700);
        warningBuzzerOnStartTime = millis();  // Record the beginning of the new warning-buzzer interval.
        isWarningBuzzerOn = true;             // Restart the warning buzzer.
      }
      else {
        // Maintain the current silent interval.
      }
    }
  }
  // Ensure the buzzer remains silent when the warning is disabled.
  else {
    noTone(buzzerSignal);
    isWarningBuzzerOn = false;  // Warning buzzer is fully disabled.
  }
}

void keyPressFeedback(char key) {
  if(key != 'D') {
    tone(buzzerSignal, 3000);
    delay(100);
    noTone(buzzerSignal);
  }
}

// ============================================================================
// Security Mode: password-protected access and controlled mode management. CONTROL LOGIC
// ============================================================================
void processSecurityMode(char key) {
  switch(doorBehavior) {
    case OPENED_COMPLETELY:
      if(isUltrasonicEnabled == true) {
        // Close immediately when an object is detected.
        if(isObjectDetected() == true) {
          isUltrasonicEnabled = false; // Temporarily disable ultrasonic sensing while the door is moving.
          closeDoorNormally();      // Continue with normal-speed closing.
        }
        else {
          // If no object is detected for 10 seconds, begin slow closing with an audible warning.
          if(millis() - doorOpenedTime >= 10000) {
            isUltrasonicEnabled = false; // Temporarily disable ultrasonic sensing while the door is moving.
            closeDoorSlowly();        // Close slowly while generating the audible warning.
          }
        }
      }
      else {
        isUltrasonicEnabled = true; // Re-enable ultrasonic sensing.
      }
      break;
    case OPENING_NORMALLY:
      openDoorNormally();
      break;
    case CLOSING_NORMALLY:
      closeDoorNormally();
      break;
    case CLOSING_SLOWLY:
      closeDoorSlowly();
      break;
    case CLOSED_COMPLETELY:
      if(key) {
        keyPressFeedback(key);  // Provide immediate audible feedback for keypad input.

        switch(keypadAction) {
          case KEYPRESS_AWAITING:
            // B: authenticate a request to open the door.
            // C: authenticate a request to change the operating mode.
            if(key == 'B' || key == 'C') {
              if(key == 'B') {
                enteringPurpose = OPEN_DOOR;
              }
              else {
                enteringPurpose = SWITCH_SYSTEM_MODE;
              }
              requestPassword();            // Display the password-entry prompt.
              keypadAction = PASSWORD_ENTERING; // Transition the keypad interface into password-entry mode.
            }
            break;

          case PASSWORD_ENTERING:
            enterPassword(key);
            break;

          case SWITCH_CONFIRMATION:
            confirmModeSwitch(key);
            break;
        }
      }
      break;
  }
}

// ============================================================================
// Auto Mode: sensor-driven automatic door operation. CONTROL LOGIC
// ============================================================================
void processAutoMode(char key) {
  switch(doorBehavior) {
    case OPENED_COMPLETELY:
      // Refresh the open-door timer whenever an object remains in the detection zone.
      if(isObjectDetected() == true) {
        doorOpenedTime = millis();  // Reset the open-door timer based on the latest object detection.
      }
      else {
        // Close at normal speed after 2 seconds without detecting an object.
        if(millis() - doorOpenedTime >= 2000) {
          closeDoorNormally();
        }
      }
      break;
    case OPENING_NORMALLY:
      openDoorNormally(); // Continue the current opening motion.
      break;
    case CLOSING_NORMALLY:
      if(isObjectDetected() == true) {
        emergencyStop();    // Stop immediately when an obstacle is detected during closing.
        openDoorNormally(); // Re-open the door after the emergency stop.
      }
      else {
        closeDoorNormally();  // Continue normal-speed closing.
      }
      break;
    case CLOSED_COMPLETELY:
      if(isUltrasonicEnabled == true) {
        if(isObjectDetected() == true) {
          openDoorNormally(); // Open the door automatically when an object is detected.
        }
        else {
          // When the door is fully closed, allow the user to request a system-mode change.
          if(keypadAction == KEYPRESS_AWAITING) {
            if(key) {
              keyPressFeedback(key);  // Provide immediate audible feedback for keypad input.

              if(key == 'C') {
                isUltrasonicEnabled = false;   // Temporarily disable ultrasonic sensing so keypad mode-switch confirmation can proceed.
                enteringPurpose = SWITCH_SYSTEM_MODE;
                keypadAction = PASSWORD_ENTERING;

                requestPassword();  // Display the password-entry prompt.
              }
            }
          }
        }
      }
      // Ultrasonic sensing is temporarily disabled so keypad interaction can proceed
// without competing with automatic door detection.
      else {
        // Process the authenticated request to change the operating mode.
        if(key && enteringPurpose == SWITCH_SYSTEM_MODE) {
          keyPressFeedback(key);  // Provide immediate audible feedback for keypad input.

          switch(keypadAction) {
            // Accept password input for the mode-change request.
            case PASSWORD_ENTERING:
              enterPassword(key);
              break;
                      
            case SWITCH_CONFIRMATION:
              confirmModeSwitch(key);
              break;
          }
        }
      }
      break;
  }
}

void setup(){
  Serial.begin(9600);

  // Configure all I/O pins used by the embedded controller.
  pinMode(buzzerSignal, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(doorOpenSwitch, INPUT_PULLUP);  // Internal pull-up is enabled; the switch common terminal is connected to GND.
  pinMode(doorCloseSwitch, INPUT_PULLUP); // Internal pull-up is enabled; the switch common terminal is connected to GND.
  pinMode(motorInputCtrlPin1, OUTPUT);
  pinMode(motorInputCtrlPin2, OUTPUT);
  pinMode(motorSpeedCtrl, OUTPUT);
  
  // Initialize the I2C LCD and enable its backlight.
  lcd.init();
  lcd.backlight();
  delay(1000);
  
  // Display the system boot sequence.
  lcd.print("BOOTING");
  delay(300);
  for(int i = 0; i < 3; i++) {
    lcd.print(".");
    delay(300);
  }

  lcd.clear();
  delay(200);
  
  // Display the system activation message.
  lcd.print("AUTOMATIC DOOR SYSTEM IS ACTIVATED");
  delay(200);
  for(int i = 0; i < 24; i++) {
    lcd.scrollDisplayLeft();
    delay(130);
  }
  delay(130);

  lcd.noDisplay();
  delay(500);

  lcd.display();
  lcd.clear();
}

void loop(){  
  // Show the current system mode whenever the keypad is idle.
  if(keypadAction == KEYPRESS_AWAITING) {
    displaySystemMode();
  }

  char key = keypad.getKey();

  // The controller implements two operating modes: Auto Mode and Security Mode.
  // Each mode applies its own control rules to door motion, sensing, and user interaction.
  // Security Mode: password-protected access and controlled mode management.
  if(systemMode == SECURITY_MODE) {
    // Execute the Security Mode control state machine.
    processSecurityMode(key);
  }
  // Auto Mode: sensor-driven automatic door operation.
  else {
    // Execute the Auto Mode control state machine.
    processAutoMode(key);
  }
}
