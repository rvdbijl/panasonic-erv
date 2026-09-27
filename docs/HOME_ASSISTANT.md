# Home Assistant usage

Building this repository does not add a device to Home Assistant, change an
ESPHome dashboard, or flash an ESP. An initial hardware deployment has verified
ESPHome polling/readback and adoption discovery; each installation still needs
commissioning before enabling automatic control.

## Connecting and commissioning

After separately installing firmware built from your own configuration, add the
device through Home Assistant's **ESPHome** integration and provide the API
encryption key from your private secrets file. Discovery may offer the device;
otherwise use its hostname/address. See the official
[ESPHome integration instructions](https://www.home-assistant.io/integrations/esphome/).
No custom Home Assistant component is required.

Check the device's entities before enabling automations:

1. `ERV Connected` and `ERV Control Ready` should be on. Compare measured supply
   and exhaust flow, temperature, humidity and power with independent readings.
2. Select Low, High, Boost and Standby individually. Check command-result readback
   and allow about a minute for flow to settle. Restore the desired normal mode.
3. Change presets only deliberately, with the commissioning values recorded.
   Confirm all six readbacks after each save. A flow target and actual airflow
   are different measurements.
4. Verify loss/recovery behavior with the installation attended. Mode, switch
   and number entities retain their last known values when the ERV link fails;
   `connected` and `control_ready` are the communication checks.

Entity IDs depend on your device/entity names and previous registrations.
Select the actual IDs from **Settings → Devices & services → Entities**; do not
assume the placeholder IDs below will match your installation. Temperature
sensors publish °C; Home Assistant can display your preferred temperature unit.
The ERV temperature decoder currently does not support below-zero °F or an
unverified Celsius wire mode. See [configuration limitations](CONFIGURATION.md).

## Routine control

Use the Mode select for routine operation. For example, a Home Assistant action
can request High using the standard
[`select.select_option`](https://www.home-assistant.io/integrations/select/) action:

```yaml
action: select.select_option
target:
  entity_id: select.panasonic_erv_mode  # Replace with your actual entity ID.
data:
  option: High
```

Options are case-sensitive: `Standby`, `Low`, `High`, `Boost`. Selecting Low or
High powers the ERV on and cancels Boost. Boost-off returns to the underlying
Low/High selection. The optional Power and Boost switches represent the same
underlying state; they are not separate fan controllers.

## Optional CO₂ example

This is an illustrative **Home Assistant automation**, not ESPHome device YAML.
It is not installed or enabled by this repository. Create an input-boolean
helper called `input_boolean.erv_automatic_ventilation`, initially off, and use
it as an explicit automation enable/manual override. Replace every entity ID.
Choose thresholds and commissioned airflow for your building; the numbers
below are examples, not a ventilation sizing recommendation.

The example requests High above 1,000 ppm for five minutes and Low below 800 ppm
for ten minutes. Between thresholds it leaves the mode unchanged. It requires
a numeric CO₂ reading, an available Low/High mode, a working ERV link, and no
reported fault. It leaves manual Standby and Boost alone. Disable the helper
when you want manual Low/High control to persist.

Paste into a single automation's **Edit in YAML** editor:

```yaml
alias: ERV example CO2 ventilation
mode: single
triggers:
  - trigger: numeric_state
    entity_id: sensor.living_room_co2
    above: 1000
    for: "00:05:00"
    id: high
  - trigger: numeric_state
    entity_id: sensor.living_room_co2
    below: 800
    for: "00:10:00"
    id: low
conditions:
  - condition: state
    entity_id: input_boolean.erv_automatic_ventilation
    state: "on"
  - condition: state
    entity_id: binary_sensor.panasonic_erv_connected
    state: "on"
  - condition: state
    entity_id: binary_sensor.panasonic_erv_control_ready
    state: "on"
  - condition: state
    entity_id: binary_sensor.panasonic_erv_fault
    state: "off"
  - condition: template
    value_template: >-
      {{ is_number(states('sensor.living_room_co2')) and
         states('select.panasonic_erv_mode') in ['Low', 'High'] }}
actions:
  - action: select.select_option
    target:
      entity_id: select.panasonic_erv_mode
    data:
      option: "{{ 'High' if trigger.id == 'high' else 'Low' }}"
```

Numeric-state triggers fire on threshold crossings; they are not a periodic
retry mechanism. The `for` timer resets on automation reload/Home Assistant
restart. A crossing skipped because a condition was false will not automatically
be replayed when the link or helper recovers. Reassess the current reading/mode
manually after recovery, or design a separate supervised recovery policy.
These semantics are described in the official
[automation trigger documentation](https://www.home-assistant.io/docs/automation/trigger/).

A sensor that keeps reporting an old numeric value is different from one marked
unavailable. Configure availability/staleness detection in the CO₂ sensor's own
integration and test it before relying on automatic control. This example has
not been run in a live Home Assistant instance.

## Adding temperature and humidity policy

Expose the readings first and compare them over time before introducing more
control rules. Relative humidity depends on temperature: compare indoor/outdoor
moisture using a temperature-aware measure such as dew point or absolute
humidity, rather than assuming a lower outdoor RH always means drier air.
Account for unavailable temperatures and the decoder's cold-weather limitation.

Keep one automation responsible for the final mode, with explicit priority for
manual override, communication failure, CO₂ demand and moisture/temperature
constraints. Otherwise separate automations can repeatedly undo each other.
Use commissioned Low/High/Boost presets for normal modulation; preset-save
persistence and write endurance have not been characterized. The component
does not itself implement CO₂ policy, frost protection overrides, or a thermostat.
