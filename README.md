# Smart Home Demo Prototype — Mechatronic System Integration

An integrated embedded systems and mechatronics prototype developed at the **Faculty of Engineering, Ain Shams University (Fall 2026)**. The system automates residential access control, security monitoring, adaptive lighting, and closed-loop climate regulation using an **Arduino Uno (ATmega328P)** executing a cooperative, non-blocking control architecture.

---

## Key Subsystems & Features

1. **Automated Access Control & Doorbell:**
   * Keypad PIN authentication (`5555#`) actuates an **SG90 micro servo motor** (`0°–90°`) coupled via a custom MDF-and-metal mechanical linkage to open and close the main door.
   * Pressing `*` triggers the active buzzer as a doorbell.
2. **Perimeter Security & Proximity Exterior Lighting:**
   * Security modes (`Armed` via `1#`, `Disarmed` via `0#`) monitor an **MC-38 magnetic reed switch** on the window, triggering an audible alarm upon breach.
   * An **HC-SR04 ultrasonic sensor** detects approach within `≤ 10 cm` to automatically illuminate the outdoor entrance LED.
3. **Adaptive & Manual Indoor Lighting:**
   * Combines an **LDR** voltage divider for automatic low-light activation with a debounced **tactile push button** for manual toggle override.
4. **Closed-Loop PWM Climate Control:**
   * A **DHT11 sensor** monitors ambient indoor temperature against a user-adjustable setpoint (`20°C–40°C`) set via a **10kΩ rotary potentiometer**.
   * Drives a **5V DC cooling fan (40×40 mm)** using PWM speed modulation through an **IRFZ44N N-Channel MOSFET** with **1N4004** flyback diode protection.
5. **Shared I2C Bus Optimization:**
   * Both the **16×2 LCD** (`0x27`) and a repurposed **PCF8574 I2C backpack** (`0x26`, hardware-modified via soldering) share `A4 (SDA)` and `A5 (SCL)` to interface the 4×3 active keypad columns while conserving microcontroller I/O pins.

---

## Hardware & Pin Allocation

| Component | Arduino Pin | Interface / Notes |
| :--- | :---: | :--- |
| **16×2 LCD Display** | `A4 (SDA)`, `A5 (SCL)` | I2C Address `0x27` |
| **Matrix Keypad (4×4, 3 cols used)** | `A4 (SDA)`, `A5 (SCL)` | Repurposed PCF8574 I2C Address `0x26` |
| **MC-38 Window Reed Switch** | `D2` | Digital Input (`INPUT_PULLUP`) |
| **Manual Light Push Button** | `D4` | Digital Input (Debounced toggle) |
| **5V Cooling Fan (via IRFZ44N)** | `D5` | PWM Output + `1N4004` flyback diode |
| **Outdoor Entrance LED** | `D6` | Digital Output (`220Ω` current limiter) |
| **HC-SR04 Ultrasonic Sensor** | `D7` (Trig), `D8` (Echo) | Proximity threshold `≤ 10 cm` |
| **SG90 Door Servo Motor** | `D9` | PWM Control (`0°` closed / `90°` open) |
| **Active Buzzer** | `D10` | Digital Output (Doorbell & breach alarm) |
| **Indoor Ambient LED** | `D11` | Digital Output (`220Ω` current limiter) |
| **DHT11 Temperature Sensor** | `D12` | Single-wire digital data |
| **LDR Light Sensor** | `A0` | Analog Input (`10kΩ` voltage divider) |
| **Setpoint Potentiometer** | `A1` | Analog Input (`20°C–40°C` mapping) |

---

## Firmware Architecture

The firmware avoids blocking `delay()` calls by implementing a cooperative non-blocking scheduler (`backgroundTasks()` / `waitMs()`). This ensures continuous sensor polling, real-time LCD status updates, and immediate keypad responsiveness across all operating states.

---

## Project Team

* **Mohamed Hossam Mohamed** (`2400613`)
* **Hazem Ahmed Hassanien** (`2400356`)
* **Fatma Mahrous Mamdouh** (`2400593`)
* **Jannah Mostafa Hassan** (`2400258`)
