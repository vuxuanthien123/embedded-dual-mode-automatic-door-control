# Hardware Design

## 1. Purpose

This document describes the hardware architecture of the Embedded Dual-Mode Automatic Door Control and Security System.

The design uses a centralized Arduino UNO R3 controller and separates the system into input devices, control logic, motor actuation, user-interface outputs, and power distribution.

## 2. Functional Hardware Blocks

```text
                         POWER SUPPLY
                              │
                    9 V / 3 A Adapter
                              │
                              ▼
                     LM2596 Buck Converter
                              │
                              ▼
                             +5 V
                              │
        ┌─────────────────────┼────────────────────────┐
        │                     │                        │
        ▼                     ▼                        ▼
   Input Devices          Controller              Output Stage
        │                     │                        │
        ├─ 4×4 Keypad ───────►│                        │
        ├─ HC-SR04 ───────────►│                        │
        └─ Limit Switches ────►│                        │
                              │                        │
                              ├──── PWM / Direction ──► TB6612FNG
                              │                         │
                              ├──── I2C ──────────────► LCD
                              │
                              └──── PWM ──────────────► Buzzer
                                                        │
                                                        ▼
                                                       Motor
```

## 3. Power Architecture

The external adapter provides 9 V DC. An LM2596 buck converter generates the regulated 5 V system rail.

The two power rails shown in the block diagram represent separate distribution branches of the same regulated 5 V source.

They should not be interpreted as two independent voltage domains.

## 4. Motor Control

The TB6612FNG is responsible for driving the DC motor.

The Arduino provides:

- Two digital direction-control signals
- One PWM speed-control signal
- Driver standby control

The physical motor direction must be verified experimentally because the logical meaning of clockwise and counter-clockwise depends on the mechanical installation.

## 5. Position Feedback

Two limit switches provide deterministic mechanical end-position feedback:

- Fully-open position
- Fully-closed position

The firmware configures these inputs with the Arduino's internal pull-up resistors.

## 6. Object Detection

The HC-SR04 is used to detect objects near the door.

The sensor provides:

- `TRIG` — output trigger pulse
- `ECHO` — measured return-pulse duration

The firmware converts the echo duration into an estimated distance.

## 7. User Interface

The 4×4 keypad provides:

- Password entry
- Protected door-access request
- Operating-mode change request
- Confirmation / cancellation

The 16×2 I2C LCD provides system-mode, password, status, and alarm feedback.

## 8. Audible Feedback

The passive buzzer provides:

- Keypress feedback
- Authentication feedback
- Slow-closing warning
- Alarm indication

## 9. Hardware Design Notes

Before physical deployment, verify:

- Motor stall and startup current
- Buck-converter current capability
- Motor-driver thermal performance
- Proper common-ground connections
- Mechanical limit-switch placement
- Door mechanism inertia and stopping distance
- Sensor blind spots
- Emergency-stop behavior

