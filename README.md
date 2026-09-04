# brew-monitor-sensor

Sensor firmware that reads an MPU-6050 accelerometer, computes the tilt, and
reports it — along with temperature and battery level — over Wi-Fi (HTTPS) to a
brew-monitor server.

## Bill of materials
- Micro controller : ESP32-C6 Super Mini
- Accelerometer : MPU-6050
- Batterie charger : TP4056
- Batterie : Panasonic 18650

## Wiring
- SDA of the MPU connect to GPIO19 of the ESP32-C6 Super Mini
- SCL of the MPU connect to GPIO20 of the ESP32-C6 Super Mini
- VCC and GND pins of ESP32-C6 and MPU connect to the batterie charger via a switch
- 2 x 100k reistors in series connected between VCC and GND, with the mid point
  connected to GPIO4 of the ESP32-C6 Super Mini
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

The project targets the ESP32-C6 Super Mini through the `esp32-c6-super-mini`
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

The sketch always includes `include/config.h`. Each physical sensor has its own
config file (`config_jp1.h`, `config_jp2.h`, `config_tr1.h`) with its WiFi
credentials, server endpoint and sensor ID/secret. To build for a specific
sensor, copy its config over `config.h`:

```sh
cp include/config_jp2.h include/config.h
pio run -t upload
```

The default `config.h` contains placeholders — edit it directly (or use one of
the per-sensor files) before flashing.
