# m5stack-glance-clock

Bringing my Glance Clock back to life using an ESP32 (M5Stack) as a bridge.

---

## Overview
This repository contains code and binaries to revive a non-functional "Glance Clock" by using an ESP32 (M5Stack) as a bridge. It smartly resurrects the clock from the outside without modifying the original internal firmware. (<a href="https://glenmoreji.blogspot.com/2026/09/reviving-glance-clock.html" target="_blank">Blog</a>)

<div align="center">
  <img src="./images/Glance.png" width="300">
</div>

---

## Demonstration
Here is a [video](https://youtu.be/5_j0KFj5OKg) demonstrating control via a web browser and iPhone Shortcuts.

---

## Features
- **ESP32 Bridge**       : Acts as a relay using M5Stack [Basic](https://docs.m5stack.com/en/core/basic) or [AtomS3 lite](https://docs.m5stack.com/en/core/AtomS3%20Lite) to send data to the Glance Clock via BLE.
- **Web API**            : Can be operated through HTTP requests from browsers or API clients.
- **Captive Portal Support**: Easily configure SSID and passwords from a smartphone or PC during initial startup or Wi-Fi configuration.
- 
---

## Requirements & Architecture

To get started, here are the minimum requirements and how the system architecture works:

### Hardware & Tools
- **Glance Clock**: The target device to revive.
- **ESP32**: M5Stack Basic or AtomS3 Lite.
- **PC**: Required to flash the firmware/program to the M5Stack.
- 
   <div align="center">
     <img src="./images/M5stack_Basic.png" width="200">
     <img src="./images/AtomS3_lite.png" width="200">
   </div>
   
### System Workflow
```text
[ Control Triggers ]
- Home Assistant (Automations / Sensor integration)
- Raspberry Pi (Periodic / Event transmissions via Node-RED, etc.)
- Smartphone Browser / iOS Shortcuts (Manual / Widget operations)
        ↓ (HTTP Requests)
[ ESP32 Bridge ]
- Wi-Fi connection & Built-in HTTP server
- Accurate time synchronization via NTP (CTS function)
- Translates received messages according to the protocol
        ↓ (BLE Communication)
[ Glance Clock ]
- Text scrolling display / Notifications / Animations
```

---

## Repository Structure
```text
m5stack-glance-clock/
├── src                 
│   └── GlanceBridge.ino    # Source code
├── tools 
│   └── GlanceCommand.html  # GUI tool
└── build/                  # Prebuilt binaries for Web Flusher, etc.
    ├── bootloader_core.bin # For Basic Core
    ├── partitions_core.bin # For Basic Core
    ├── firmware_core.bin   # For Basic Core
    ├── bootloader_s3.bin   # For AtomS3 lite
    ├── partitions_s3.bin   # For AtomS3 lite
    └── firmware_s3.bin     # For AtomS3 lite
```
> **Note for Arduino IDE Users:**  
> When compiling using the Arduino IDE, please move all files inside the `src/` folder directly under a new folder named `GlanceBridge` (`GlanceBridge/GlanceBridge.ino`) before opening the project.

---

## Installation (Flashing via Web Flusher)
You can directly flash the firmware via a browser-based Web Flusher without setting up a development environment (like Arduino IDE).

1. Connect the M5Stack to your PC via a USB cable.
2. Open a Web Flusher page such as [esptool-js](https://espressif.github.io/esptool-js/) in a compatible browser (like Chrome or Edge).
3. Specify the three files located in the `build/` folder to their respective correct addresses/slots for flashing:
   - `bootloader.bin` (Address: `0x1000`)
   - `partitions.bin` (Address: `0x8000`)
   - `firmware.bin` (Address: `0x10000`)
 
---

## Usage & Captive Portal
1. Restart the M5Stack after flashing the firmware.
2. Upon initial startup, since there are no Wi-Fi settings configured, the M5Stack operates in Access Point (AP) mode.
3. Connect to the Wi-Fi network broadcasted by the M5Stack (`M5Stack-Setup`) from your smartphone or PC's Wi-Fi settings screen.
4. The Captive Portal configuration screen will automatically pop up (or appear when you open a browser).
5. Enter your home Wi-Fi SSID and password and save them; the M5Stack will then connect to your home network.  
   (Input via serial monitor from a PC is also available.)
6. Once the PIN code appears on the clock's display, enter `http://glance-clock.local/pin?code=[PIN-code]` to start operating as a bridge.
7. Once **"Done"** is displayed on the Glance Clock, the pairing is successfully completed.  
   *(Tip: Pressing **BtnA** on the Basic or the physical button on the AtomS3 Lite allows you to clear Wi-Fi settings, pairing data, and information.)*
8. Open **`tools/GlanceCommand.html`** in your browser to easily control the clock.
   
   <div align="center">
     <img src="./images/GUI_html.png" width="200">
   </div>

---

## Known Issues / Limitations
- **Calendar Integration Feature**:  
    Currently, the calendar integration feature (`/calendar`) is not correctly documented due to a lack of parsed information.  
    Please use with caution.
- **Timezone & NTP**:     
  The timezone is hardcoded to Japan Standard Time (JST+9) at `9 * 3600` seconds using the NICT NTP server (`ntp.nict.jp`).
  If you use the device outside of Japan or in a different timezone, please update `gmtOffset_sec` as well as the NTP server.

---

## Acknowledgements
This project would not have been possible without the invaluable prior research and open-source contributions by predecessors in the community. I am deeply grateful to:

- **Hypfer** for [Hypfer/glance-clock](https://github.com/Hypfer/glance-clock) (BLE protocol reverse-engineered)
- **frannraf** for [frannraf/glance-clock-control](https://github.com/frannraf/glance-clock-control)

If they had not shared their information and code with the public, I would not have been able to achieve any of this. Thank you so much for paving the way!

---

## Disclaimer
This project is an unofficial, independent third-party implementation based on reverse-engineered protocols. It is not affiliated with, authorized, maintained, or sponsored by the original hardware manufacturer. 

Use of this firmware, bridge software, and web tools is entirely at your own risk. The author assumes no responsibility or liability for any damage to your hardware, data loss, or any other issues arising from the use of this project.

---

## License
This software is released under the [MIT License](LICENSE).
