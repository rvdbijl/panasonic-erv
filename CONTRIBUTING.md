# Contributing

This repository is an ESPHome external component, using ESPHome's standard
`components/<domain>/__init__.py`, Python validation/code generation, platform
modules and C++ source layout. It does not need a Home Assistant integration
manifest or a PlatformIO library manifest. The public example uses a Git source
and explicitly selects `panasonic_erv`.

## Development

Use Python 3.12 and an isolated environment:

```sh
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements-dev.txt
pytest -q
python tools/prepare_examples.py --board s3
esphome config build/s3-local/test.yaml
esphome compile build/s3-local/test.yaml
python tools/prepare_examples.py --board esp32
esphome compile build/esp32-local/test.yaml
```

These are build-only commands. Never add automatic flashing, discovery or
control of a physical ERV to CI. `build/` contains generated credentials and
artifacts and is ignored. Do not publish your own secrets.yaml or a flash backup.
The build helper uses documentation placeholders plus a random build-only API
key; its output is not a deployable personal configuration.

To test the actual published source after a release:

```sh
python tools/prepare_examples.py --remote --board s3
esphome config build/s3-remote/test.yaml
esphome compile build/s3-remote/test.yaml
```

`tests/test_native.py` compiles and runs the protocol/controller tests with
warnings treated as errors. The fixtures contain real captured command/status
bytes. Preserve them as independent evidence instead of generating expected
frames with the same encoder being tested. `tests/test_config.py` runs real
ESPHome validation against valid and deliberately invalid YAML configurations.

The controller layer has no ESPHome dependency and tests fragmentation,
checksum rejection, queue ordering, preserved edits, stale data, timeouts,
transport failure and unsigned-millisecond rollover. The ESPHome adapter
publishes readback and sensor availability. Compile checks cover the adapter,
all entity platforms and the atomic action.

## Style and drawings

Run `ruff format components tests tools` and `clang-format -i` on C++ sources
using the repository style. Check Python with `ruff check components tests tools`. Run tests after substantive changes. Generate the
schematic with `python tools/draw_wiring.py`; inspect both its labels and wiring
connections in the resulting PDF/SVG before committing a change.

## Protocol changes / another model

A new model needs its own measured voltage/pinout and labeled OEM captures.
Include the whole poll/status/write transaction, serial settings, displayed
state, separate supply/exhaust edits, checksum evidence and response over time.
Do not infer support from a model name, connector or shared header alone.

Keep model-specific encoders/profile checks distinct. Broadening a guard without
mapping the corresponding write fields can silently overwrite settings. Explain
unknown values and sentinel handling; do not present guesses as decoded sensors.
New runtime paths need tests for rejection/failure as well as successful frames.

A useful report includes ERV/controller model and firmware, ESP board, ESPHome
version, electrical measurements, redacted YAML and a short reproducible action.
Avoid uploading network secrets or unrelated device logs.

## Release checklist

1. Run native/configuration tests and both offline builds on the pinned ESPHome
   version. Test a minimal optional-entity configuration if adapter dependencies
   change.
2. Check docs option/entity names against Python schemas and runtime behavior.
3. Render/inspect the schematic and check links.
4. Commit reviewed files; tag a release. Keep existing release tags immutable.
5. Validate and compile the documented GitHub source from a clean external-
   component cache. Record the exact supported/tested versions.
6. Update the example ref for the next release deliberately. Never imply a
   compile test establishes electrical or on-device behavior.
