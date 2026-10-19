# Wiring guide

## ESP32 to HX711

| HX711 pin | ESP32 connection |
|---|---|
| DOUT (DT) | GPIO16 |
| SCK (CLK) | GPIO17 |
| GND | GND |
| VCC | 3.3V, **only if supported by your HX711 module** |

Check the supply-voltage requirements and logic levels of your particular HX711 breakout. All devices must share ground. Avoid feeding 5 V signals directly into ESP32 GPIO pins.

## Load cell to HX711

Connect the load cell's excitation and signal pairs to the HX711 terminals marked `E+`, `E-`, `A+`, and `A-`. Wire colours vary by manufacturer; check the sensor datasheet or measure the bridge before connecting. Do not assume a colour mapping without verifying your specific cell.

## Mechanical considerations

Mount the fixed end of the bar load cell firmly and support the spool platform on its measuring end. Prevent the platform from touching the case or base, which would bypass the sensor and cause incorrect readings. Keep wires free of mechanical tension.
