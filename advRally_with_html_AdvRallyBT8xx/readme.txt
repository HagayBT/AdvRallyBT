Adventure Rally Bluetooth Controller — User Manual

Overview
Details on the project web site:
https://hagayb.wixsite.com/advrallybt

Todo first:
make changes to first lines on main.cpp before uploading

/******************************************************/
char UNITNAME[] = "AdvRallyBT8xx";
const char* wifiPassword = "12345678";
const bool isAdventure = true; // Enduro is 5 functions (false), Adventure is 8 functions (true)
/******************************************************/

  1. Set the unit name "AdvRallyBT"
    - convention for 5 functions: "AdvRallyBT501", number from 500 and up
    - convention for 8 functions: "AdvRallyBT801", number from 800 and up
  2. Set Wi-Fi Password
  3. Update isAdventure according to the number of functions
    - 5 functions give us the option to monitor, and the configuration site shows only 5 options
    - also, when 5 functions are used, the loop is faster
  4. Ensure the pinout matches the code

Developer API (served by the device)
- GET /ping → {"ping":true}
- GET /unit → {"unitname":"AdvRallyBT8xx"}
- GET /getActiveMode → {"keyMode":1|2|3}
- POST /setActiveMode?keyMode=N → {"ok":true,"mode":N}
- GET /getMode?idx=N → {"blob":"19 comma‑separated integers"}
- POST /setMode?idx=N&blob=… → {"ok":true}
- GET /getLed → {"brightness":0..100}
- POST /setLed?brightness=0..100 → {"ok":true,"brightness":N}

Build and Upload (PlatformIO)
- Prerequisites: VS Code + PlatformIO; ESP32 connected via USB.
- Upload firmware: pio run --target upload
- Upload web UI (SPIFFS): pio run --target uploadfs
- Serial monitor (optional): pio device monitor

Pinout (ESP32)
- GPIO26: UP
- GPIO18: DOWN
- GPIO22: LEFT
- GPIO21: RIGHT
- GPIO19: PUSH
- GPIO03: TOGGLE DOWN
- GPIO01: TOGGLE UP
- GPIO00: BUTTON
- GPIO39: UNPAIRE (input‑only; ext. pull‑up; active‑low)
- GPIO25: Main LED (PWM)
- GPIO27: SK6812 DIN (RGB)

Ownership / Legal
- Proprietary: Hagay Ben‑Tovim. All rights reserved.
- Ride responsibly. Obey local laws. Test controls safely before riding.
