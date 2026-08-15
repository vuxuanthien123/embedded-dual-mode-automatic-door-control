# Software Architecture

## 1. Architectural Approach

The firmware uses a state-oriented control architecture.

Instead of representing the complete system with a single state variable, independent dimensions of behavior are modeled explicitly.

```text
SYSTEM_MODE
    │
    ├── AUTO_MODE
    └── SECURITY_MODE
          │
          ▼
    Keypad / Authentication
          │
          ▼
    Door Behavioral State
          │
          ├── Opening
          ├── Opened
          ├── Closing
          ├── Slow Closing
          ├── Closed
          └── Emergency Stopped
```

## 2. State Abstractions

### SYSTEM_MODE

Controls the high-level system behavior.

- `AUTO_MODE`
- `SECURITY_MODE`

### KEYPAD_ACTION

Controls the current keypad interaction.

- `KEYPRESS_AWAITING`
- `PASSWORD_ENTERING`
- `SWITCH_CONFIRMATION`

### ENTERING_PURPOSE

Specifies why password authentication is being performed.

- `OPEN_DOOR`
- `SWITCH_SYSTEM_MODE`
- `NONE`

### DOOR_BEHAVIOR

Represents the physical/behavioral state of the door.

- `OPENED_COMPLETELY`
- `OPENING_NORMALLY`
- `CLOSING_NORMALLY`
- `CLOSING_SLOWLY`
- `CLOSED_COMPLETELY`
- `EMERGENCY_STOPPED`

### MOTOR_DIRECTION

Represents the command issued to the motor driver.

- `CLOCK_WISE`
- `COUNTER_CLOCK_WISE`
- `BRAKE`

## 3. Main Control Loop

The top-level loop performs three conceptual operations:

1. Update the LCD when the keypad is idle.
2. Poll the keypad.
3. Dispatch control logic according to the current system mode.

```text
loop()
  │
  ├── Display current system mode
  │
  ├── Read keypad
  │
  ├── SECURITY_MODE ──► processSecurityMode()
  │
  └── AUTO_MODE ──────► processAutoMode()
```

## 4. Modular Functions

The firmware separates responsibilities into dedicated functions, including:

- `displaySystemMode()`
- `clearPasswordBuffer()`
- `enterPassword()`
- `requestPassword()`
- `isObjectDetected()`
- `activateAlarm()`
- `verifyPassword()`
- `requestConfirm()`
- `confirmModeSwitch()`
- `setMotorDirection()`
- `emergencyStop()`
- `openDoorNormally()`
- `closeDoorNormally()`
- `closeDoorSlowly()`
- `keyPressFeedback()`
- `processSecurityMode()`
- `processAutoMode()`

This organization keeps low-level hardware actions separated from high-level behavioral logic.

## 5. Timing

The firmware uses `millis()`-based timestamps for important behavioral decisions such as:

- Motor-motion timing
- Door-open timeout
- Warning-buzzer timing

This approach allows the main control loop to continue executing while timed conditions are monitored.

## 6. Authentication

Password entry is handled as a dedicated keypad interaction state.

The workflow is:

```text
Idle
 │
 ├── B/C
 ▼
Password Entry
 │
 ├── Digits ──► Update password buffer
 │
 ├── A ───────► Delete digit
 │
 └── D ───────► Verify password
                    │
              ┌─────┴─────┐
              ▼           ▼
            Valid       Invalid
              │           │
              ▼           ▼
        Grant action   Increment attempts
                          │
                    Repeated failures
                          │
                          ▼
                       Alarm
```

## 7. Safety Response

During normal closing, ultrasonic detection is evaluated.

If an obstacle is detected:

```text
CLOSING_NORMALLY
       │
       ▼
Obstacle detected?
   │          │
  Yes         No
   │           │
   ▼           ▼
Emergency    Continue
Stop         closing
   │
   ▼
Re-open
```

The exact mechanical safety of the final system still depends on the physical mechanism and hardware implementation.

