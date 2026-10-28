# CerePhyte Firmware

<p align="y">
  <img src="images/isleri_logo.jpg" alt="ISLERI organization logo" width="180">
</p>

Firmware for a two-board sensor system. An Arduino reads sensors and controls
two 8-channel multiplexers; an ESP8266 receives the readings over serial and
exposes them through a Wi-Fi access point and a small HTTP API.

## Repository contents

| Path | Purpose |
| --- | --- |
| [`Cerephyte_Arduino_Master/Cerephyte_Arduino_Master.ino`](Cerephyte_Arduino_Master/Cerephyte_Arduino_Master.ino) | Samples the sensors, controls the multiplexers, and sends serial telemetry. |
| [`Cerephyte_NodeMCU_REST_API_AP/Cerephyte_NodeMCU_REST_API_AP.ino`](Cerephyte_NodeMCU_REST_API_AP/Cerephyte_NodeMCU_REST_API_AP.ino) | Creates the ESP8266 Wi-Fi access point, parses telemetry, and serves HTTP endpoints. |
| [`images/isleri_logo.jpg`](images/isleri_logo.jpg) | isleri organisation logo used above. |
| [`LICENSE`](LICENSE) | Apache License 2.0. |

## Hardware and wiring

For the hardware design, component details, and wiring and pinout guidance,
see the [CP hardware resources repository](https://github.com/ISLERI-labs/CP_hardware_resources).

## Build and upload

1. Install the Arduino IDE or Arduino CLI and the appropriate board support
   packages for your Arduino board and ESP8266 NodeMCU.
2. Install these Arduino libraries through the Library Manager:
   - **Adafruit ADS1X15**
   - **Adafruit INA219**
3. Open and upload `Cerephyte_Arduino_Master/Cerephyte_Arduino_Master.ino`
   to the Arduino master.
4. Review the ESP8266 access-point settings in
   `Cerephyte_NodeMCU_REST_API_AP/Cerephyte_NodeMCU_REST_API_AP.ino`. Change
   the network name and password to values appropriate for your deployment;
   do not use the source's example credentials for a production or shared
   environment.
5. Select an ESP8266 NodeMCU board and upload
   `Cerephyte_NodeMCU_REST_API_AP/Cerephyte_NodeMCU_REST_API_AP.ino`.
6. Connect the hardware following the
   [CP hardware resources](https://github.com/ISLERI-labs/CP_hardware_resources).
   Join the Wi-Fi network configured in the ESP8266 sketch and open
   `http://192.168.4.1/`.

The Arduino sends a `DATA` packet every 100 ms at 9600 baud. The ESP8266
expects the comma-separated fields in this order:

```text
DATA,sample,mux1_channel,mux2_channel,voltage,bus_voltage,current_mA,t2,t3
```

The Arduino's USB serial interface runs at 115200 baud for serial monitoring.

## HTTP API

The ESP8266 runs as a Wi-Fi access point and serves requests at
`http://192.168.4.1` by default.

| Method and path | Description |
| --- | --- |
| `GET /` | Lists the available API routes. |
| `GET /api/data` | Returns the latest valid telemetry as JSON. |
| `GET /api/mux1?channel=0` | Selects channel 0–7 on multiplexer 1. |
| `GET /api/mux2?channel=0` | Selects channel 0–7 on multiplexer 2. |

Example data response:

```json
{
  "ok": true,
  "sample": 42,
  "mux1_channel": 0,
  "mux2_channel": 0,
  "voltage": 1.234,
  "bat": 12.34,
  "curr": 56.78,
  "t2": 0,
  "t3": 1,
  "age_ms": 12
}
```

Before the first valid serial packet, `/api/data` returns `ok: false` and null
telemetry fields. An invalid or missing multiplexer channel returns HTTP 400;
valid channels are integers from 0 through 7.

## License

Copyright and License

Copyright © 2026 Omm Prakash Das.

This repository's original firmware code is licensed under the Apache License, Version 2.0. See "LICENSE" (LICENSE) for the full license text.

Third-party components remain subject to their respective licenses.