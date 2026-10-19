# Calibration guide

The firmware uses the `bogde/HX711` library. Calibration is performed using USB serial commands and stored with ESP32 `Preferences` under namespace `filament-scale`, key `factor`.

1. Boot the scale with the platform empty. It performs an initial tare after a three-second delay if the HX711 is ready.
2. Open PlatformIO's serial monitor at **115200 baud** and select a line ending that sends a newline.
3. Send `TARE` with the platform empty.
4. Add a known mass between 0 and 5000 g, such as 200 g. Let it settle.
5. Send `CAL 200`. The firmware averages 20 raw samples, calculates counts per gram, and stores the factor.
6. Send `FACTOR` to inspect the saved value.
7. Verify against one or more known masses.

`CAL` expects the known mass in grams and assumes the empty platform has already been tared. Do not calibrate with a spool of unknown weight. The firmware's initial fallback factor is `422.6200` counts/g; a previously saved factor overrides it.

**Important:** The tare offset is not persisted; a new tare occurs at startup. Never reboot with a spool on the scale unless you intend that spool to be zeroed out.

Remote tare is supported via HTTP, but **remote calibration is not**. Calibration requires serial access in this version.
