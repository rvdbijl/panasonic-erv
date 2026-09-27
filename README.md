# Panasonic ERV for ESPHome

An ESPHome **external component** for the Panasonic **FV-16VEC1S** BalancedHome
ERV's wall-controller UART. It exposes ventilation controls and measured status
to Home Assistant through ESPHome's native API. It is not a cloud integration.

The protocol was recovered from this model's original controller and tested in
a standalone ESP32 replay application. The ESPHome component has been compiled
and tested offline; **it has not yet been installed or hardware-tested under
ESPHome**. See [validation](docs/VALIDATION.md) for the exact evidence and limits.

## What it provides

- Mode selector: **Standby, Low, High, Boost**.
- Optional Power and Boost switches.
- Six independently editable airflow presets: Low/High/Boost × **SA/EA**.
- Measured supply/exhaust flow, indoor/outdoor temperature and humidity, and
  a display-correlated power reading.
- Connection, control-readiness, fault-code, frame-count and command-result
  diagnostics.
- An atomic six-preset action for automation, alongside normal ESPHome number,
  select and switch actions.
- Configurable ESP32 UART pins, polarity, RX pull-down and communication timing.

**SA** means supply air; **EA** means exhaust air. Public entities use explicit
names. The raw protocol pairs are EA then SA, verified through separate supply
and exhaust setting changes. Temperature values are published in °C; Home
Assistant can display °F according to your preferences.

## Compatibility

| Item | Support |
|---|---|
| ERV protocol | FV-16VEC1S, captured 73-byte status / 59-byte write format |
| ESPHome | 2026.9.0 minimum; tested with 2026.9.0 |
| ESP32 frameworks | ESP-IDF examples compiled on ESP32-S3 and classic ESP32 |
| Arduino framework / other ESP32 variants | Not in the current compile-test matrix |
| ESP8266, RP2040, external/virtual UART bridges | Not supported by this component |
| Other Panasonic ERVs | Unverified; similar names/connectors do not establish compatibility |
| Original controller connected as another transmitter | Not supported |
| Below-zero °F / controller Celsius wire encoding | Not decoded; see sensor limitations |

Different board pins are supported through the standard `uart:` configuration;
that does **not** make another ERV model's protocol or electrical interface
compatible. Read [model support and protocol limits](docs/PROTOCOL.md) before
adapting this component.

## Hardware first

Use the [wiring guide](docs/WIRING.md) and the
[printable schematic](docs/wiring.pdf) ([SVG](docs/wiring.svg)). The working
interface on the tested unit is:

- ERV **TX → 2.2 kΩ → ESP RX node**, with **4.7 kΩ from RX node to ground**.
- ESP **TX → 1.0 kΩ → ERV RX**, with the OEM controller TX disconnected.
- Shared signal ground; ESP powered through USB, **not** the ERV's 12 V pin.
- 4800 baud, 8 data bits, even parity, one stop bit, inverted RX and TX.

The receive divider is for the measured **4.16–4.48 V ERV TX line**. It is **not
suitable for the approximately 6.7 V OEM-controller TX line**. The wiring guide
explains measurement, internal-pull loading, pin selection and signal direction.
A series resistor is not a level shifter or isolation barrier.

## Add it to an ESPHome configuration

ESPHome automatically discovers this repository's `components/panasonic_erv/`
directory. No custom C++ include, HACS installation, Python package installation,
or Home Assistant custom integration is required.

```yaml
external_components:
  - source: github://rvdbijl/panasonic-erv@v0.1.0
    components: [panasonic_erv]
```

Start with the [complete ESP32-S3 example](examples/panasonic-erv.yaml).
Copy [secrets.yaml.example](examples/secrets.yaml.example) to a private
`secrets.yaml`, replace the placeholders, and generate your own API key:

```sh
openssl rand -base64 32
```

Choose a board definition and pins matching your hardware. The example uses
GPIO16/GPIO17 on the tested ESP32-S3, but these are configurable:

```yaml
uart:
  id: erv_uart
  tx_pin:
    number: GPIO17
    inverted: true
  rx_pin:
    number: GPIO16
    inverted: true
  baud_rate: 4800
  data_bits: 8
  parity: EVEN
  stop_bits: 1
  rx_buffer_size: 512
  rx_full_threshold: 1
  rx_timeout: 3

panasonic_erv:
  id: erv
  uart_id: erv_uart
  rx_pull_down: true

select:
  - platform: panasonic_erv
    panasonic_erv_id: erv
    mode:
      name: ERV Mode
```

The abbreviated snippet is not a whole device configuration; the complete
example also includes ESP32, logging, Wi-Fi, API and OTA configuration. Use a
dedicated UART. Do not send ESPHome logs or unrelated UART writes over it.

For development, replace the GitHub source with a local components directory:

```yaml
external_components:
  - source:
      type: local
      path: /absolute/path/to/panasonic-erv/components
    components: [panasonic_erv]
```

See the official [ESPHome external-component documentation](https://esphome.io/components/external_components/)
for source/ref/refresh options. Use a release tag or a commit SHA for repeatable
builds. `@main` is useful for development but can change without warning.

## Operation

The component starts **polling**, not replaying a previous control command, on
boot. It waits for a fresh checksum-valid status before accepting writes. Mode,
switch and number entities publish the **ERV's readback**, rather than assuming
a request succeeded.

Commands are serialized, checked against subsequent status, and not retried
automatically. Connection loss clears pending edits; reconnecting does not
replay old requests. Changing presets uses the last confirmed values for the
other channels. Check the `command_result` text sensor if an edit is rejected.

A preset is a target, not a guarantee of actual flow. Duct pressure and the
ERV's control logic can keep measured flow below a target. Allow approximately
a minute after a mode change to assess flow, and use the measured-flow sensors.

Controls retain their last known state during an ERV-link outage; their values
are not evidence that the ERV is still responding. Use `connected` and
`control_ready` in automations. Measured sensors become unavailable after the
status timeout. The ESP's API connection alone is not an ERV connection check.

## Documentation

- [Device Builder adoption](docs/ADOPTION.md): discovery, personal YAML and
  migration from the earlier replay firmware.

- [Wiring and printable schematic](docs/WIRING.md): voltages, resistor loading,
  configurable pins, installation checks and original-controller recovery.
- [Configuration reference](docs/CONFIGURATION.md): every component option,
  entity, action, units and queue behavior.
- [Home Assistant usage](docs/HOME_ASSISTANT.md): controls and an optional CO₂
  automation example with hysteresis; nothing is installed automatically.
- [Protocol reference](docs/PROTOCOL.md): framing, checksums, verified offsets,
  captured commands, unknown fields and compatibility limits.
- [Troubleshooting](docs/TROUBLESHOOTING.md): no response, faults, stale values,
  rejected writes, UART settings and voltage checks.
- [Validation](docs/VALIDATION.md): build/test evidence and untested behavior.
- [Contributing](CONTRIBUTING.md): isolated development, tests and new-model reports.

## Development checks

```sh
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements-dev.txt
pytest -q
python tools/prepare_examples.py --board s3
esphome compile build/s3-local/test.yaml
```

These commands build locally; they do not run `esphome upload` or `esphome run`,
modify a dashboard/device configuration, or contact an ERV. The helper supplies
temporary build-only credentials. Do not deploy its generated configurations.
GitHub Actions runs native/configuration tests and both example builds.

MIT licensed. This is an independent community project, not a Panasonic product
or an official ESPHome integration. Panasonic and ESPHome names identify the
hardware and software this project works with.
