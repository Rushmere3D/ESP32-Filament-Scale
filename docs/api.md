# HTTP API reference

The ESP32 runs an HTTP server on port **80** on the local Wi-Fi network. No authentication or TLS is implemented.

## `GET /api/weight`

Returns the latest valid weight reading, in grams, and current calibration factor.

Example request: `http://<SCALE_IP>/api/weight`

**HTTP 200** (example):

```json
{"weight":200.0,"unit":"g","calibration":422.6200}
```

- `weight`: numeric measured weight in grams, rounded to one decimal place in the response
- `unit`: always `g`
- `calibration`: current counts-per-gram calibration factor

**HTTP 503** when no valid reading is available or the last reading is more than three seconds old:

```json
{"error":"No recent weight reading"}
```

## `POST /api/tare`

Requests a tare operation; ensure the platform is empty and still.

**HTTP 200**:

```json
{"success":true,"message":"Tare complete"}
```

**HTTP 503** if the HX711 is not ready:

```json
{"success":false,"error":"HX711 not ready"}
```

Example using curl:

```bash
curl -X POST http://<SCALE_IP>/api/tare
```

Tare temporarily invalidates the cached weight. Allow a new measurement before calling `/api/weight` again.

## Timing and limitations

The firmware attempts to sample every 250 ms and logs readings to serial. Network and HX711 operations can take longer when the hardware is unresponsive. The API has no authentication, device discovery, calibration endpoint, or streaming updates. Do not expose it outside a trusted LAN.
