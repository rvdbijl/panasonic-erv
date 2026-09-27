#!/usr/bin/env python3
"""Create isolated build-only configurations; never contact or flash a device."""

import argparse
import base64
import secrets
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument(
    "--remote",
    action="store_true",
    help="test the documented GitHub source instead of local source",
)
p.add_argument("--board", choices=["s3", "esp32"], default="s3")
a = p.parse_args()
out = ROOT / "build" / (a.board + ("-remote" if a.remote else "-local"))
out.mkdir(parents=True, exist_ok=True)
s = (ROOT / "examples/panasonic-erv.yaml").read_text()
if not a.remote:
    s = s.replace(
        "source: github://rvdbijl/panasonic-erv@v0.1.0",
        "source:\n      type: local\n      path: " + str(ROOT / "components"),
    )
if a.board == "esp32":
    s = s.replace("esp32-s3-devkitc-1", "esp32dev").replace("flash_size: 16MB", "flash_size: 4MB")
# Validate the optional action as well, without adding any boot trigger.
s += "\n" + (ROOT / "examples/atomic-presets.yaml").read_text()
(out / "test.yaml").write_text(s)
(out / "secrets.yaml").write_text(
    (ROOT / "examples/secrets.yaml.example")
    .read_text()
    .replace(
        "REPLACE_WITH_OUTPUT_OF_openssl_rand_-base64_32",
        base64.b64encode(secrets.token_bytes(32)).decode(),
    )
)
print(out / "test.yaml")
