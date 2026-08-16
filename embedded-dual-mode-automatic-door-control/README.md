# Embedded Dual-Mode Automatic Door Control and Security System

A small embedded-system project built with an **Arduino UNO R3** to control a motorized door in two operating modes:

* **Auto Mode** — the door opens automatically when an object is detected.
* **Security Mode** — opening the door requires password authentication.

The system uses an ultrasonic sensor for object detection, mechanical limit switches for door-position feedback, and a TB6612FNG motor driver for DC motor control. A 16×2 I2C LCD and a passive buzzer provide basic user feedback.

This project is developed as an **academic embedded-systems prototype**, so the design focuses on understanding the interaction between sensors, actuators, the microcontroller, and firmware state logic rather than on production-level safety or security.

---

## 1. System Overview

The main controller is an Arduino UNO R3.

### Main components

| Component                    | Function                                      |
| ---------------------------- | --------------------------------------------- |
| Arduino UNO R3               | Main controller                               |
| 4×4 matrix keypad            | User input and password entry                 |
| HC-SR04                      | Object detection                              |
| TB6612FNG                    | DC motor driver                               |
| DC motor                     | Door actuation                                |
| 2× mechanical limit switches | Fully-open and fully-closed position feedback |
| 16×2 I2C LCD                 | Display system status and messages            |
| Passive buzzer               | Key feedback, warning, and alarm              |
| 9 V / 3 A adapter            | Main power source                             |
| LM2596 buck converter        | Converts 9 V input to a 5 V system supply     |

The firmware is written in Arduino C++ using the `Keypad` and `LiquidCrystal_I2C` libraries.

---

## 2. Hardware Architecture

The system is divided into three main parts:

```text
INPUT DEVICES  →  CONTROLLER  →  OUTPUT DEVICES
                     |
                 Arduino UNO
```

### Input devices

* 4×4 matrix keypad
* HC-SR04 ultrasonic sensor
* Open-position limit switch
* Closed-position limit switch

### Controller

* Arduino UNO R3

The Arduino reads the sensors and keypad, determines the current system and door states, and generates the required control signals.

### Output devices

* TB6612FNG motor driver
* DC motor
* 16×2 I2C LCD
* Passive buzzer

The motor driver is controlled by two direction signals and one PWM speed signal.

---

## 3. Power Supply

The system uses a **9 V / 3 A external adapter** as the main power source.

```text
9 V / 3 A Adapter
        │
        ▼
   LM2596 Buck
     Converter
        │
        ▼
      5 V
   ┌────┴────┐
   ▼         ▼
Rail 1     Rail 2
  5 V        5 V
```

The LM2596 converts the 9 V input to approximately 5 V.

The project diagram shows two 5 V power rails. These are **two distribution rails from the same LM2596 output**, not two independent voltage levels.

* **Power rail 1** is mainly used for the TB6612FNG motor-driver supply.
* **Power rail 2** supplies the Arduino-side peripherals such as the HC-SR04, LCD, and buzzer.

The grounds are shared between the circuits.

> The actual motor supply in this prototype is also derived from the 5 V rail. The suitability of this supply depends on the selected DC motor and its current requirements.

---

## 4. Hardware Block Diagram

![Hardware block diagram](hardware-block-diagram.png)

The block diagram shows the connection between the input devices, Arduino UNO, motor driver, and output devices.

The Arduino communicates with:

* Keypad through digital I/O
* HC-SR04 through `Trig` and `Echo`
* Limit switches through digital inputs
* TB6612FNG through direction and PWM control signals
* LCD through I2C
* Buzzer through a digital/PWM-capable output

---

## 5. Arduino Pin Assignment

The current firmware uses the following Arduino UNO R3 pins.

| Arduino pin | Connected device    | Function              |
| ----------- | ------------------- | --------------------- |
| A0 / 14     | Keypad              | Row 1                 |
| A1 / 15     | HC-SR04             | Trigger               |
| A2 / 16     | HC-SR04             | Echo                  |
| A3 / 17     | TB6612FNG           | Motor input 1         |
| D2          | Keypad              | Column 3              |
| D3          | Open limit switch   | Fully-open position   |
| D4          | Closed limit switch | Fully-closed position |
| D5          | TB6612FNG           | PWM motor speed       |
| D6          | Passive buzzer      | Buzzer signal         |
| D7          | Keypad              | Column 4              |
| D8          | Keypad              | Row 4                 |
| D9          | Keypad              | Row 3                 |
| D10         | TB6612FNG           | Motor input 2         |
| D11         | Keypad              | Column 2              |
| D12         | Keypad              | Row 2                 |
| D13         | Keypad              | Column 1              |

The LCD uses the Arduino I2C interface.

The two limit switches use the Arduino's internal pull-up resistors:

```cpp
pinMode(doorOpenSwitch, INPUT_PULLUP);
pinMode(doorCloseSwitch, INPUT_PULLUP);
```

Therefore, a pressed switch is read as `LOW`.

---

## 6. Operating Modes

The firmware implements two operating modes.

### Auto Mode

In Auto Mode, the HC-SR04 is used to detect an object near the door.

Basic operation:

```text
Door closed
     │
     ▼
Object detected?
   │       │
  No      Yes
   │       │
   │       ▼
   │    Open door
   │       │
   │       ▼
   │   Door fully open
   │       │
   │       ▼
Object still detected?
   │       │
  Yes      No
   │       │
Reset      ▼
timer   Wait 2 seconds
           │
           ▼
       Close door
```

The configured detection distance is approximately **17 cm**.

When the door is fully open:

* The timer is reset while an object remains in the detection zone.
* If no object is detected for **2 seconds**, the door starts closing.

During normal closing:

* If an object is detected, the motor is stopped.
* The door is then commanded to open again.

This provides a simple obstacle-response mechanism.

---

### Security Mode

Security Mode requires password authentication for controlled door access.

When the door is fully closed:

* `B` requests door opening.
* `C` requests a change of operating mode.
* The user enters a four-digit password.
* `D` submits the password.
* `A` deletes the last entered digit.

A successful password for door access opens the door.

A successful password for a mode change is followed by an additional confirmation:

```text
*  → Confirm
#  → Cancel
```

The ultrasonic sensor is not continuously used for automatic opening while the door is closed in Security Mode.

---

## 7. Security Mode Door Closing

After the door has been opened in Security Mode, the system monitors the ultrasonic sensor.

If an object is detected, the door closes normally according to the current control logic.

If no object is detected for **10 seconds**, the system starts a slow-closing procedure.

During slow closing:

* Motor PWM is reduced.
* The buzzer produces a periodic warning.
* The closed-position limit switch stops the motor when the door reaches the closed position.

The warning pattern is approximately:

```text
BEEP → SILENCE → BEEP → SILENCE → ...
```

Each interval is approximately 500 ms.

---

## 8. Password and Alarm Logic

The current firmware uses a four-digit password stored directly in the source code.

```cpp
const char defaultPassword[] = "1234";
```

The password is not encrypted or hashed.

Failed authentication attempts produce an audible indication. After repeated failures, the alarm sequence is activated.

The alarm:

* Displays a system alarm message on the LCD.
* Activates the buzzer.
* Runs for approximately 10 seconds.
* Temporarily represents a lockdown state.

This is intended as a simple demonstration of authentication and alarm handling, not as a secure access-control implementation.

---

## 9. Firmware Structure

Instead of implementing all behavior in one large routine, the firmware separates several concepts into enumerated states.

### System mode

```cpp
enum SYSTEM_MODE {
    AUTO_MODE,
    SECURITY_MODE
};
```

### Keypad interaction

```cpp
enum KEYPAD_ACTION {
    KEYPRESS_AWAITING,
    PASSWORD_ENTERING,
    SWITCH_CONFIRMATION
};
```

### Authentication purpose

```cpp
enum ENTERING_PURPOSE {
    OPEN_DOOR,
    SWITCH_SYSTEM_MODE,
    NONE
};
```

### Door behavior

```cpp
enum DOOR_BEHAVIOR {
    OPENED_COMPLETELY,
    OPENING_NORMALLY,
    CLOSING_NORMALLY,
    CLOSING_SLOWLY,
    CLOSED_COMPLETELY,
    EMERGENCY_STOPPED
};
```

This makes the main control logic easier to divide between Auto Mode and Security Mode.

The main loop selects the appropriate control routine:

```cpp
if (systemMode == SECURITY_MODE) {
    processSecurityMode(key);
}
else {
    processAutoMode(key);
}
```

---

## 10. Main Control Functions

Some of the main firmware functions are:

| Function                | Purpose                            |
| ----------------------- | ---------------------------------- |
| `processAutoMode()`     | Handles Auto Mode behavior         |
| `processSecurityMode()` | Handles Security Mode behavior     |
| `isObjectDetected()`    | Measures distance using HC-SR04    |
| `openDoorNormally()`    | Starts or continues normal opening |
| `closeDoorNormally()`   | Starts or continues normal closing |
| `closeDoorSlowly()`     | Performs slow closing with warning |
| `emergencyStop()`       | Stops motor motion                 |
| `verifyPassword()`      | Checks the entered password        |
| `activateAlarm()`       | Runs the alarm sequence            |
| `confirmModeSwitch()`   | Handles mode-switch confirmation   |

---

## 11. Door Position Feedback

Two mechanical limit switches provide feedback about the door's end positions:

```text
                 Door movement
                     │
        ┌────────────┴────────────┐
        ▼                         ▼
  Open limit switch        Close limit switch
        │                         │
        └────────── Arduino ──────┘
```

The switches allow the firmware to determine when the door has reached:

* Fully open
* Fully closed

The Arduino uses `INPUT_PULLUP`, so the corresponding input becomes `LOW` when the switch is pressed.

This prevents the motor from continuing to run after the door reaches an end position.

---

## 12. System Flowchart

![System flowchart](system-flowchart.png)

The flowchart describes the main behavior of the firmware, including:

* Initial system startup
* Mode selection
* Auto Mode object detection
* Door opening and closing
* Obstacle handling
* Security Mode password entry
* Failed authentication
* Alarm activation
* Door access
* Operating-mode switching

The implementation is based on repeated execution of the Arduino `loop()` function rather than a separate real-time operating system.

---

## 13. Power Architecture

![Power architecture](power-architecture.png)

The power architecture separates the physical distribution of the 5 V supply into two rails.

### Power rail 1

Used primarily for the motor-driver side:

```text
LM2596 5 V
    │
    ▼
Power rail 1
    │
    └── TB6612FNG VM
```

### Power rail 2

Used for the controller and peripheral devices:

```text
LM2596 5 V
    │
    ▼
Power rail 2
    ├── HC-SR04
    ├── LCD
    ├── Passive buzzer
    └── Arduino-side circuits
```

Both rails originate from the same LM2596 output and share the system ground.

---

## 14. Libraries

The firmware currently uses:

```cpp
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
```

These libraries are used for:

* `LiquidCrystal_I2C` — communication with the 16×2 I2C LCD.
* `Keypad` — scanning the 4×4 matrix keypad.

---

## 15. Running the Project

### Hardware

Connect the components according to the hardware block diagram and verify the following before powering the system:

1. The LM2596 output is adjusted to approximately 5 V.
2. All circuit grounds are connected correctly.
3. The limit switches are connected according to the `INPUT_PULLUP` configuration.
4. The motor driver receives the required supply voltage.
5. The motor current is within the capability of the power supply and motor driver.
6. The motor direction matches the physical opening and closing mechanism.

### Firmware

1. Open the `.ino` file in Arduino IDE.
2. Install the required libraries.
3. Select **Arduino UNO** as the board.
4. Select the correct serial port.
5. Upload the firmware.
6. Test the limit switches before running continuous motor operation.

---

## 16. Keypad Layout

The keypad is configured as follows:

```text
┌───┬───┬───┬───┐
│ 1 │ 2 │ 3 │ A │
├───┼───┼───┼───┤
│ 4 │ 5 │ 6 │ B │
├───┼───┼───┼───┤
│ 7 │ 8 │ 9 │ C │
├───┼───┼───┼───┤
│ * │ 0 │ # │ D │
└───┴───┴───┴───┘
```

Key functions:

| Key   | Function                              |
| ----- | ------------------------------------- |
| `0–9` | Enter password digits                 |
| `A`   | Delete last password digit            |
| `B`   | Request door opening in Security Mode |
| `C`   | Request operating-mode switch         |
| `D`   | Submit password                       |
| `*`   | Confirm mode switch                   |
| `#`   | Cancel mode switch                    |

---

## 17. Current Limitations

This project is a prototype for studying embedded control and does not provide the safety or security features expected from a real automatic door.

Some current limitations are:

* The password is stored directly in the firmware.
* There is no cryptographic authentication.
* The HC-SR04 is used as the main obstacle-detection sensor.
* There is no independent hardware emergency-stop circuit.
* The motor power system is simplified for the prototype.
* The firmware uses blocking functions such as `delay()` and `pulseIn()`.
* The system does not include a dedicated real-time operating system.
* Mechanical and electrical protection depends on the prototype hardware.

These limitations are acceptable for a student project, but they would need to be addressed before using a similar design in a real access-control or safety-critical application.

---

## 18. Project Structure

The current project is intentionally kept relatively small:

```text
project/
├── Modified_edition.ino
├── README.md
├── hardware-block-diagram.png
├── power-architecture.png
└── system-flowchart.png
```

The firmware is currently contained in a single Arduino `.ino` file because the project is small enough that splitting it into multiple source modules is not necessary yet.

---

## 19. Project Purpose

The main purpose of this project is to practice the design of a small embedded control system by combining:

* Digital input handling
* Sensor interfacing
* Motor control
* PWM
* I2C communication
* Keypad scanning
* User-interface feedback
* State-based control logic
* Password authentication
* Basic fault and obstacle handling

The project focuses on the relationship between **hardware behavior and firmware logic**, rather than simply demonstrating individual Arduino components.

---

## License

This project is released under the MIT License. See [`LICENSE`](LICENSE).

---

## Author

**Vu Xuan Thien**

Computer Engineering / Integrated Circuit Design / Embedded Systems
