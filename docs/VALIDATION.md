# Validation and known limits

Baseline: **2026-09-27**, ESPHome **2026.9.0**, Python **3.12**, ESP-IDF.
This page distinguishes earlier replay evidence from ESPHome deployment checks.

## Physical evidence from the earlier replay project

The original FV-16VEC1S controller was captured in both directions using an
ESP32-S3. Separate Low, High, Boost on/off, Standby and On actions established
59-byte command frames and subsequent 73-byte status readback. Separate supply
and exhaust preset edits established raw **EA-first, SA-second** ordering and
full six-preset save frames. The user reported that the standalone replay
application worked well after restoring active transmit wiring.

The electrical measurements and resistor circuit in [WIRING.md](WIRING.md)
come from that bench setup. They are not universal electrical specifications.
Temperature/humidity assignments were correlated with simultaneous readings;
power was correlated at approximately 3 W Standby, 60–61 W High and 90 W Boost.
Broader encodings and operating ranges remain unverified.

## Automated checks for this component

`pytest -q` passes **18 tests**, including native C++ test programs with multiple
scenarios and real ESPHome schema validation:

- Exact bytes/checksums for six captured control transitions, an asymmetric
  preset save, and restoration; incompatible profiles and invalid ranges reject.
- Fragmented frames, noise/checksum rejection, partial-frame expiry and recovery.
- No startup state replay, readback confirmation, queue bounds, preservation of
  sequential edits, stale-status rejection, timeout without retry, cancellation
  on link loss, transport failure, and millisecond-clock rollover.
- Fahrenheit-to-Celsius conversion and unavailable temperature sentinels.
- Complete YAML validation, configurable pins, UART framing rejection, timing
  bounds, prohibited switch restore modes and RX pull capability validation.
- Minimal entity-free hubs, separate UARTs for multiple hubs, rejection of
  two hubs sharing the same UART, and the adoption package with personal Wi-Fi.

The full example plus optional atomic-preset action has been compiled offline
for both `esp32-s3-devkitc-1` (16 MB) and `esp32dev` (4 MB) with ESP-IDF. These
builds compile the ESPHome integration, entities and templated action, not only
the protocol headers. Build-only credentials are generated in ignored folders.

The matching SVG/PDF schematic is generated from one script. The one-page,
Letter-landscape PDF has been rendered and visually inspected for legibility,
connection dots, signal directions, divider/ground placement and clipping.

GitHub Actions passed all three jobs (tests, ESP32-S3 build and classic ESP32
build) for release commit `223792f`:
[release validation run](https://github.com/rvdbijl/panasonic-erv/actions/runs/36345735677).
The adoption-package revision also passed tests and both builds:
[adoption validation run](https://github.com/rvdbijl/panasonic-erv/actions/runs/36353417358).
See [workflow results](https://github.com/rvdbijl/panasonic-erv/actions) for later commits. A workflow definition alone is not a passing build.

The published source `github://rvdbijl/panasonic-erv@v0.1.0` was also fetched by
ESPHome into a fresh external-component cache and compiled successfully for
ESP32-S3. The fetched commit was
`223792f073afb1e0a4e63023659c9136210b44d8`; every component source file matched
the reviewed local source. This verifies GitHub discovery and the documented
release reference as well as local-directory integration.

Reproduce the checks using [CONTRIBUTING.md](../CONTRIBUTING.md). The `--remote`
helper option tests the documented GitHub source/ref instead of the local
component. It requires network access and is meaningful after the tag exists.

## First ESPHome hardware deployment

On 2026-09-27 the ESP32-S3 was updated wirelessly from replay v0.8 to an ESPHome
2026.9.0 build using the v0.1.0 component and v0.1.1 adoption package. Only the
application was uploaded; the original bootloader, partition table and NVS were
retained. The 837,552-byte application fit the existing 3 MiB OTA slot.

Verified after reboot:

- Existing bench Wi-Fi credentials were migrated on-device; the board rejoined
  the LAN and served its ESPHome web interface.
- Native API reported project `rvdbijl.panasonic-erv`, version `0.1.1`.
- mDNS advertised the published v0.1.1 adoption-package URL.
- `connected` and `control_ready` were true, the valid-status count increased,
  and no ERV fault was reported.
- High-mode readback showed approximately SA 60 / EA 40 CFM and 43 W.
- All six pre-update presets were preserved: Low SA/EA 40/30, High 60/40,
  Boost 92/90. These are the owner's settings, not suggested defaults.
- Indoor/outdoor readings were 64/61°F (published as Celsius) and 68/79% RH.

No mode or preset command was issued in this deployment check. One parser
rejection was present after startup; a short successful observation does not
establish long-term error-free operation. The initial command-result text can
remain "Idle; awaiting status" until a control request; use `connected` and
`control_ready` for actual link state.

## What is not yet established

- Control writes from the ESPHome adapter, sustained reliability, startup
  waveforms, Wi-Fi outages and runtime recovery still need attended tests.
- Discovery was verified on the LAN; adoption inside the owner's Device Builder
  and Home Assistant integration are separate user-side steps.
- Other ERV models/revisions and other ESP32 variants/frameworks are unverified.
  Successful compilation on classic ESP32 does not establish board pin safety.
- The example Home Assistant automation is documentation, not a live-system test.
- Negative Fahrenheit/Celsius wire formats, unknown configuration flags, power
  field width across its full range, persistent-save behavior and write endurance
  remain open questions. See [PROTOCOL.md](PROTOCOL.md).

Readback proves reported state, not independent command acceptance when the
requested state was already present. Airflow response is physical evidence
separate from a settings readback. No universal Panasonic compatibility claim
or electrical certification is implied by these tests.
