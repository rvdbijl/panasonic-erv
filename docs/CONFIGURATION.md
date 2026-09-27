# Configuration and entity reference

Use [the complete example](../examples/panasonic-erv.yaml) as a starting point.
Every entity is optional. The hub polls and decodes even if only a few entities
are configured. Multiple hubs are allowed, each with a separate native ESP32
UART; select the hub using `panasonic_erv_id` in each entity platform block.

## Hub: `panasonic_erv:`

| Option | Default | Meaning |
|---|---|---|
| `id` | generated | ESPHome ID, used by entities/actions |
| `uart_id` | required / inferred by ESPHome when unambiguous | Dedicated native ESP32 UART |
| `poll_interval` | `1s` | Minimum spacing between any transmissions; 1–5 s |
| `status_timeout` | `10s` | No recent valid status means disconnected; up to 60 s, at least 2 × poll interval |
| `command_timeout` | `8s` | Sent write must match readback within this period; up to 60 s, at least 2 × poll interval |
| `rx_pull_down` | `true` | Apply internal RX pull-down after UART setup; false disables both pulls |

Standard ESPHome component options also apply. Do not override setup priority:
the RX pull setting must be applied **after** the UART has initialized.

The referenced UART must have RX and TX pins, 4800 baud, 8 data bits, EVEN parity
and 1 stop bit. Pin numbers and inversion are standard UART options; use the
example's inverted=true on both for the documented circuit. UART sharing with
another device is rejected by ESPHome's UART validation. Logger output should
use a different interface; the example disables serial logging (`baud_rate: 0`).

A write also requires status no more than **5 seconds** old, independently of
`status_timeout`. This prevents editing against an old settings snapshot.

## Mode selector

```yaml
select:
  - platform: panasonic_erv
    panasonic_erv_id: erv
    mode:
      name: ERV Mode
      id: erv_mode
```

| Option | Request semantics |
|---|---|
| `Standby` | Power off, cancel Boost; retain selected Low/High speed and presets |
| `Low` | Power on, select Low, cancel Boost |
| `High` | Power on, select High, cancel Boost |
| `Boost` | Power on, enable Boost, retain selected Low/High speed underneath |

Readback determines the displayed selection. Invalid or unsupported mode flags
do not invent a new selection; check the diagnostics. No mode is restored or
sent on ESP startup.

## Power and Boost switches

Both are optional in a `switch: - platform: panasonic_erv` block:

- `power`: On retains selected speed/Boost; Off sends Standby and cancels Boost.
- `boost`: On powers the ERV on and enables Boost; Off cancels Boost while
  preserving power and the underlying Low/High selection.

The only accepted `restore_mode` is `DISABLED`. These are readback-controlled
switches, not GPIO outputs. An ESP reboot must not replay a saved switch state.
Switch inversion is not supported; use the normal meanings of On and Off.

## Airflow numbers

A `number: - platform: panasonic_erv` block accepts any of:

| Key | Meaning |
|---|---|
| `low_sa` / `low_ea` | Low supply/exhaust target |
| `high_sa` / `high_ea` | High supply/exhaust target |
| `boost_sa` / `boost_ea` | Boost supply/exhaust target |

All are integer CFM, 30–160, step 1, configuration-category entities. The software
conservatively enforces **Low ≤ High ≤ Boost separately for each channel**.
An individual number edit is serialized as a complete six-preset write while
preserving the other five current values. Edits are queued as intentions, then
built from fresh status at send time; two rapid channel changes do not copy the
same stale baseline over each other.

The number does not optimistically show a requested value before readback.
If the edit is rejected, the last reported value remains. Check `command_result`.
When changing multiple ordered targets, raise Boost before raising High/Low,
or use the atomic action below to avoid an invalid intermediate ordering.

## Atomic six-preset action

```yaml
- panasonic_erv.set_presets:
    id: erv
    low_sa: 30
    low_ea: 30
    high_sa: 60
    high_ea: 60
    boost_sa: 90
    boost_ea: 90
```

All six fields are required and support ESPHome lambdas. Runtime checks still
reject non-finite/fractional/out-of-range values and invalid ordering. The action
queues one save; returning from the action does not mean readback has succeeded.
Use the diagnostic result/values before making dependent decisions.

The optional [atomic-presets package](../examples/atomic-presets.yaml) defines a
script and does not execute it at startup. Do not use a commissioning-preset
write for every small CO₂ fluctuation: use the mode selector for routine control.
EEPROM/flash behavior and power-cycle persistence have not been characterized.

## Sensors

All keys below belong in a `sensor: - platform: panasonic_erv` block. Normal
ESPHome sensor options, filters and IDs are available.

| Key | Unit / precision | Meaning and limits |
|---|---|---|
| `sa_flow` | CFM, 0 decimals | Measured supply airflow |
| `ea_flow` | CFM, 0 decimals | Measured exhaust airflow |
| `indoor_temperature` | °C, 1 decimal | Positive Fahrenheit wire values converted to Celsius |
| `outdoor_temperature` | °C, 1 decimal | Same encoding limitation |
| `indoor_humidity` | %RH, 0 decimals | Indoor/return humidity, display-correlated |
| `outdoor_humidity` | %RH, 0 decimals | Outdoor humidity, display-correlated |
| `power` | W, 0 decimals | Display-correlated at 3 W standby, 60–61 W High, ~90 W Boost; wider range/width remains provisional |
| `valid_frames` | count | Checksum-valid 73-byte status count since ESP boot |
| `rejected_frames` | count | Parser rejection/partial-frame expiry count, **not** a hardware parity-error counter |

Temperatures are decoded only for raw 0–126°F; 127 is the observed Standby
unavailable sentinel and 128–255 are suppressed because signed/invalid encodings
are unverified. This is not support for an ERV configured to send Celsius bytes.
Values below 0°F cannot currently be decoded. Humidity outside 0–100% is suppressed;
Standby supplied 255. Suppressed and stale measurements publish `NaN` so clients
can show unavailable. A measured 0 CFM is valid, especially in Standby/startup.

## Binary and text diagnostics

`binary_sensor: - platform: panasonic_erv`:

| Key | Meaning |
|---|---|
| `connected` | A valid status arrived within `status_timeout` |
| `fault` | The three fault-code bytes are nonzero; last known fault state |
| `control_ready` | Fresh status and supported configuration; commands may be requested |

`control_ready` does not mean a command queue slot is guaranteed available.
The queue holds 8 waiting intentions plus one in-flight write; overflow is
rejected. Controls are deliberately unavailable for unknown configuration
profiles instead of overwriting fields with guesses.

`text_sensor: - platform: panasonic_erv`:

- `fault_code`: literal three-character code or `None`; F01 is wall-controller
  communication error. Unknown/non-printable bytes are not silently called OK.
- `command_result`: most recent event, with request ID: Queued, Sent,
  Readback matches, Timeout/not retried, Rejected, or Connection lost/cancelled.

These diagnostics are configuration/transport observations, not a safety
certification of the equipment. See [troubleshooting](TROUBLESHOOTING.md).

## Transaction behavior

A received frame must have the correct header, length and checksum before it
can update entities or confirm a write. A pending write checks power, speed,
Boost and all six presets in subsequent readback; measured flow is not its
acknowledgement because fan speed takes time to settle.

“Readback matches” describes observed state. If the ERV already reported that
state, it does not independently prove acceptance of a redundant command.
No separate acknowledgement packet has been established.

The parser handles fragmented/concatenated frames and resynchronizes after
noise. A partial frame expires after 250 ms without additional read bytes. Polls
continue while waiting for write results. Timeouts cancel dependent queued
edits; connection loss cancels pending and queued requests. Writes are never
automatically retried, persisted, or restored on reconnect/reboot.

Measured sensors become unavailable on link loss. Control and fault entities
retain their last known values; gate automations on `connected`/`control_ready`
rather than assuming the ESP's Wi-Fi connection proves ERV communication.
