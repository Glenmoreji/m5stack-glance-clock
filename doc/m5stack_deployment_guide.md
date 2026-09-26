# M5Stack Target Configuration & Deployment Guide

This document summarizes the required Arduino IDE tool menu configurations and `esptool` memory address offsets for deploying binaries to M5Stack Core Basic (ESP32) and AtomS3 / AtomS3 Lite (ESP32-S3) devices.

---

## 1. Arduino IDE Tool Menu Settings

| Setting Option (Tools Menu) | M5Stack Core Basic<br>(ESP32) | AtomS3 / AtomS3 Lite<br>(ESP32-S3) | Notes & Details |
| :--- | :--- | :--- | :--- |
| **Board** | `M5Stack-Core-ESP32` | `M5AtomS3` | Target microcontroller platform. |
| **Flash Mode** | `DIO 80MHz` | `DIO 80MHz` | Must use `DIO` mode for stable booting on both devices. |
| **Flash Size** | `4MB (32Mb)` | `8MB (64Mb)` | Onboard SPI flash memory capacity. |
| **Partition Scheme** | `Default 4MB with spiffs` | `Huge APP (3MB No OTA)` / `8M with spiffs` | Flash memory allocation profile. |
| **USB CDC On Boot** | N/A | `Enabled` | Enables native USB CDC serial output on ESP32-S3. |
| **USB Mode** | N/A | `Hardware CDC and JTAG` | ESP32-S3 internal USB hardware interface mode. |
| **Upload Mode** | `UART0` | `UART0 / Hardware CDC` | Flashing interface mode. |
| **CPU Frequency** | `240MHz (WiFi)` | `240MHz (WiFi)` | Standard operational clock frequency. |

> **Important Notes:**
> - When switching target boards in Arduino IDE, always re-verify the **Flash Mode** and **Flash Size** settings, as they do not always reset automatically to hardware-specific defaults.
> - For standalone operation on ESP32-S3, ensure `Serial.setTxTimeoutMs(0)` is included in `setup()` or USB CDC output is checked with `if (Serial)` to prevent blocking when no USB serial host is attached.

---

## 2. esptool Flash Address Offset Table

| Binary File | M5Stack Core Basic<br>(ESP32) | AtomS3 / AtomS3 Lite<br>(ESP32-S3) | Purpose |
| :--- | :--- | :--- | :--- |
| **`bootloader.bin`** | **`0x1000`** | **`0x0`** | First-stage bootloader binary. |
| **`partitions.bin`** | **`0x8000`** | **`0x8000`** | Flash partition layout table. |
| **`boot_app0.bin`** | **`0xe000`** | **`0xe000`** | Boot image selection data. |
| **`firmware.bin`** (`*.ino.bin`) | **`0x10000`** | **`0x10000`** | Main application executable binary. |

---

## 3. Flash Commands Reference

### M5Stack Core Basic (ESP32)

```bash
# Erase full flash memory
esptool.py --chip esp32 --port COM3 erase_flash

# Flash exported binaries
esptool.py --chip esp32 --port COM3 write_flash \
  0x1000  bootloader.bin \
  0x8000  partitions.bin \
  0xe000  boot_app0.bin \
  0x10000 firmware.bin
```

### AtomS3 / AtomS3 Lite (ESP32-S3)

```bash
# Erase full flash memory
esptool.py --chip esp32s3 --port COM3 erase_flash

# Flash exported binaries
esptool.py --chip esp32s3 --port COM3 write_flash \
  0x0     bootloader.bin \
  0x8000  partitions.bin \
  0xe000  boot_app0.bin \
  0x10000 firmware.bin
```