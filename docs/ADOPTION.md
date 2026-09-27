# Adopting an ESP32-S3 into ESPHome Device Builder

The [adoption package](../examples/panasonic-erv-adopt.yaml) adds ESPHome project
metadata, mDNS discovery and a browser control page to the released protocol
component. It requires ESPHome **2026.9.0 or newer** and targets an ESP32-S3 with
16 MB flash. GPIO16/GPIO17 remain configurable substitutions.

## An already-installed adoption firmware

1. Put Device Builder and the ESP on networks that allow discovery and direct
   connections. The device advertises the project `rvdbijl.panasonic-erv`.
2. Adopt/take control of the discovered **Panasonic ERV** device in Device Builder.
   It creates your own YAML referencing the public package. Configure your Wi-Fi
   secrets and review the generated configuration before installing it.
3. Add the device through Home Assistant's ESPHome integration. If adoption
   creates an API encryption key, use that key after installing that build.
4. Verify **ERV Connected**, **ERV Control Ready**, actual SA/EA flow and fault
   code. Merely seeing the ESP online does not verify the ERV serial connection.

The installed migration firmware has no web login or API key, matching the
previous bench interface. The public adoption package leaves API encryption and
OTA credentials to the owner's configuration. Device Builder can generate a
per-device API key; none is shared in this repository. The browser page is on
port 80. No recurring ERV mode or preset writes are configured at boot.

If discovery does not reach your Device Builder, create a device with the YAML
below, edit it there, and install wirelessly using the board's current IP. This
is an **ESP32-S3** example, not a universal board definition:

```yaml
substitutions:
  name: panasonic-erv
  friendly_name: Panasonic ERV
  erv_rx_pin: GPIO16
  erv_tx_pin: GPIO17

packages:
  erv: github://rvdbijl/panasonic-erv/examples/panasonic-erv-adopt.yaml@v0.1.1

esphome:
  name: ${name}
  friendly_name: ${friendly_name}
  name_add_mac_suffix: false

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  # Set use_address to the board's current IP if hostname resolution fails.

# Optional: generate your own key with openssl rand -base64 32.
# api:
#   encryption:
#     key: !secret erv_api_encryption_key
```

The public package uses the released `v0.1.0` protocol implementation. Its
`0.1.1` project version identifies the added adoption configuration, not a new
protocol mapping. It disables API/Wi-Fi watchdog reboots so losing Home Assistant
or Wi-Fi does not routinely interrupt ERV polling.

## Migrating from the earlier replay firmware

A bench firmware upload is an **application-only OTA**, not a factory flash.
It preserves the bootloader, partition table and NVS. The bench board has two
3 MiB application slots, with NVS at `0x9000` and OTA data at `0xE000`.
A migration build must fit those slots and use that known layout for build-time
size checks. Never send a merged factory image to the bench `/update` route.

The one-time migration build copies the saved `erv-bench` SSID/password into
ESPHome's Wi-Fi preferences on the device. It does not log, export or commit the
credentials. Existing ESPHome Wi-Fi settings take precedence. The original
namespace remains available for returning to the bench firmware. The existing
password-protected setup hotspot is retained as a recovery path for this build.
These migration-specific files and private hotspot credentials are not part of
the general adoption package.

Normal ESPHome application OTA does not replace the existing partition table.
`allow_partition_access: false` is explicit in the adoption package. On a migrated
bench board, keep application images below **3 MiB**, regardless of a build's
larger default flash/partition-size estimate. Do not request partition-table or
bootloader changes as part of routine updates. A future deliberate USB factory
flash can establish a different layout, but that is a separate operation.

Keep a copy of the matching replay application binary and original partition
CSV before migration. A successful ESPHome installation can receive a replacement
application through its native OTA service. If no network update path remains,
USB recovery may be necessary; automatic rollback is not guaranteed.

After adoption the public package does not include the one-time NVS migration
code or private fallback-hotspot settings. Supply your Wi-Fi configuration in
your own YAML. Add a private `wifi.ap` password and `captive_portal` there if you
want to retain a setup hotspot in future builds.

ESPHome's official [device sharing guide](https://esphome.io/guides/creators/)
describes the project metadata and dashboard-import mechanism. General wiring,
configuration, sensor limitations and automation guidance remain in this repo's
other documentation.
