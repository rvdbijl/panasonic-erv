# Troubleshooting

Begin with [the wiring guide](WIRING.md). Change one thing at a time and keep
the original controller available. A running ESP or an online Home Assistant
API does not establish that the ERV UART is working.

## No valid status / Connected is off

1. Check ERV power, shared ground and the actual TX/RX signal directions.
2. Confirm ESP RX receives **ERV TX through the divider**, not the controller's
   higher-voltage TX line. RX/TX were initially reversed in the original bench
   investigation; correcting them immediately produced complete replies.
3. Confirm 4800 baud, 8 data bits, EVEN parity, 1 stop bit, RX/TX inverted for the documented
   circuit. An outgoing scope decode alone does not prove the correct ERV pin
   receives it.
4. Check the loaded receive-node waveform and margins against your MCU's
   datasheet. See the divider calculations; do not attach a raw 4.4 V/6.7 V signal
   directly to a 3.3 V input.
5. Confirm the OEM TX is disconnected and the ESP is the sole transmitter.
6. Check UART pin ownership, logger settings and board flash/PSRAM pin conflicts.

The standard poll is 12 bytes. A valid own-model status is 73 bytes beginning
`A5 A5 5A 5A` with operation/object `03 0B`. The component polls once per second
by default; a correct RX path should keep the valid-frame counter increasing.

## Connected is on, Control Ready is off

The device is readable but its status is too old for a write, its presets are
outside the supported range/order, or an uncharacterized configuration field
has changed from the captured profile. Check command-result logs and compare
raw status with [the protocol reference](PROTOCOL.md). Do not remove profile
checks simply to make controls activate; that could overwrite unknown settings.

The write freshness limit is 5 seconds, even if status_timeout is longer.
Alternative ventilation/recirculation modes have not been mapped. Return to
the known controller settings or report a complete labeled capture.

## A number snaps back / a command is rejected

The component publishes actual readback rather than optimistic requested state.
Check that each value is an integer 30–160 and that Low ≤ High ≤ Boost holds for
**each** channel. Raise the upper presets first or use the atomic six-preset
action when a sequence of individual edits would violate ordering.

The waiting queue is bounded to 8 requests. Rapid repeated writes can overflow
it. Rejected, timed-out or cancelled commands appear in `command_result` with
an ID. Commands are not retried automatically. After a timeout, dependent queued
edits are cancelled so they cannot unexpectedly execute against an uncertain
state. Reconnection does not replay old requests.

## Readback matches, but airflow differs from the target

The confirmation checks reported mode/presets, not measured CFM. Fan settling,
duct static pressure, auto-balancing and ERV operating logic affect actual flow.
Allow approximately a minute before evaluating it. Use the measured SA/EA
sensors, not only the preset numbers. In the original tests a nominal 90/90
Boost target produced substantially unequal actual flows.

A confirmation of a state that was already reported is not proof that a
redundant write was accepted. Change one known setting and observe both readback
and resulting behavior when verifying a new installation.

## F01 or another fault

F01 is the original wall-controller communication error. The standalone bench
observed it clear with polling, but this is not a guarantee that every fault
will clear automatically. The component reports literal fault codes and a
nonzero-fault binary sensor; it does not issue fault-reset commands.

Use the correct Panasonic manual and normal service procedures for other
faults. Preserve the code and the conditions under which it appeared. Do not
hide a motor/sensor/damper fault by treating it as a communications-only issue.

## Temperatures/RH unavailable or unexpected

In Standby the observed unit sent temperature 127 and humidity 255. These are
published as unavailable, not 127°F/255%. Measurements also become unavailable
when valid status stops arriving.

The current temperature mapping assumes the captured unsigned Fahrenheit wire
encoding. The component converts supported values to Celsius for ESPHome/HA;
HA may display Fahrenheit. Below-zero °F values and a controller's Celsius wire
mode are unverified and are not decoded. Humidity outside 0–100 is suppressed.
See the exact limitations in [configuration](CONFIGURATION.md).

## After changing GPIOs or the electrical interface

Update the existing UART's pin definitions, not the library source. GPIO labels
are not physical connector positions. Keep the correct inversion and framing.
If you disable `rx_pull_down`, recalculate and remeasure the divider voltage;
internal pull loading was part of the working circuit. UART bridges and non-ESP32
boards are outside this release's support scope.

## Collecting useful diagnostics

Include the ESPHome version, board/module, ERV and controller model/revision,
UART YAML, resistor values, measured signal levels, fault code, command-result
sequence and expected/observed SA/EA values. Redact Wi-Fi credentials, API keys
and personal network details before sharing YAML/logs.

ESPHome's [UART debug facility](https://esphome.io/components/uart/#debugging)
can log raw RX/TX data on the existing UART; leave `dummy_receiver` false so
this component remains the receiver. Do not enable another component that
consumes the UART stream. Keep captures around the actual command transition,
including the full following status response, not only a screenshot of hex.

If the setup must be restored, power down, disconnect ESP TX, and restore the
OEM controller wiring before repowering. Never leave both transmitters driving
ERV RX.
