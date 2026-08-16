# Embedded Dual-Mode Automatic Door Control and Security System

> An embedded automatic door control system integrating sensor-based automation, password-protected access control, obstacle detection, motor actuation, user interaction, and audible safety feedback.

![Hardware Block Diagram](docs/images/hardware-block-diagram.png)

## Overview

This project implements a centralized embedded door-control system built around an **Arduino UNO R3**.

The controller integrates:

- 4×4 matrix keypad for user input
- HC-SR04 ultrasonic  for object detection
- TB6612FNG dual H-bridge motor driver
- DC motor for door actuation
- Two mechanical limit switches for end-position feedback
- 16×2 I2C LCD for user-interface feedback
- Passive buzzer for interaction and safety warnings
- 9 V / 3 A external power supply with LM2596 buck conversion to a regulated 5 V supply distributed across two power rails

The firmware is organized around explicit system states and operating modes instead of a single monolithic control routine.

---

## Key Features

- **Dual operating modes**
  - Auto Mode
  - Security Mode
- Password-based access control
- Authenticated switching between operating modes
- Ultrasonic object detection
- Obstacle detection during door closing
- Emergency stop and automatic re-opening
- Mechanical end-position detection through limit switches
- PWM-based motor speed control
- Slow-closing behavior with audible warning
- Failed-password alarm and temporary lockout behavior
- LCD-based system-status and user feedback
- Modular, state-oriented firmware organization
- Non-blocking timing for door and warning-state decisions using `millis()` where applicable

---

## System Architecture

The system follows a **centralized embedded-control architecture**.

The Arduino UNO R3 acts as the primary controller responsible for:

1. Reading user input from the keypad
2. Acquiring distance information from the ultrasonic sensor
3. Monitoring door-position limit switches
4. Managing authentication and operating-mode transitions
5. Generating motor-control signals
6. Updating the LCD interface
7. Generating audible feedback and warnings

### Hardware Architecture

![Hardware Block Diagram](docs/images/hardware-block-diagram.png)

### Control Flow

The firmware separates high-level operating modes from door behavioral states and keypad interaction states.

![System Flowchart](docs/images/system-flowchart.png)

Detailed documentation:

- [`docs/hardware-design.md`](docs/hardware-design.md)
- [`docs/software-architecture.md`](docs/software-architecture.md)
- [`docs/control-flow.md`](docs/control-flow.md)

---

## Operating Modes

### AUTO MODE

Auto Mode provides sensor-driven automatic door operation.

Typical sequence:

1. Monitor the HC-SR04 ultrasonic sensor.
2. Detect an object within the configured range.
3. Open the door automatically.
4. Keep the door open while an object remains detected.
5. Start normal closing after the configured absence timeout.
6. Monitor for obstacles while the door is closing.
7. Stop and re-open the door if an obstacle is detected.

### SECURITY MODE

Security Mode provides password-controlled access and system-mode management.

Typical sequence:

1. Wait for a user command through the keypad.
2. Request password authentication for protected operations.
3. Validate the entered credential.
4. Grant or reject the requested operation.
5. Provide audible feedback for keypad interaction and authentication.
6. Activate an alarm/lockdown sequence after repeated failed attempts.
7. Allow authenticated switching between Auto Mode and Security Mode.

---

## Software Architecture

The firmware uses explicit enumerations to represent independent aspects of system state.

### System Mode

```text
SYSTEM_MODE
├── AUTO_MODE
└── SECURITY_MODE
```

### Keypad Interaction

```text
KEYPAD_ACTION
├── KEYPRESS_AWAITING
├── PASSWORD_ENTERING
└── SWITCH_CONFIRMATION
```

### Authentication Purpose

```text
ENTERING_PURPOSE
├── OPEN_DOOR
├── SWITCH_SYSTEM_MODE
└── NONE
```

### Door Behavioral State

```text
DOOR_BEHAVIOR
├── OPENED_COMPLETELY
├── OPENING_NORMALLY
├── CLOSING_NORMALLY
├── CLOSING_SLOWLY
├── CLOSED_COMPLETELY
└── EMERGENCY_STOPPED
```

### Motor Command

```text
MOTOR_DIRECTION
├── CLOCK_WISE
├── COUNTER_CLOCK_WISE
└── BRAKE
```

This separation makes the control logic easier to reason about because operating mode, user interaction, authentication purpose, mechanical state, and motor command are not represented by one overloaded variable.

---

## Engineering Highlights

| Area | Implementation |
|---|---|
| Controller | Arduino UNO R3 |
| Operating modes | Auto / Security |
| User input | 4×4 matrix keypad |
| Distance sensing | HC-SR04 ultrasonic sensor |
| Position feedback | Open/close limit switches |
| Motor driver | TB6612FNG |
| Motor control | Direction + PWM speed control |
| Display | 16×2 I2C LCD |
| Audible feedback | Passive buzzer |
| Authentication | Password-based |
| Fault response | Obstacle stop + re-opening |
| Warning behavior | Slow closing + audible warning |
| Firmware architecture | Explicit state-oriented control |

---

## Hardware Components

| Component | Function | Interface |
|---|---|---|
| Arduino UNO R3 | Central embedded controller | GPIO / PWM / I2C |
| 4×4 Keypad | User command and password input | Digital GPIO |
| HC-SR04 | Object / obstacle detection | Digital I/O |
| TB6612FNG | DC motor driver | Digital + PWM |
| DC Motor | Door actuation | H-bridge output |
| Limit Switch ×2 | Mechanical end-position feedback | Digital input |
| LCD 1602 I2C | User interface | I2C |
| Passive Buzzer | Audible feedback / warning | PWM-capable GPIO |
| LM2596 Buck Converter | 9 V → 5 V power conversion | Power |
| 9 V / 3 A Adapter | Primary external power source | Power |

---

## Power Architecture

The current design uses a single regulated **5 V system rail**, generated from the external 9 V supply through an LM2596 buck converter.

![Power Architecture](docs/images/power-architecture.png)

The hardware diagram separates the distribution visually into two 5 V power rails. These rails should be understood as **5 V distribution branches**, not independent voltage sources.

---

## Pin Configuration

The following mapping is derived from the current firmware.

### 4×4 Keypad

| Signal | Arduino Pin |
|---|---:|
| Row 1 | D14 / A0 |
| Row 2 | D12 |
| Row 3 | D9 |
| Row 4 | D8 |
| Column 1 | D13 |
| Column 2 | D11 |
| Column 3 | D2 |
| Column 4 | D7 |

### HC-SR04

| Signal | Arduino Pin |
|---|---:|
| TRIG | D15 / A1 |
| ECHO | D16 / A2 |

### Door Position Limit Switches

| Signal | Arduino Pin |
|---|---:|
| Fully open | D3 |
| Fully closed | D4 |

The firmware uses `INPUT_PULLUP`; the switch common terminal is therefore connected to GND.

### TB6612FNG

| Signal | Arduino Pin |
|---|---:|
| Motor input 1 | D17 / A3 |
| Motor input 2 | D10 |
| PWM speed control | D5 |

### LCD 1602 I2C

| Signal | Arduino Pin |
|---|---:|
| SDA | A4 |
| SCL | A5 |

### Passive Buzzer

| Signal | Arduino Pin |
|---|---:|
| Signal | D6 |

---

## Keypad Interface

The keypad assigns dedicated keys to system operations:

| Key | Function |
|---|---|
| `A` | Delete the most recently entered password digit |
| `D` | Submit password |
| `B` | Request protected door access in Security Mode |
| `C` | Request a system-mode change |
| `*` | Confirm mode change |
| `#` | Cancel mode change |

Password input is masked on the LCD.

---

## Safety and Fault Handling

The firmware incorporates several mechanisms intended to reduce unsafe door behavior:

- Mechanical end-position detection using limit switches
- Obstacle detection during normal closing
- Emergency motor stop
- Automatic re-opening after obstacle detection
- Slow closing with audible warning
- Password-attempt limitation
- Alarm behavior after repeated authentication failures

These mechanisms are intended for an academic engineering prototype and should not be interpreted as certification for safety-critical or industrial deployment.

---

## Design Decisions

### Why use explicit states?

Door control involves several simultaneous dimensions: operating mode, keypad interaction, authentication purpose, mechanical position, and motor command.

Representing these dimensions explicitly makes transitions easier to understand, test, and extend.

### Why use both ultrasonic sensing and limit switches?

The ultrasonic sensor provides **environmental information** about objects near the door.

The limit switches provide **deterministic mechanical feedback** indicating that the door has reached a physical end position.

These sensors therefore serve different control purposes.

### Why use the TB6612FNG?

The TB6612FNG provides independent direction and PWM speed control for a DC motor while keeping the motor-drive stage separate from the Arduino's logic-level control signals.

### Why use a regulated 5 V rail?

The system is designed around 5 V logic and peripherals. The LM2596 buck converter provides two regulated lower-voltage rails from the external 9 V supply.

---

## Installation and Setup

### Requirements

- Arduino IDE
- Arduino UNO R3
- `Keypad` library
- `LiquidCrystal_I2C` library
- Hardware components listed above

### Firmware

Open:

```text
firmware/embedded_dual_mode_automatic_door_control_security_system.ino
```

Install the required Arduino libraries, select the correct Arduino UNO board and serial port, then upload the firmware.

### Initial Configuration

Before deployment, verify:

- Motor direction relative to the physical door mechanism
- Limit-switch wiring and active states
- HC-SR04 detection range
- Motor PWM values
- LCD I2C address
- Password configuration
- Power-rail wiring
- Common ground between logic and driver control circuitry

---

## Project Structure

```text
embedded-dual-mode-automatic-door-control/
│
├── README.md
├── LICENSE
├── .gitignore
│
├── docs/
│   ├── hardware-design.md
│   ├── software-architecture.md
│   ├── control-flow.md
│   └── images/
│       ├── hardware-block-diagram.png
|       ├── power-architecture.png
│       └── system-flowchart.png
│
├── firmware/
│   └── embedded_dual_mode_automatic_door_control_security_system.ino
│
└── media/
```

---

## Limitations

This project is an **academic engineering prototype**, not a commercial access-control product.

Current limitations include:

- Password credentials are stored directly in firmware.
- No cryptographic authentication is implemented.
- No secure credential-management mechanism is provided.
- The system does not include redundant safety sensors.
- Mechanical safety depends on the physical door mechanism.
- No motor-current sensing is implemented.
- No encoder-based position feedback is currently used.
- The control system has not been certified for industrial or safety-critical operation.

These limitations are documented intentionally to distinguish the current prototype from a production-grade access-control system.

---

## Future Improvements

Potential engineering extensions include:

- Secure credential storage
- Password-change functionality
- Non-volatile configuration storage
- Persistent authentication lockout
- Additional redundant obstacle sensors
- Motor-current sensing
- Encoder-based position feedback
- Dedicated PCB implementation
- Watchdog-based fault recovery
- Event logging
- Remote monitoring and control
- More formal verification and fault-injection testing

---

## Project Status

**Status:** Engineering Prototype

| Subsystem | Status |
|---|---|
| Hardware architecture | Documented |
| Firmware architecture | Implemented |
| Motor control | Implemented |
| Ultrasonic sensing | Implemented |
| Limit-switch feedback | Implemented |
| Keypad interface | Implemented |
| LCD interface | Implemented |
| Auto Mode | Implemented |
| Security Mode | Implemented |
| Password authentication | Implemented |
| Alarm handling | Implemented |
| Engineering documentation | Included |

---

## Repository Development Strategy

The repository is intended to evolve with the engineering project.

Recommended milestone structure:

```text
Initial project structure
        ↓
Hardware interface implementation
        ↓
Motor-control implementation
        ↓
Sensor integration
        ↓
User-interface implementation
        ↓
Auto Mode
        ↓
Security Mode
        ↓
Authentication and alarm handling
        ↓
Safety and fault-response logic
        ↓
Documentation review and final cleanup
        ↓
v1.0.0 engineering release
```

---

## License

This project is released under the MIT License. See [`LICENSE`](LICENSE).

---

## Author

**Vu Xuan Thien**

Computer Engineering / Integrated Circuit Design / Embedded Systems
