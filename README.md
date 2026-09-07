<div align="center">

# 📚 Smart Library Seat Management System

**Automatic seat occupancy detection & real-time reporting using IoT**

![ESP32](https://img.shields.io/badge/ESP32-Microcontroller-blue?logo=espressif&logoColor=white)
![Firebase](https://img.shields.io/badge/Firebase-RTDB-orange?logo=firebase&logoColor=white)
![Arduino](https://img.shields.io/badge/Arduino-C++-00979D?logo=arduino&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-green)
![Status](https://img.shields.io/badge/Status-Prototype-yellow)

[Architecture](#-system-architecture) · [Hardware](#-hardware-components) · [Setup](#-getting-started) · [API Docs](#-nfc-api-endpoint) · [Future Work](#-future-improvements)

</div>

---

## 🎯 Problem Statement

In university libraries, students often:
- ❌ Waste time searching for an available seat
- ❌ "Reserve" seats by leaving belongings — with no accountability
- ❌ Have no visibility into seat availability across floors

This system solves all three by **detecting occupancy with sensors**, **authenticating users via NFC**, and **publishing seat status in real time** to a cloud dashboard.

---

## 🏗️ System Architecture

```
┌──────────────┐      HTTP POST       ┌──────────────────────────────┐
│  NFC Phone   │ ──────────────────▶  │         ESP32                │
│  App/Reader  │   { tag, token }     │                              │
└──────────────┘                      │  ┌──────────┐  ┌──────────┐ │
                                      │  │PIR Sensor│  │Load Cell │ │
                                      │  │ (Motion) │  │ (Weight) │ │
                                      │  └────┬─────┘  └────┬─────┘ │
                                      │       ▼              ▼       │
                                      │  ┌────────────────────────┐  │
                                      │  │    State Machine       │  │
                                      │  │ EMPTY ↔ OCCUPIED ↔    │  │
                                      │  │        RESERVED        │  │
                                      │  └───────────┬────────────┘  │
                                      │              │               │
                                      │   ┌──────────┴──────────┐   │
                                      │   │  LEDs    │ Firebase  │   │
                                      │   │ Status   │  Update   │   │
                                      │   └──────────┴──────────┘   │
                                      └──────────────────────────────┘
                                                     │ HTTPS
                                                     ▼
                                      ┌──────────────────────────────┐
                                      │   Firebase Realtime Database │
                                      │         /seat/1              │
                                      └──────────────┬───────────────┘
                                                     ▼
                                      ┌──────────────────────────────┐
                                      │    Web / Mobile Dashboard    │
                                      └──────────────────────────────┘
```

---

## 🔧 Hardware Components

| Component | Model/Spec | Purpose |
|:---|:---|:---|
| Microcontroller | **ESP32** | Central processing & WiFi connectivity |
| Motion Sensor | **PIR Sensor** | Detects human presence near the seat |
| Weight Sensor | **HX711 + Load Cell** | Detects belongings on the seat/desk |
| LED (Red) | 5mm LED | Indicates seat is **occupied** |
| LED (Green) | 5mm LED | Indicates seat is **empty** |
| LED (Yellow) | 5mm LED | Indicates seat is **reserved** |
| NFC Tags | NTAG213/215 | User identification via phone app |

---

## 📌 Pin Configuration

| Component | GPIO |
|:---|:---|
| PIR Sensor OUT | `36` |
| HX711 DT (Data) | `35` |
| HX711 SCK (Clock) | `25` |
| LED — Occupied (Red) | `12` |
| LED — Empty (Green) | `32` |
| LED — Reserved (Yellow) | `33` |

<details>
<summary><b>Wiring Diagram (text)</b></summary>

```
ESP32
├── GPIO 36 ◄── PIR Sensor OUT
├── GPIO 35 ◄── HX711 DT
├── GPIO 25 ──▶ HX711 SCK
├── GPIO 12 ──▶ Red LED    ──▶ 220Ω ──▶ GND
├── GPIO 32 ──▶ Green LED  ──▶ 220Ω ──▶ GND
├── GPIO 33 ──▶ Yellow LED ──▶ 220Ω ──▶ GND
├── 3.3V    ──▶ PIR VCC, HX711 VCC
└── GND     ──▶ Common Ground
```

</details>

---

## 🔄 State Machine

```
                NFC Tap-In
    ┌──────┐ ────────────────▶ ┌──────────┐
    │ EMPTY│                   │ OCCUPIED │
    └──────┘ ◀──────────────── └──────────┘
                NFC Tap-Out         │
                    ▲               │ Person leaves
                    │               │ (Object stays)
                    │               ▼
                    │          ┌──────────┐
                    └───────── │ RESERVED │
                   Tap-Out /   └──────────┘
                  Auto-release      │ Person returns
                                    ▼
                               ┌──────────┐
                               │ OCCUPIED │
                               └──────────┘
```

| State | Condition | LED | Firebase `status` |
|:---|:---|:---:|:---|
| **EMPTY** | No user logged in | 🟢 | `"EMPTY"` |
| **OCCUPIED** | User tapped in + person detected | 🔴 | `"OCCUPIED"` |
| **RESERVED** | User away, belongings remain | 🟡 | `"RESERVED"` |

---

## ☁️ Firebase Integration

On every state change, the ESP32 pushes to Firebase Realtime Database:

**Path:** `/seat/1`

```json
{
  "status": "OCCUPIED",
  "user": "user123",
  "timerRunning": false
}
```

> [!NOTE]
> The ESP32 uses email/password authentication with the Firebase SDK. Token refresh and reconnection are handled automatically.

---

## 🌐 NFC API Endpoint

The ESP32 hosts a local HTTP server to receive NFC scan events.

### `POST /scan`

**Request Body:**

```json
{
  "tag": "user123",
  "token": "<secret_token>"
}
```

**Response Codes:**

| Code | Meaning |
|:---|:---|
| `200 OK` | `{"status":"ok"}` — Tap processed |
| `400 Bad Request` | Invalid JSON |
| `401 Unauthorized` | Wrong token |
| `405 Method Not Allowed` | Use POST |

<details>
<summary><b>cURL Example</b></summary>

```bash
curl -X POST http://192.168.1.100/scan \
  -H "Content-Type: application/json" \
  -d '{"tag": "user123", "token": "your-secret-token"}'
```

</details>

---

## 🚀 Getting Started

### Prerequisites

- [Arduino IDE](https://www.arduino.cc/en/software) 2.x+ or [PlatformIO](https://platformio.org/)
- ESP32 Board Support Package
- Firebase project with **Realtime Database** and **Email/Password Auth** enabled

### Step 1 — Install Libraries

| Library | Install via | Purpose |
|:---|:---|:---|
| [`Firebase ESP Client`](https://github.com/mobizt/Firebase-ESP-Client) | Library Manager | Firebase RTDB |
| [`ArduinoJson`](https://github.com/bblanchon/ArduinoJson) | Library Manager | JSON handling |
| [`HX711_ADC`](https://github.com/olkal/HX711_ADC) | Library Manager | Load cell ADC |

### Step 2 — Configure Credentials

Create a `secrets.h` file in the project root:

```cpp
#define FIREBASE_API_KEY      "your-api-key"
#define FIREBASE_DATABASE_URL "https://your-project.firebaseio.com"
#define USER_EMAIL            "your-email@example.com"
#define USER_PASSWORD         "your-password"

#define WIFI_SSID             "your-ssid"
#define WIFI_PASSWORD         "your-wifi-password"

const char* secretToken =     "your-nfc-secret";
```

> [!WARNING]
> **Never commit `secrets.h` to version control.** Add it to your `.gitignore` — see the [recommended `.gitignore`](#recommended-gitignore) below.

### Step 3 — Wire Hardware

Connect components per the [Pin Configuration](#-pin-configuration) table.

### Step 4 — Calibrate Load Cell

1. Upload with default `CALIBRATION_FACTOR = 645.8`
2. Place a known weight on the load cell
3. Adjust `CALIBRATION_FACTOR` until serial output matches actual weight
4. Set `OBJECT_THRESHOLD_GRAMS` (default: `100g`)

> [!TIP]
> Start calibration with an empty load cell. Use a water bottle (~500g) as a reference weight.

### Step 5 — Upload & Run

1. Board: **ESP32 Dev Module**
2. Upload the sketch
3. Open Serial Monitor at **115200 baud**
4. Expected output:

```
✅ Connected: 192.168.x.x
✅ Firebase Ready!
✅ Setup Complete!
```

---

## 📊 Serial Monitor

Status is printed every 10 seconds:

```
--- Status ---
State: OCCUPIED
User: user123
Person: YES
Object: YES
Firebase: Ready
WiFi: OK
Weight: 342.5g
--------------
```

---

## 🛠️ Tech Stack

| Category | Technology |
|:---|:---|
| MCU | ESP32 (Xtensa LX6 / FreeRTOS) |
| Language | C++ (Arduino Framework) |
| Sensors | PIR, HX711 + Load Cell |
| Connectivity | WiFi 802.11 b/g/n |
| Protocols | HTTP (local), HTTPS (Firebase) |
| Cloud | Firebase RTDB + Firebase Auth |
| Data Format | JSON via ArduinoJson |

---

## 📁 Project Structure

```
smart-library-seat/
├── smart_library_seat.ino   # Main firmware
├── secrets.h                # Credentials (⚠️ gitignored)
├── secrets.h.example        # Credentials template
├── .gitignore
├── LICENSE
└── README.md
```

### Recommended `.gitignore`

```gitignore
# Credentials
secrets.h

# Build
build/
.pio/

# IDE
.vscode/
*.code-workspace
```

---

## 🔮 Future Improvements

- [ ] **HTTPS on ESP32** — TLS for NFC API
- [ ] **MQTT broker** — Replace Firebase direct writes for scale
- [ ] **OTA firmware updates** — Remote management
- [ ] **Auto-release timer** — Free RESERVED seats after 15 min
- [ ] **Multi-seat support** — Unique seat IDs and central coordination
- [ ] **Web dashboard** — Real-time seat map across floors
- [ ] **Analytics** — Peak hours, session duration, utilization
- [ ] **mDNS discovery** — `seat1.local` instead of IP addresses

---

## 🤝 Contributing

Contributions are welcome! To get started:

1. **Fork** this repository
2. **Create** a feature branch: `git checkout -b feature/auto-release`
3. **Commit** your changes: `git commit -m "Add auto-release timer"`
4. **Push** to the branch: `git push origin feature/auto-release`
5. Open a **Pull Request**

> [!NOTE]
> Please open an issue first to discuss significant changes.

---

## 👤 Author

**Kumar Manan**
- Designed, developed & tested as a **Tinkering Lab** project

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).

```
MIT License — feel free to use and modify for educational purposes.
```
