# Embedded Dual-Mode Automatic Door Control and Security System

An Arduino UNO R3-based embedded control system that combines **automatic door operation, password-protected access, obstacle handling, and operating-mode management**.

The project is designed as a small but complete embedded-control system: sensors provide information about the physical environment, the keypad provides human commands, the firmware maintains explicit system states, and the motor driver converts those decisions into physical door movement.

> **The main idea:** the door is not simply "a motor that opens and closes".  
> It is a state-driven system whose behavior changes according to the operating mode, user intent, authentication result, sensor conditions, and physical position of the door.

---

## 1. What This Project Does

The system has two operating modes:

```text
                    EMBEDDED DOOR SYSTEM
                            |
                +-----------+-----------+
                |                       |
                v                       v
           AUTO MODE              SECURITY MODE
                |                       |
        HC-SR04 detection        Keypad + password
                |                       |
                v                       v
        Automatic operation      Controlled access
                |                       |
                +-----------+-----------+
                            |
                            v
                     Door controller
                            |
                            v
                      DC motor + door
```

### Auto Mode

Auto Mode is intended for hands-free operation.

- The HC-SR04 detects an object approaching the door.
- A closed door opens automatically when an object is detected within the configured range.
- The door remains open while an object is detected.
- After **2 seconds without detection**, the door begins normal-speed closing.
- If an obstacle is detected while the door is closing, the controller:
  1. immediately brakes the motor,
  2. enters an emergency-stopped state,
  3. commands the door to reopen.

This mode prioritizes automatic interaction and obstacle response.

### Security Mode

Security Mode changes the door from an automatic-access system into a password-controlled system.

- `B` requests door access.
- `C` requests an operating-mode change.
- Password authentication is required for both operations.
- A correct password can open a fully closed door.
- Four failed password attempts activate an alarm/lockdown sequence.
- When the door is open, the HC-SR04 can trigger closing when an object is detected.
- If no object is detected for **10 seconds**, the system begins slow closing with a periodic audible warning.
- Switching between Auto Mode and Security Mode requires an additional confirmation step.

---

## 2. System Concept

The system is built around one central idea:

> **Separate the physical state of the door from the reason the controller is operating.**

The firmware therefore does not use one giant state variable containing every possible combination.

Instead, it models several independent dimensions:

```text
SYSTEM_MODE
    What operating policy is active?

KEYPAD_ACTION
    What is the keypad currently doing?

ENTERING_PURPOSE
    Why is authentication being requested?

DOOR_BEHAVIOR
    What is the physical door currently doing?

MOTOR_DIRECTION
    What motor command is being applied?

LIMIT_SWITCH_STATE
    What is the electrical state of a limit switch?
```

For example, the controller can conceptually be in:

```text
SECURITY_MODE
+
PASSWORD_ENTERING
+
OPEN_DOOR
+
CLOSED_COMPLETELY
```

These states answer four different questions instead of forcing everything into one large state machine.

This separation is the central architectural idea behind the firmware.

---

## 3. System Architecture

![System Flowchart](docs/images/system-flowchart.png)

The overall runtime flow is:

```text
                       +----------------+
                       |  Arduino UNO   |
                       |   Main Loop    |
                       +-------+--------+
                               |
                       +-------v--------+
                       |  System Mode   |
                       +-------+--------+
                               |
                 +-------------+-------------+
                 |                           |
                 v                           v
             AUTO MODE                SECURITY MODE
                 |                           |
          HC-SR04 sensing             Keypad / Password
                 |                           |
                 +-------------+-------------+
                               |
                               v
                        Door behavior
                               |
                +--------------+--------------+
                |              |              |
                v              v              v
           Limit switches   Motor driver   LCD/Buzzer
                               |
                               v
                            DC Motor
```

The Arduino UNO R3 is the controller. It receives inputs from the keypad, ultrasonic sensor, and limit switches; processes them according to the current operating mode and door state; and drives the motor, LCD, and buzzer.

---

## 4. Hardware Architecture

![Hardware Block Diagram](docs/images/hardware-block-diagram.png)

### Main components

| Component | Role |
|---|---|
| Arduino UNO R3 | Main embedded controller |
| 4×4 Matrix Keypad | User commands and password entry |
| HC-SR04 | Object/obstacle detection |
| 2× Mechanical Limit Switches | Fully-open and fully-closed position feedback |
| TB6612FNG | Dual H-bridge motor driver |
| DC Motor | Door actuator |
| 16×2 I2C LCD | User interface and system status |
| Passive Buzzer | Key feedback, authentication feedback, warning, and alarm |
| 9 V / 3 A Adapter | External power source |
| LM2596 Buck Converter | Converts the adapter output to the 5 V system supply |

### Power distribution

![Power Architecture](docs/images/power-architecture.png)

The power design uses:

```text
9 V / 3 A Adapter
        |
        v
LM2596 Buck Converter
        |
        v
       5 V
        |
        +-------- Power Rail 1
        |
        +-------- Power Rail 2
```

The two rails represent **two physical 5 V distribution paths**, not two different voltage levels. They share the same 5 V source and common ground.

This arrangement keeps the wiring organized while allowing the controller and peripheral sections to be powered from the same regulated supply.

---

## 5. Hardware Interfaces

### Arduino UNO R3

The Arduino is responsible for:

- reading the keypad,
- measuring the HC-SR04,
- reading the limit switches,
- controlling the TB6612FNG,
- driving the buzzer,
- updating the LCD,
- maintaining the system state,
- executing the main control loop.

### Keypad

The 4×4 keypad is used for:

```text
0–9  -> password digits

A    -> delete the last password digit
D    -> submit password

B    -> request door access in Security Mode
C    -> request system-mode change

*    -> confirm mode change
#    -> cancel mode change
```

### HC-SR04

The ultrasonic sensor is used for object detection.

The firmware considers an object detected when the measured distance is:

```text
0 cm < distance < 17 cm
```

The sensor is primarily responsible for automatic operation in Auto Mode and for door-closing decisions while the door is open in Security Mode.

### Limit switches

Two mechanical switches provide physical end-position feedback:

```text
Open limit switch
        |
        v
Fully-open door position

Close limit switch
        |
        v
Fully-closed door position
```

The switches use the Arduino's internal pull-up resistors:

```text
Switch pressed    -> LOW
Switch released   -> HIGH
```

### TB6612FNG

The motor driver receives logical direction and PWM commands from the Arduino.

The firmware uses three logical motor commands:

```text
CLOCK_WISE
COUNTER_CLOCK_WISE
BRAKE
```

Normal and slow movement are distinguished by PWM:

```text
Normal speed  -> PWM 250
Slow closing  -> PWM 90
Brake         -> PWM 0
```

The actual clockwise/counter-clockwise physical direction depends on the motor's mechanical installation.

### LCD

The 16×2 I2C LCD provides:

- current operating mode,
- password prompts,
- access results,
- mode-switch confirmation,
- alarm status,
- activation messages.

### Passive buzzer

The buzzer provides several forms of feedback:

```text
Short beep
    -> keypad feedback

Positive tone
    -> successful authentication

Three short beeps
    -> failed authentication

Alternating warning
    -> slow closing

Alarm tone
    -> repeated authentication failure
```

---

## 6. Software Architecture

The firmware is organized as a **state-oriented, single-loop embedded control system**.

The main execution path is:

```text
setup()
   |
   v
Initialize hardware
   |
   v
loop()
   |
   +--> Display current mode
   |
   +--> Read keypad
   |
   +--> Check SYSTEM_MODE
           |
           +--> AUTO_MODE
           |      |
           |      v
           |  processAutoMode()
           |
           +--> SECURITY_MODE
                  |
                  v
              processSecurityMode()
```

There is no RTOS and no multitasking framework. All control logic is executed cooperatively from the Arduino `loop()`.

---

## 7. Door Behavioral Model

The physical door is represented by:

```text
DOOR_BEHAVIOR
├── CLOSED_COMPLETELY
├── OPENING_NORMALLY
├── OPENED_COMPLETELY
├── CLOSING_NORMALLY
├── CLOSING_SLOWLY
└── EMERGENCY_STOPPED
```

The basic movement cycle is:

```text
CLOSED_COMPLETELY
        |
        | open request
        v
OPENING_NORMALLY
        |
        | open limit reached
        v
OPENED_COMPLETELY
        |
        | close request
        v
CLOSING_NORMALLY
        |
        | close limit reached
        v
CLOSED_COMPLETELY
```

Auto Mode adds the safety path:

```text
CLOSING_NORMALLY
        |
        | obstacle detected
        v
EMERGENCY_STOPPED
        |
        | reopen
        v
OPENING_NORMALLY
```

Security Mode adds:

```text
OPENED_COMPLETELY
        |
        | 10 s without object
        v
CLOSING_SLOWLY
        |
        | close limit reached
        v
CLOSED_COMPLETELY
```

The door's physical behavior is therefore independent from the reason that caused the movement.

---

## 8. User Interaction and Authentication

Authentication is modeled separately from door movement.

### Keypad states

```text
KEYPAD_ACTION
├── KEYPRESS_AWAITING
├── PASSWORD_ENTERING
└── SWITCH_CONFIRMATION
```

### Authentication purpose

```text
ENTERING_PURPOSE
├── OPEN_DOOR
├── SWITCH_SYSTEM_MODE
└── NONE
```

The general authentication flow is:

```text
User command
     |
     v
Request password
     |
     v
PASSWORD_ENTERING
     |
     v
Verify password
     |
     +----------+----------+
     |                     |
   Correct              Incorrect
     |                     |
     v                     v
Perform request       Retry / alarm
```

For mode switching:

```text
Password correct
       |
       v
SWITCH_CONFIRMATION
       |
    +--+--+
    |     |
    *     #
    |     |
 Confirm Cancel
```

This prevents an accidental or incomplete authentication request from immediately changing the operating mode.

---

## 9. Auto Mode Behavior

The Auto Mode control philosophy is:

> **Detect → open → remain open while occupied → close when clear → reopen if obstructed.**

### Closed door

```text
CLOSED_COMPLETELY
        |
        +-- Object detected
        |       |
        |       v
        |   OPENING_NORMALLY
        |
        +-- No object
                |
                v
             remain closed
```

### Open door

When the door reaches the fully-open limit:

```text
OPENED_COMPLETELY
        |
        +-- Object detected
        |       |
        |       v
        |   reset open timer
        |
        +-- No object for 2 s
                |
                v
        CLOSING_NORMALLY
```

### Obstacle during closing

```text
CLOSING_NORMALLY
        |
        | obstacle detected
        v
EMERGENCY_STOPPED
        |
        v
OPENING_NORMALLY
```

This is the core safety behavior of Auto Mode.

---

## 10. Security Mode Behavior

Security Mode changes the control philosophy from automatic access to authenticated access.

### Door access

```text
Door closed
    |
    v
Press B
    |
    v
Enter password
    |
    +-- Correct -> open door
    |
    +-- Incorrect -> retry
                    |
                    +-- 4th failure -> alarm
```

### Mode switching

```text
Press C
   |
   v
Enter password
   |
   +-- Incorrect -> retry
   |
   +-- Correct
         |
         v
Confirm switch
     |
   +---+---+
   |       |
   *       #
   |       |
 Switch   Cancel
```

A successful mode switch also synchronizes ultrasonic sensing with the selected operating mode.

---

## 11. Security Mode Closing Strategy

Security Mode uses a different closing policy from Auto Mode.

When the door is fully open:

```text
                    OPENED_COMPLETELY
                           |
                  +--------+--------+
                  |                 |
           Object detected     No object
                  |                 |
                  v                 v
         Close normally       Wait 10 seconds
                                    |
                                    v
                           Close slowly + warning
```

Slow closing uses:

```text
Motor PWM = 90

Buzzer:
    500 ms ON
    500 ms OFF
    repeated
```

This creates an audible warning during the slow-closing phase.

---

## 12. Authentication Failure Handling

The controller counts password attempts.

```text
Incorrect password
       |
       v
attempts < 4 ?
    /       \
  yes        no
   |          |
   v          v
Retry       Alarm
              |
              v
          Lockdown sequence
```

The alarm sequence runs for approximately 10 seconds and uses the LCD and buzzer to indicate the security event.

The attempt counter is reset after a successful authentication or after the alarm sequence.

---

## 13. Timing Behavior

The important timing parameters currently implemented in the firmware are:

| Parameter | Value | Purpose |
|---|---:|---|
| Auto Mode absence timeout | 2 s | Start normal closing |
| Security Mode absence timeout | 10 s | Start slow closing |
| Slow-closing buzzer ON | 500 ms | Warning interval |
| Slow-closing buzzer OFF | 500 ms | Warning interval |
| Object detection range | < 17 cm | HC-SR04 detection threshold |
| Normal motor PWM | 250 | Normal door movement |
| Slow motor PWM | 90 | Slow closing |
| Password attempts | 4 | Trigger alarm after repeated failures |

The continuous door-control timing uses `millis()`.

Some user-interface, feedback, and alarm sequences use `delay()`, which temporarily blocks the main loop.

---

## 14. Initial State

After startup, the firmware initializes the logical state as:

```text
SYSTEM_MODE       = AUTO_MODE
KEYPAD_ACTION     = KEYPRESS_AWAITING
ENTERING_PURPOSE  = NONE
DOOR_BEHAVIOR     = CLOSED_COMPLETELY
isUltrasonicEnabled = true
```

The software therefore assumes that the physical door is fully closed when the controller starts.

This is an important prototype assumption: a production system should verify the actual physical position during startup rather than relying only on software state.

---

## 15. Pin Mapping

### Keypad

| Signal | Arduino pin |
|---|---:|
| Row 1 | A0 / D14 |
| Row 2 | D12 |
| Row 3 | D9 |
| Row 4 | D8 |
| Column 1 | D13 |
| Column 2 | D11 |
| Column 3 | D2 |
| Column 4 | D7 |

### Sensors and actuators

| Device | Signal | Arduino pin |
|---|---|---:|
| HC-SR04 | TRIG | A1 / D15 |
| HC-SR04 | ECHO | A2 / D16 |
| Open limit switch | Input | D3 |
| Close limit switch | Input | D4 |
| Passive buzzer | Signal | D6 |
| TB6612FNG | Motor input 1 | A3 / D17 |
| TB6612FNG | Motor input 2 | D10 |
| TB6612FNG | PWM / speed | D5 |
| LCD 1602 I2C | SDA | A4 |
| LCD 1602 I2C | SCL | A5 |

---

## 16. Firmware Structure

The firmware is currently implemented as a single Arduino sketch.

The main logical responsibilities are:

| Function | Responsibility |
|---|---|
| `loop()` | Main runtime dispatcher |
| `processAutoMode()` | Auto Mode control logic |
| `processSecurityMode()` | Security Mode control logic |
| `openDoorNormally()` | Normal-speed opening |
| `closeDoorNormally()` | Normal-speed closing |
| `closeDoorSlowly()` | Slow closing and warning |
| `emergencyStop()` | Motor braking and emergency state |
| `setMotorDirection()` | Motor-driver abstraction |
| `isObjectDetected()` | HC-SR04 measurement |
| `enterPassword()` | Keypad password entry |
| `verifyPassword()` | Authentication |
| `confirmModeSwitch()` | Mode-switch confirmation |
| `activateAlarm()` | Failed-authentication alarm |
| `displaySystemMode()` | LCD mode display |
| `keyPressFeedback()` | Keypad feedback |

The code therefore follows a simple hierarchy:

```text
loop()
 |
 +-- System Mode
      |
      +-- processAutoMode()
      |      |
      |      +-- sensor decisions
      |      +-- door state
      |      +-- motor commands
      |
      +-- processSecurityMode()
             |
             +-- keypad decisions
             +-- authentication
             +-- door state
             +-- motor commands
```

---

## 17. Why the Architecture Is Organized This Way

A single monolithic state machine could describe every combination of operating mode, authentication, keypad interaction, and door movement.

However, that would quickly produce states such as:

```text
AUTO_CLOSED
AUTO_OPENING
AUTO_OPENED
AUTO_CLOSING
SECURITY_CLOSED
SECURITY_OPENING
SECURITY_OPENED
SECURITY_CLOSING
SECURITY_PASSWORD_ENTRY
SECURITY_MODE_CONFIRMATION
...
```

This approach mixes concepts that are logically different.

The current architecture instead separates them:

```text
SYSTEM_MODE
      +
KEYPAD_ACTION
      +
ENTERING_PURPOSE
      +
DOOR_BEHAVIOR
      +
Runtime flags
```

This makes the firmware easier to reason about:

```text
What policy is active?
        -> SYSTEM_MODE

What is the user interface doing?
        -> KEYPAD_ACTION

Why are we authenticating?
        -> ENTERING_PURPOSE

What is the physical door doing?
        -> DOOR_BEHAVIOR
```

This is the main software-design principle of the project.

---

## 18. Project Structure

A minimal repository structure is intentionally used:

```text
embedded-dual-mode-automatic-door/
│
├── .github/
│
├── docs/
│   └── images/
│       ├── power-architecture.png
│       ├── hardware-block-diagram.png
│       └── system-flowchart.png
│
├── firmware/
│   └── embedded_dual_mode_automatic_door/
│       └── embedded_dual_mode_automatic_door.ino
│
├── media/
│
├── .gitignore
├── LICENSE
└── README.md
```

The README is intentionally the primary documentation entry point. The diagrams in `docs/images/` provide visual detail without requiring separate architecture documents.

---

## 19. Limitations and Engineering Notes

This project is an **academic/engineering prototype**, not a certified industrial or access-control product.

### Security

- The password is currently stored directly in firmware source code.
- No cryptographic authentication is implemented.
- No secure credential storage is used.
- The alarm/lockdown behavior is software-based.

### Control

- The controller is single-threaded.
- Some operations use blocking `delay()` calls.
- A blocking operation temporarily prevents the main loop from processing new events.

### Sensors

- HC-SR04 detection depends on distance, object geometry, placement, and environmental conditions.
- The ultrasonic sensor is not a certified safety sensor.

### Startup

- The initial software state assumes the door is fully closed.
- A more robust system should establish the actual physical position during startup.

### Mechanical safety

A real deployment would require additional safety mechanisms such as appropriate emergency-stop hardware, current/overload protection, fail-safe behavior, independent obstacle detection, and mechanical safety measures.

---

## 20. Future Improvements

Possible next steps include:

- Replace blocking `delay()` sequences with non-blocking timers.
- Separate the firmware into multiple source modules.
- Add startup door-position verification.
- Improve password storage and authentication security.
- Add persistent configuration for system parameters.
- Add more robust obstacle detection.
- Introduce explicit fault states for sensor or actuator failures.
- Add motor-current or stall detection.
- Improve event handling so safety inputs can be processed during user-interface operations.
- Add automated tests for the state-transition logic.

---

## 21. Demonstration Scenarios

### Scenario A — Automatic opening

```text
Door closed
    ↓
Object approaches
    ↓
HC-SR04 detects object
    ↓
Door opens
    ↓
Open limit reached
    ↓
Door remains open while object is detected
```

### Scenario B — Automatic closing

```text
Door open
    ↓
No object detected
    ↓
2 seconds elapsed
    ↓
Normal closing
    ↓
Close limit reached
    ↓
Door closed
```

### Scenario C — Obstacle during Auto Mode closing

```text
Door closing
    ↓
Obstacle detected
    ↓
Emergency brake
    ↓
Door reopens
```

### Scenario D — Password-protected access

```text
Security Mode
    ↓
Press B
    ↓
Enter password
    ↓
Correct password
    ↓
Door opens
```

### Scenario E — Repeated authentication failure

```text
Wrong password
    ↓
Retry
    ↓
Wrong password
    ↓
Retry
    ↓
Wrong password
    ↓
Retry
    ↓
4th failure
    ↓
Alarm / lockdown sequence
```

### Scenario F — Switching operating modes

```text
Press C
    ↓
Password authentication
    ↓
Correct password
    ↓
Mode-switch confirmation
    |
    +-- * -> Switch mode
    |
    +-- # -> Cancel
```

---

## 22. Summary

This project demonstrates a complete embedded-control loop:

```text
        HUMAN / ENVIRONMENT
                |
        +-------+-------+
        |               |
      Keypad          HC-SR04
        |               |
        +-------+-------+
                |
                v
        +---------------+
        |  Arduino UNO  |
        |               |
        | State Model   |
        | Control Logic |
        +-------+-------+
                |
       +--------+--------+
       |        |        |
       v        v        v
     Motor     LCD     Buzzer
       |
       v
      Door
       |
       v
 Limit Switches
       |
       +-----------> Arduino
```

The project is fundamentally about **closed-loop embedded control**:

> **Sense the environment → interpret the current state → apply the appropriate control policy → drive the actuator → observe the physical result → transition to the next state.**

Auto Mode demonstrates sensor-driven control and obstacle response. Security Mode demonstrates human interaction, authentication, mode management, timed behavior, and alarm handling.

Together, these mechanisms turn a simple DC motor into a small embedded system with **state, interaction, sensing, decision-making, actuation, and safety-oriented behavior**.

## License

This project is released under the MIT License. See [`LICENSE`](LICENSE).

---

## Author

**Vu Xuan Thien**

Computer Engineering / Integrated Circuit Design / Embedded Systems
