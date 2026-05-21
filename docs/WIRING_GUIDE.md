# Wiring Guide

## GPIO Pin Assignment Summary

| GPIO | Component | Signal | Notes |
|------|-----------|--------|-------|
| 4 | HX711 | DOUT | Data out |
| 5 | HX711 | SCK | Clock |
| 12 | HC-SR04 | TRIG | Trigger |
| 14 | HC-SR04 | ECHO | Echo (3.3V safe) |
| 34 | Water Level Sensor | Signal | ADC input only |
| 35 | LDR (Hopper) | Signal | ADC input only |
| 2 | LED (Hopper) | Control | Built-in or external |
| 27 | Float Sensor | Signal | Digital input, PULL_UP |
| 13 | Servo Motor | PWM | 50Hz PWM signal |
| 26 | Relay Module | IN | Active LOW |

> GPIO 34 and 35 are INPUT ONLY on ESP32 — do not use as outputs.

---

## 1. HX711 Load Cell Amplifier

### Purpose
Measures the weight of food in the bowl (grams).

### Wiring

```
Load Cell Wires ──▶ HX711 Wheatstone Bridge:
  Red    ──▶ E+  (Excitation +)
  Black  ──▶ E-  (Excitation -)
  White  ──▶ A-  (Signal -)
  Green  ──▶ A+  (Signal +)

HX711 ──▶ ESP32:
  VCC    ──▶  3.3V
  GND    ──▶  GND
  DT     ──▶  GPIO 4
  SCK    ──▶  GPIO 5
```

### Notes
- Secure load cell to a rigid platform under the food bowl.
- Tare (zero) the scale with the empty bowl on startup.
- Average 10–20 readings for stability.
- Keep load cell wires away from servo/pump wiring to minimize noise.

---

## 2. Water Level Sensor (Analog)

### Purpose
Detects water level percentage in the food/water bowl.

### Wiring

```
Water Level Sensor ──▶ ESP32:
  VCC    ──▶  3.3V (or 5V — check sensor datasheet)
  GND    ──▶  GND
  Signal ──▶  GPIO 34 (ADC)
```

### Notes
- Use a voltage divider if sensor outputs 5V signal to protect ESP32 ADC.
- Calibrate by reading values when empty (0%) and when full (100%).
- Typical output: 0–3.3V maps to 0–100% level.

---

## 3. HC-SR04 Ultrasonic Sensor (Pet Detection)

### Purpose
Detects pet presence within 20–50 cm of the feeding station.

### Wiring

```
HC-SR04 ──▶ ESP32:
  VCC    ──▶  5V (use ESP32 VIN pin)
  GND    ──▶  GND
  TRIG   ──▶  GPIO 12
  ECHO   ──▶  GPIO 14 (via voltage divider: 1kΩ + 2kΩ)
```

### Voltage Divider for ECHO pin (5V → 3.3V)

```
ECHO ──▶ 1kΩ ──▶ GPIO 14
               |
             2kΩ
               |
             GND
```

### Notes
- HC-SR04 outputs 5V on ECHO — must voltage-divide to 3.3V for ESP32 safety.
- Alternatively, use HC-SR04P variant which supports 3.3V operation.
- Set detection threshold: distance < 30 cm = pet detected.

---

## 4. LED + LDR (Hopper Food Level Detector)

### Purpose
Detects whether food remains inside the hopper using a light barrier.

### Concept
```
[LED] ─────────── hopper ──────────── [LDR]
        light crosses hopper cross-section
  
  Food present → light blocked → LDR reads HIGH resistance
  Hopper empty → light reaches LDR → LDR reads LOW resistance
```

### Wiring

**LED:**
```
ESP32 GPIO 2 ──▶ 220Ω resistor ──▶ LED Anode (+)
LED Cathode (-) ──▶ GND
```

**LDR (Photoresistor):**
```
3.3V ──▶ 10kΩ ──▶ GPIO 35 ──▶ LDR ──▶ GND
               (voltage divider midpoint = GPIO 35)
```

### Logic
```
High ADC reading (LDR dark) → Food present → hopper OK
Low ADC reading  (LDR lit)  → No food blocking → hopper LOW/EMPTY
```

### Notes
- Mount LED and LDR directly across from each other on opposite walls of hopper.
- Use black electrical tape to block ambient light from the sides.
- Take 5 readings and average them to filter noise.
- Turn LED ON before reading, turn OFF after to save power.

---

## 5. DIY Float Sensor (Water Reservoir)

### Purpose
Detects whether the water reservoir still contains water.

### Best DIY Implementation: Magnetic Reed Switch + Floating Magnet

This is the most reliable DIY approach — no electrical contacts touch water.

### Materials
- 1x Magnetic Reed Switch (normally open)
- 1x Small strong magnet (neodymium disk, 10mm)
- 1x Foam or bottle cap (buoyant platform for magnet)
- 1x Small plastic tube or guide rod
- Thin wire

### Mechanical Design

```
Reservoir (side view):
┌─────────────────────────────┐
│                             │
│  │ Guide rod                │
│  │                          │
│  │ ┌──────────────────┐     │  ← HIGH water = magnet UP
│  │ │ Floating magnet  │     │
│  │ └──────────────────┘     │
│  │                          │
│  │                          │
│  │    [REED SWITCH]         │  ← Mount reed switch at LOW level mark
│  │                          │
│  ╰──────────────────────────┤
└─────────────────────────────┘
  
When water is HIGH: magnet floats up → reed switch OPEN (no contact)
When water is LOW:  magnet drops down → magnet near reed → switch CLOSES
```

### Wiring

```
Reed Switch ──▶ ESP32:
  One terminal ──▶ GPIO 27
  Other terminal ──▶ GND

ESP32 GPIO 27 configured as INPUT_PULLUP
```

### Logic
```
GPIO 27 = HIGH (switch open, magnet away) → Reservoir HAS WATER
GPIO 27 = LOW  (switch closed, magnet near) → Reservoir LOW/EMPTY
```

### Alternative: Conductive Contact Sensor

```
Two bare wire probes at LOW-LEVEL mark:
  Wire A ──▶ GPIO 27 (INPUT_PULLUP)
  Wire B ──▶ GND

Water bridges the probes:
  Probes wet  → GPIO LOW  → water present
  Probes dry  → GPIO HIGH → reservoir empty
```

> Recommended: Reed switch method — more hygienic and reliable long-term.

---

## 6. Servo Motor (Food Dispensing Flap)

### Purpose
Opens and closes the food dispensing flap.

### Wiring

```
Servo ──▶ External 5V Supply + ESP32:
  Red   (VCC)    ──▶  External 5V+
  Brown (GND)    ──▶  External GND (shared with ESP32 GND)
  Orange (Signal) ──▶  GPIO 13
```

### IMPORTANT
- NEVER power servo from ESP32 3.3V or 5V pins — servo draws too much current.
- Use a dedicated 5V 1A (minimum) power supply or buck converter.
- Share GND between external supply and ESP32.

### Angles
```
CLOSED position: 0°
OPEN position:   90° (adjust based on your flap design)
SHAKE motion:    oscillate 80°–100° briefly to prevent jamming
```

---

## 7. 6V Submersible Pump + Relay Module

### Purpose
Pump refills the water bowl. Relay safely switches the 6V pump circuit.

### Relay Wiring

```
Relay Module Connections:

Control Side (low voltage):
  VCC  ──▶  5V (ESP32 VIN or external 5V)
  GND  ──▶  GND (shared)
  IN   ──▶  GPIO 26 (active LOW — LOW turns relay ON)

Power Side (high voltage / pump circuit):
  COM  ──▶  6V Supply (+)
  NO   ──▶  Pump VCC (red wire)
  Pump GND (black wire) ──▶  6V Supply (-)
```

### Diagram

```
                    ┌────────────────────────┐
ESP32               │       RELAY MODULE     │
GPIO 26 ──▶ IN      │                        │
5V     ──▶ VCC      │  COM ──── 6V (+)       │──── 6V Power Supply (+)
GND    ──▶ GND      │  NO  ──── Pump (+)     │──── Pump Red Wire
                    │          Pump (-)      │──── 6V Power Supply (-)
                    └────────────────────────┘
                              Pump Black Wire ──── 6V (-)
```

### Notes
- Most relay modules are ACTIVE LOW: GPIO LOW = relay ON = pump runs.
- Add a flyback diode (1N4007) across pump terminals if relay doesn't have one.
- Do NOT run pump longer than 30 seconds without checking water level.
- Implement dry-run protection: never run pump if reservoir is empty.

---

## 8. Power Supply Summary

```
┌─────────────────────────────────────────────────────┐
│                 POWER DISTRIBUTION                   │
│                                                      │
│  USB 5V/2A ──▶ ESP32 USB port (for programming)     │
│                                                      │
│  5V/2A Wall Adapter ──▶ ESP32 VIN ──▶ ESP32 logic   │
│                      └──▶ HC-SR04 VCC                │
│                      └──▶ Relay VCC                  │
│                      └──▶ Servo VCC (if < 500mA)     │
│                                                      │
│  Dedicated 5V/2A ──▶ Servo (recommended separate)   │
│                                                      │
│  6V/1A Adapter ──▶ Relay NO/COM ──▶ Water Pump      │
│                                                      │
│  3.3V from ESP32 ──▶ HX711, LDR, Float Sensor       │
└─────────────────────────────────────────────────────┘
```

---

## 9. Common Grounding Rule

All GNDs MUST be connected together:
- ESP32 GND
- External 5V supply GND
- External 6V supply GND

Floating grounds cause erratic sensor readings and relay misfires.

---

## 10. Assembly Checklist

- [ ] Load cell mounted rigidly under bowl platform
- [ ] HX711 wired to load cell and ESP32
- [ ] Water level sensor submerged at correct depth in bowl
- [ ] HC-SR04 mounted at pet head height, facing feeding area
- [ ] LED and LDR positioned across hopper (light barrier)
- [ ] Float sensor mounted in reservoir at low-water mark
- [ ] Servo mounted to food flap mechanism, moves freely
- [ ] Relay module wired correctly (verify NO vs NC)
- [ ] Pump submerged in water reservoir, output tube routed to bowl
- [ ] All GNDs tied together
- [ ] External power supplies tested before connecting to components
- [ ] GPIO 34 and 35 only used as inputs (hardware limitation)
