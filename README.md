# ESP32 Filament Scale

Wi-Fi-enabled filament spool scale based on an ESP32 and HX711 load-cell amplifier. Provides live weight readings and remote tare over HTTP for the [Panda Touch Filament Manager](https://github.com/Rushmere3D/PandaTouch-Filament-Manager) or other clients on the same local network.

> **Status:** Working prototype tested with Panda Touch Filament Manager. The HTTP API currently supports weight readings and tare; remote calibration is **not yet implemented**.

## Features

- HX711 load-cell readings, displayed in grams
- Calibration through the USB serial monitor using a known mass
- Calibration factor stored in ESP32 non-volatile storage (`Preferences`)
- Automatic tare at startup (platform must be empty)
- Wi-Fi connection with retry handling
- HTTP weight endpoint and remote tare endpoint
- Serial diagnostic commands

## Hardware

- ESP32 development board (tested configuration: `esp32doit-devkit-v1`)
- HX711 amplifier board
- Suitable bar load cell and mechanical spool platform
- USB cable and suitable power supply

### Connections

| HX711 | ESP32 | Purpose |
|---|---|---|
| DOUT / DT | GPIO16 | Data output |
| SCK / CLK | GPIO17 | Clock |
| GND | GND | Common ground |
| VCC | 3.3V (if supported by your HX711 module) | Supply |

Load-cell wire colours are **not universal**. Follow the load-cell manufacturer's E+/E−/A+/A− identification, not colour alone. See [wiring guide](docs/wiring.md).

## Build and upload

1. Install [Visual Studio Code](https://code.visualstudio.com/) and the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode).
2. Open this repository folder as a PlatformIO project.
3. Copy `include/secrets.example.h` to `include/secrets.h` and enter your 2.4 GHz Wi-Fi SSID and password. `secrets.h` is ignored by Git.
4. Connect the ESP32 by USB. Select the correct serial port if PlatformIO does not detect it automatically.
5. Build and upload using PlatformIO. Open the serial monitor at **115200 baud**.
6. Start with the scale platform **empty**: the firmware waits three seconds and performs an automatic tare on startup.
7. Note the ESP32 IP address shown in the serial monitor. For reliable Panda Touch operation, consider a DHCP reservation in your router.

**Configuration:** `platformio.ini` uses `espressif32@6.12.0`, `esp32doit-devkit-v1`, Arduino, and `bogde/HX711@^0.7.5`. If your ESP32 differs, change the board setting accordingly.

## Calibration

1. Remove all weight from the scale and send `TARE` through the serial monitor.
2. Place a known weight (for example, 200 g) on the platform, and wait until it is still.
3. Send `CAL 200` (substitute the actual mass in grams).
4. Send `FACTOR` to confirm the stored calibration factor.
5. Remove and replace the known weight and check the displayed reading. Repeat calibration if necessary.

The calibration factor is stored in ESP32 NVS and survives restarts. Tare is performed again on each boot, so the platform must be empty at startup. See [calibration guide](docs/calibration.md).

## HTTP API

Substitute the ESP32's IP address for `<SCALE_IP>`.

| Method | Endpoint | Function |
|---|---|---|
| `GET` | `/api/weight` | Latest weight in grams and calibration factor |
| `POST` | `/api/tare` | Zero the scale remotely |

Example successful `GET /api/weight` response:

```json
{"weight":200.0,"unit":"g","calibration":422.6200}
```

The numeric values are examples, not guaranteed measurements. If there is no recent valid reading, the endpoint responds with HTTP 503. See [API reference](docs/api.md).

## Serial commands

| Command | Description |
|---|---|
| `TARE` | Zero the scale with an empty platform |
| `CAL 200` | Calibrate with a 200 g known mass (change as needed) |
| `FACTOR` | Print current calibration factor |
| `WIFI` | Print connection status and IP address |
| `HELP` | List available commands |

## Panda Touch integration

The Panda Touch Filament Manager polls `/api/weight` and sends `POST /api/tare`. Set the scale's local IP address in the Panda Touch firmware configuration. Weight stability detection and Spoolman inventory updates are performed by the **Panda Touch project**, not by this scale firmware.

## Troubleshooting

- **HX711 not ready:** Check power, ground, DOUT, SCK, and the load-cell amplifier.
- **Wrong or negative weight:** Tare with an empty platform and recalibrate using a known mass. Negative calibration factors may be valid depending on wiring.
- **Can't connect over Wi-Fi:** Check the 2.4 GHz network details, signal strength, and IP address reported over serial.
- **Weight endpoint returns 503:** The HX711 may be unavailable or a fresh reading has not yet been collected.
- **Weight changes after reboot:** The firmware tares at startup; do not leave a spool on the platform during boot.

## Planned improvements

- Remote calibration and settings API
- More detailed health/status reporting
- Improved device discovery and network configuration
- Additional diagnostics and documentation for mechanical designs

## Security and licensing

This prototype HTTP API has **no authentication**. Use it only on a trusted local network; do not expose port 80 to the internet. Do not commit `include/secrets.h` or real Wi-Fi credentials. A license has not yet been selected; add a `LICENSE` file before inviting external contributions or redistribution.
