# Attic-ERV

[`examples/attic-erv.yaml`](../examples/attic-erv.yaml) is the complete device
configuration for the owner's ESP32-S3 DevK 1.0 board: ESP32-S3-WROOM-1,
16 MB flash, CP2102 USB bridge, and an addressable LED labeled **RGB@IO48**.
The device name is `attic-erv`; its display name is **Attic-ERV**.

## First Wi-Fi setup

The initial firmware contains no home Wi-Fi credentials. Configure them privately
using either method:

- Connect to **Attic-ERV Setup**, then open `http://192.168.4.1` if the setup
  portal does not open automatically. This setup hotspot has no password.
- Keep USB connected and use [ESPHome Web](https://web.esphome.io/) in a browser
  supporting Web Serial. Connect to the CP2102 port and use its Wi-Fi setup flow.
  The firmware already includes Improv Serial; no replacement firmware is needed.

Once connected to the LAN, the device advertises ESPHome adoption metadata and
its native API. Adopt it in ESPHome Device Builder, and add its discovered ESPHome
integration in Home Assistant. Its hostname is `attic-erv.local`.

## Keep this configuration after adoption

The advertised `v0.1.1` adoption URL points to the repository's generic package.
It does **not** contain this local board-specific LED configuration. Before
installing from Device Builder, replace its generated YAML with the complete
`examples/attic-erv.yaml`, and retain your private Wi-Fi/API/OTA settings there.
Do not put those credentials in this repository.

For example, your private Device Builder configuration can add `ssid` and
`password` secret references under the existing `wifi:` section. Preserve any
API encryption or OTA authentication added during adoption.

## RGB indication

- **Green, 80 ms:** the checksum-valid ERV status counter advanced. This indicates
  received status, rather than merely transmitting a poll. Under normal polling,
  a reply arrives approximately once a second.
- **Red, 250 ms:** the parser rejection counter advanced.
- **Repeating red, once a second:** no recent ERV status, or an ERV-reported fault.
  Persistent error indication begins after a 10-second startup grace period.
- Red takes priority over green. Brightness is 20%, transitions are immediate,
  and the LED starts off. It is an internal indicator rather than a controllable
  Home Assistant light entity.

The ERV link timeout remains 10 seconds. The LED represents ERV communication
and fault state; it is not a Wi-Fi connection indicator. Without an ERV attached,
repeating red is expected. A parser rejection includes malformed frames and
expired partial frames, not every possible hardware UART error.

GPIO48 is confirmed by this board's printed LED label. Other revisions may use
a different pin; adjust `status_led_pin` only after checking that board. The ERV
UART remains GPIO16 RX / GPIO17 TX, while USB logging and Improv use UART0.

## Local build and installation

From the repository root with its development environment installed:

```sh
.venv/bin/esphome config examples/attic-erv.yaml
.venv/bin/esphome compile examples/attic-erv.yaml
```

Uploading is a separate, explicit operation targeting the identified ESP USB
port. These build commands do not flash or actuate an ERV. See the main wiring
guide before connecting ERV signals.
