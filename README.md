# brew-monitor-sensor

Sensor firmware that reads an MPU-6050 accelerometer, computes the tilt, and
reports it — along with temperature and battery level — over Wi-Fi (HTTPS) to a
brew-monitor server.

## Bill of materials
- Micro controller : Beetle ESP32-C6 (DFR1117)
- Accelerometer : MPU-6050
- Batterie charger : TP4056
- Batterie : Panasonic 18650

## Wiring
- SDA of the MPU connect to GPIO19 of the ESP32-C6
- SCL of the MPU connect to GPIO20 of the ESP32-C6
- VIN and GND pins of ESP32-C6 connect to the batterie charger via a switch
- VCC and GND pings of the MPU connect to the 3V3 and GND pins of the ESP32-C6 respectively
- 2 x 100k reistors in series connected between VIN and GND, with the mid point
  connected to GPIO4 of the ESP32-C6
- TP4056 directly connects to the batterie

## Dependencies
- [PlatformIO Core](https://docs.platformio.org/page/core.html)
- PlatformIO automatically downloads the `pioarduino` espressif32 platform fork
  (Arduino core 3.x, required for reliable ESP32-C6 support) on first build.

## Install PlatformIO

```sh
# Requires Python 3
pip install -U platformio
```

Or use the [PlatformIO IDE extension for VS Code](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide).

## Build & upload

The project targets the Beetle ESP32-C6 through the `esp32-c6-super-mini`
environment defined in `platformio.ini`.

```sh
# Build (compiles src/hardware.ino)
pio run

# Upload to the board over the native USB-C port
pio run -t upload

# Open the serial monitor (baud rate is set via `monitor_speed`)
pio device monitor

# List connected devices
pio device list
```

If the USB port isn't auto-detected, set `upload_port` in `platformio.ini`.

## Project layout

```
platformio.ini      # build/upload configuration
src/hardware.ino    # firmware sketch
include/config.h    # ACTIVE configuration (included by the sketch)
```

## Configuring the sensor

Wi-Fi credentials, the server endpoint, and the sensor ID/secret are **no longer
compiled into the firmware**. Instead, the sketch ships with a WiFiManager-based
config portal and stores all settings in the ESP32 NVS flash (`Preferences`,
namespace `brewmon`).

### First boot (factory / after `pio run -t erase`)

1. Power the sensor. Because there is no saved config, the device boots straight
   into the config portal AP.
2. On your phone/laptop, connect to the Wi-Fi network named
   `BrewMonitor-<id>` (no password).
3. Open `http://192.168.4.1` (usually it redirects automatically via the
   captive portal).
4. Fill in:
   - **Wi-Fi credentials** (SSID + password of your WLAN, selected from the scan
     or typed)
   - **Server hostname** (e.g. `brews.example.com`)
   - **Server port** (e.g. `443`)
   - **Sensor ID** and **Sensor secret** (as issued for this sensor)
5. Click **save**. The portal tries to connect to the Wi-Fi; on success the
   settings are persisted in NVS and the device restarts into the normal
   measure → report → deep-sleep cycle.

### Re-configuring a configured sensor

Every time the device boots/wakes, the onboard LED turns **on (solid)** for a
short window (`BOOT_PRESS_WINDOW_MS`, default 3 s). Press the **BOOT button**
(GPIO9) during that window:

1. The LED blinks a few times to acknowledge the press.
2. The config portal opens (LED keeps flashing while open).
3. The settings are pre-filled; change what you need and click **Save**.
4. The device restarts with the new config.

If the portal is closed without saving (exit or the
`CONFIG_PORTAL_TIMEOUT_S` timeout), the device continues with the existing
config. On the very first boot (nothing saved yet), closing without saving
sends it to deep sleep; it will reopen the portal on the next boot.

### Reset to factory

```sh
# Erase NVS (and the whole flash), then re-flash the firmware
pio run -t erase
pio run -t upload
```

### Tweaking

All of the above timings/pins live at the top of `include/config.h`
(`STATUS_LED_PIN`, `LED_ACTIVE_HIGH`, `BOOT_PRESS_WINDOW_MS`,
`CONFIG_PORTAL_TIMEOUT_S`, `CONFIG_AP_SSID_PREFIX`). 
