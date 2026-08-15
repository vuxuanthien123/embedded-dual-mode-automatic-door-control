# Control Flow

## High-Level Flow

![System Flowchart](images/system-flowchart.png)

The firmware starts in Auto Mode with the door assumed to be fully closed.

## Startup

```text
START
  │
  ▼
Initialize serial communication
  │
  ▼
Configure GPIO
  │
  ▼
Initialize LCD
  │
  ▼
Display boot sequence
  │
  ▼
Enter main control loop
```

## Mode Dispatch

```text
                ┌──────────────┐
                │  Main Loop   │
                └──────┬───────┘
                       │
                Read keypad
                       │
             ┌─────────┴─────────┐
             ▼                   ▼
       SECURITY_MODE          AUTO_MODE
             │                   │
             ▼                   ▼
 processSecurityMode()   processAutoMode()
```

## Auto Mode

The Auto Mode state machine is centered around door behavior.

### Closed

- Monitor the ultrasonic sensor.
- If an object is detected, begin opening.
- Permit a keypad request for authenticated mode switching.

### Opening

- Continue motor operation.
- Stop when the open limit switch is reached.

### Opened

- Continue monitoring the object-detection zone.
- Reset the open timer when an object is detected.
- Begin normal closing after the configured absence interval.

### Closing

- Continue normal closing.
- If an obstacle is detected, perform an emergency stop and re-open.

## Security Mode

Security Mode provides password-controlled operations.

```text
Security Mode
      │
      ▼
Wait for keypad command
      │
      ├── B ──► Door-access authentication
      │
      └── C ──► Mode-switch authentication
```

After password verification:

- Valid authentication grants the requested operation.
- Invalid authentication increments the attempt counter.
- Repeated failures activate the alarm sequence.

## Slow Closing

Security Mode includes a slow-closing behavior after the configured waiting condition.

The warning buzzer follows a repeating sound/silence pattern while the door closes at reduced speed.

## Mode Switching

A valid mode-switch request leads to a confirmation stage:

```text
Password valid
      │
      ▼
Request confirmation
      │
   ┌──┴──┐
   ▼     ▼
  `*`   `#`
   │     │
   ▼     ▼
Switch  Cancel
mode    request
```

