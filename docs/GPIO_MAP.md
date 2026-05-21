# GPIO Pin Map

## ESP32 Pin Assignment

```
┌──────────────────────────────────────────────────────────────┐
│                    ESP32 DEV BOARD                            │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │   EN        GPIO 23   GPIO 22   GPIO 1(TX)  GPIO 3(RX)  │ │
│  │   GPIO 36   GPIO 21   GND       GPIO 19     GPIO 18     │ │
│  │   GPIO 39   GPIO 5*   GPIO 17   GPIO 16     GPIO 4*     │ │
│  │   GPIO 34*  GPIO 18   GPIO 5*   GPIO 17     GPIO 2*     │ │
│  │   GPIO 35*  GPIO 19   GPIO 21   GPIO 22     GPIO 15     │ │
│  │   GPIO 32   GPIO 21   SDA/SCL   GPIO 23     GPIO 8      │ │
│  │   GPIO 33   GPIO 22   GPIO 14*  GPIO 12*    GPIO 7      │ │
│  │   GPIO 25   GPIO 23   GPIO 27*  GPIO 26*    GPIO 6      │ │
│  │   GPIO 26*  GND       GPIO 25   GPIO 33     GPIO 11     │ │
│  │   GPIO 27*  GPIO 3RX  GPIO 32   GPIO 34*    GPIO 10     │ │
│  │   GPIO 14*  GPIO 1TX  GND       GPIO 35*    GPIO 9      │ │
│  │   GPIO 12*  GND       3V3       GPIO 39     GND         │ │
│  │   GND       5V        GND       GPIO 36     GPIO 13*    │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  (* = Used in this project)                                   │
└──────────────────────────────────────────────────────────────┘
```

## Pin Assignments

| GPIO | Direction | Component | Signal | Voltage |
|------|-----------|-----------|--------|---------|
| 4 | INPUT | HX711 | DT (Data) | 3.3V |
| 5 | OUTPUT | HX711 | SCK (Clock) | 3.3V |
| 12 | OUTPUT | HC-SR04 | TRIG | 3.3V |
| 13 | OUTPUT | Servo Motor | PWM Signal | 3.3V |
| 14 | INPUT | HC-SR04 | ECHO | 3.3V (via divider) |
| 2 | OUTPUT | Hopper LED | ON/OFF | 3.3V |
| 26 | OUTPUT | Relay Module | IN (active LOW) | 3.3V |
| 27 | INPUT | Float Sensor | Digital | 3.3V (PULL_UP) |
| 34 | INPUT | Water Level | Analog | 0–3.3V |
| 35 | INPUT | LDR Hopper | Analog | 0–3.3V |

## Voltage Reference

| Pin | ADC Range | Notes |
|-----|-----------|-------|
| 34 | 0–4095 → 0–100% | Water sensor |
| 35 | 0–4095 → dark/bright | LDR light level |

## Reserved / Do Not Use

| GPIO | Reason |
|------|--------|
| 0 | Boot mode selection — do not connect |
| 1 | UART TX — avoid if using Serial |
| 3 | UART RX — avoid if using Serial |
| 6–11 | Flash memory — NEVER use |
| 12 | Boot strapping — sensitive during boot |

## Signal Levels

All ESP32 GPIOs operate at 3.3V logic. Do NOT apply 5V directly to any GPIO pin.

Safe 5V interfaces:
- HC-SR04 VCC: 5V (from VIN) — but ECHO pin must be divided to 3.3V
- Relay VCC: 5V — relay IN pin is 3.3V compatible on most modules
- Servo VCC: 5V external — signal pin is 3.3V compatible

## config.h Pin Definitions (matches firmware)

```cpp
// HX711 Load Cell
#define PIN_HX711_DT    4
#define PIN_HX711_SCK   5

// Ultrasonic (HC-SR04)
#define PIN_ULTRASONIC_TRIG  12
#define PIN_ULTRASONIC_ECHO  14

// Servo Motor
#define PIN_SERVO       13

// Water Level Sensor (analog)
#define PIN_WATER_LEVEL 34

// Hopper Sensor (LDR + LED)
#define PIN_LDR         35
#define PIN_HOPPER_LED  2

// Float Sensor (digital)
#define PIN_FLOAT       27

// Relay (water pump)
#define PIN_RELAY       26
```
