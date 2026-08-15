# System Architecture

## Architectural Overview

The system is organized into five major layers:

```text
┌───────────────────────────────────────────────┐
│              User Interaction                │
│         Keypad + LCD + Buzzer                │
├───────────────────────────────────────────────┤
│             Control Application              │
│       Auto Mode / Security Mode              │
├───────────────────────────────────────────────┤
│              State Management                │
│ Door / Keypad / Authentication / Motor State │
├───────────────────────────────────────────────┤
│             Hardware Interface               │
│ GPIO / PWM / I2C / Ultrasonic Timing         │
├───────────────────────────────────────────────┤
│                  Hardware                   │
│ Arduino / Sensors / Driver / Motor / Switch │
└───────────────────────────────────────────────┘
```

## Responsibilities

### Input Layer

Responsible for acquiring:

- Keypad commands
- Ultrasonic measurements
- Mechanical limit-switch states

### Control Layer

Responsible for:

- Mode selection
- Authentication
- Door behavior
- Safety responses
- Timing decisions

### Output Layer

Responsible for:

- Motor direction and speed
- LCD messages
- Audible feedback

## Design Principle

The design separates **what the system should do** from **how individual hardware devices are driven**.

For example:

```text
processAutoMode()
        │
        ▼
openDoorNormally()
        │
        ▼
setMotorDirection()
        │
        ▼
TB6612FNG
        │
        ▼
DC Motor
```

This separation allows the high-level control logic to remain understandable while hardware-specific actions remain localized.

