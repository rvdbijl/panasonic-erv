# Captured protocol and support boundary

This is a record of the **FV-16VEC1S** bench investigation, not an official
Panasonic protocol specification. Numeric offsets below are zero-based from
the start of the complete frame. Raw bytes and expected replies used by the
regression suite are embedded in `tests/replay_protocol_test.cpp` and
`tests/fixtures/status.h`; they contain no Wi-Fi credentials or host addresses.

## Electrical framing

- UART: 4800 baud, 8 data bits, EVEN parity, 1 stop bit.
- Both directions are inverted; physical idle is low.
- Header: `A5 A5 5A 5A`.
- Bytes 4–5: checksum, little-endian.
- Bytes 6–7: operation/object.
- Bytes 8–9: payload length, little-endian; total frame length=`12+length `.
- Bytes 10–11: captured address/header values `01 00`; not generalized.

Checksum for all captured polls/status/writes:

`(0xC0AD + sum(frame[6:])) & 0xFFFF`

This matches the captured families. It is not a claim that every Panasonic
message type uses the same algorithm. The parser requires exact frame length
and checksum, bounds frames to 512 bytes, resynchronizes after malformed data,
and expires a partial frame after 250 ms without additional received bytes.

## Poll and status

Known 12-byte poll:

```
A5 A5 5A 5A BA C0 01 0B 00 00 01 00
```

The response is 73 bytes, operation/object `03 0B`, payload length `3D 00`.
There is no proven independent acknowledgement packet for writes. The software
checks subsequent status against the requested state.

| Status offset | Meaning | Evidence / limit |
|---|---|---|
| 12 | Power: 0 Standby, 1 On | Separate OEM Standby/On captures |
| 13 | Mode/profile byte | Captured 1; other operating modes unverified |
| 14 | Selected fan: 0 Low, 1 High | Separate OEM Low/High captures |
| 22–23 / 24–25 | Measured **EA / SA** CFM | Asymmetric supply/exhaust edits |
| 26–27 / 28–29 | Boost **EA / SA** presets | Saved readback after independent edits |
| 30–31 / 32–33 | High **EA / SA** presets | Same |
| 34–35 / 36–37 | Low **EA / SA** presets | Same |
| 41 | Boost active: 0/1 | OEM Boost on/off and resulting flow |
| 47 / 48 | Outdoor / indoor RH | Simultaneous user readings 73% / 67% |
| 49 / 50 | Outdoor / indoor temperature, °F | Simultaneous 64°F / 66°F readings |
| 52–53 | Power candidate, unsigned LE W | 3 W standby, 60–61 W High, ~90 W Boost; high byte always 0 in observations |
| 57–59 | Three fault-code bytes | Zero means no reported code; ASCII F01 seen during loss of controller comms |

Airflow fields are unsigned little-endian pairs in the observed range. The
component presents SA first in its public arrays despite raw EA-first ordering.
Raw temperatures 127 and humidities 255 appeared in Standby. Fahrenheit values
128–255 and any Celsius wire mode are unverified, so larger temperature bytes
are not published as plausible measurements.

## Own-model writes

Control/configuration writes are 59 bytes: operation/object `02 0B`, payload
length `2F 00`, header 10–11 = `01 00`. Writes carry many settings, not only the
field the user changed. A complete captured normal Low write is:

```
A5 A5 5A 5A 30 C5 02 0B 2F 00 01 00 01 01 00 00 01 FF 00 00 00 00
5A 00 5A 00 3C 00 3C 00 1E 00 1E 00 00 00 2D 00 00 05 32 02 02
03 00 00 01 02 01 00 00 FF 00 00 00 32 00 3B 01
```

| Write offset | Function |
|---|---|
| 12 | Power: 0 Standby, 1 On |
| 14 | Low 0 / High 1 |
| 17 | Captured normal command FF; config preview stages 1 Boost, 2 High, 3 Low; final save 0 |
| 18–19 / 20–21 | Configuration preview **EA / SA** values |
| 22–23 / 24–25 | Saved Boost **EA / SA** |
| 26–27 / 28–29 | Saved High **EA / SA** |
| 30–31 / 32–33 | Saved Low **EA / SA** |
| 37 and 38 | Both 0 for Boost off, both 1 for Boost on; individual meanings unverified |
| 52 | Varies 0/1/2 in configuration; exact semantics unknown |

High differs from the captured Low write at byte 14 plus checksum. Standby
changes byte 12 plus checksum. Boost on changes 37/38 plus checksum. Turning
Boost off or powering On with Low selected produced the same Low-state write.

### Preset save versus preview

Entering configuration made the controller run the current Boost preview.
Changing a field changed the live preview at 18/20 while the saved preset fields
stayed unchanged. Advancing through High and Low changed preview stage 17.
The final save had byte 17 = 0 and preview fields 18–21 = `FF FF FF FF`, with all six
new presets in 22–33. Readback then changed the saved values.

Two separate editing passes established channel assignments:

| Pass | Boost EA/SA | High EA/SA | Low EA/SA |
|---|---|---|---|
| Baseline |90/90 |60/60 |30/30 |
| Supply edits only |90/80 |60/50 |30/35 |
| Exhaust edits only |85/80 |65/50 |31/35 |
| Restored |90/90 |60/60 |30/30 |

The second save and restoration used write byte 52 = 1. The first used 0. This
component uses the captured final-save form with 1, verified in golden-frame
tests; it does not claim to understand every commissioning flag. The earlier
replay application exposed this save form for user testing, and the user reported
that basic control worked well. The ESPHome implementation itself is unflashed.

## Preserving known state and refusing unknown profiles

`replay_protocol.h` starts from a captured own-model write, preserves the current
power/speed/Boost state and all six presets unless a request intentionally
changes them, then recomputes the checksum. Before writing, it checks a set of
otherwise unchanged status fields against the captured profile. A different
mode/configuration is readable but may be **control-incompatible**.

This is deliberate: copying every status byte into a write would be wrong
because the layouts differ. Replaying an old full settings frame could also
undo new settings. The present profile guard is conservative, not a complete
model identification or a universal settings-preservation guarantee. Unknown
write fields and other ERV settings are still uncharacterized.

## Prior art and official references

The investigation consulted
[ShadowZZP's Panasonic ERV project](https://github.com/ShadowZZP/esphome-panasonic-erv)
(commit 3470015bf9aab7dbed7fae4a9c99e1c486c1b780). Its target's 9600/8N1 and 66-byte
write format differed; its Low/High/Standby/On frames did not control the tested
FV-16VEC1S. This repository's write fixtures came from our own original-controller
captures, rather than relabeling those other-model commands.

[Panasonic's model information](https://iaq.na.panasonic.com/erv/balanced-home-elite-plus-erv)
identifies the FV-16VEC1S airflow range as 30–160 CFM. The
[official wall-control manual](https://na.panasonic.ca/hubfs/PCI%20-%20Panasonic%20North%20America%20Canada/IAQ/Ventilation%20Resources%20and%20Images/ERV/English%20Resources/Installation%20Manual%20-%20ERV%20-%20Wall%20Control%20for%20BalancedHome%20Series.pdf?hsLang=en-ca)
describes the user controls and identifies F01 as wall-controller communication
error; it does not supply this UART command map.

## Work still needed

- First hardware validation of this ESPHome port, including startup, long-running
  operation and loss/recovery cases.
- Exact semantics of remaining configuration bytes and broader profile support.
- Signed/below-zero °F temperatures, Celsius-mode bytes, other sentinels and
  sensor-failure behavior.
- Full power range/width, firmware revisions and other ERV models.
- Persistence across ERV power cycles and wear implications of repeated preset
  writes. Routine CO₂ control should prefer selecting commissioned modes.
