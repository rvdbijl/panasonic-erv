"""Run real ESPHome validation in isolated directories; no hardware access."""

import base64
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]


def validate(tmp_path, transform=lambda s: s):
    config = (
        (ROOT / "examples/panasonic-erv.yaml")
        .read_text()
        .replace(
            "source: github://rvdbijl/panasonic-erv@v0.1.0",
            "source:\n      type: local\n      path: " + str(ROOT / "components"),
        )
    )
    (tmp_path / "test.yaml").write_text(transform(config))
    secrets = (
        (ROOT / "examples/secrets.yaml.example")
        .read_text()
        .replace(
            "REPLACE_WITH_OUTPUT_OF_openssl_rand_-base64_32",
            base64.b64encode(bytes(range(32))).decode(),
        )
    )
    (tmp_path / "secrets.yaml").write_text(secrets)
    result = subprocess.run(
        [sys.executable, "-m", "esphome", "config", str(tmp_path / "test.yaml")],
        capture_output=True,
        text=True,
    )
    return result.returncode, result.stdout + result.stderr


def test_documented_example(tmp_path):
    code, output = validate(tmp_path)
    assert code == 0, output


@pytest.mark.parametrize(
    "old,new,expected",
    [
        ("baud_rate: 4800", "baud_rate: 9600", "requires baud rate 4800"),
        ("parity: EVEN", "parity: NONE", "requires parity EVEN"),
        ("data_bits: 8", "data_bits: 7", "requires 8 data bits"),
        ("stop_bits: 1", "stop_bits: 2", "requires 1 stop bits"),
        ("poll_interval: 1s", "poll_interval: 100ms", "poll_interval"),
        ("status_timeout: 10s", "status_timeout: 1s", "at least twice"),
        ("command_timeout: 8s", "command_timeout: 1s", "at least twice"),
        ("name: Power\n", "name: Power\n      restore_mode: ALWAYS_ON\n", "DISABLED"),
        (
            "  tx_pin:\n    number: ${erv_tx_pin}\n    inverted: true\n",
            "",
            "declare a tx_pin",
        ),
    ],
)
def test_reject_invalid_config(tmp_path, old, new, expected):
    code, output = validate(tmp_path, lambda s: s.replace(old, new))
    assert code != 0, output
    assert expected in output, output


def test_configurable_pins(tmp_path):
    code, output = validate(
        tmp_path, lambda s: s.replace("GPIO16", "GPIO4").replace("GPIO17", "GPIO5")
    )
    assert code == 0, output


def test_input_only_pin_rejects_internal_pull(tmp_path):
    def change(s):
        return (
            s.replace("esp32-s3-devkitc-1", "esp32dev")
            .replace("flash_size: 16MB", "flash_size: 4MB")
            .replace("GPIO16", "GPIO34")
        )

    code, output = validate(tmp_path, change)
    assert code != 0 and "pulldown" in output.lower(), output
    code, output = validate(
        tmp_path,
        lambda s: change(s).replace("rx_pull_down: true", "rx_pull_down: false"),
    )
    assert code == 0, output


def test_hub_without_entities(tmp_path):
    code, output = validate(tmp_path, lambda s: s.split("\nselect:\n", 1)[0])
    assert code == 0, output


def test_reject_two_hubs_on_same_uart(tmp_path):
    code, output = validate(
        tmp_path,
        lambda s: (
            s.split("\npanasonic_erv:\n", 1)[0]
            + "\npanasonic_erv:\n  - id: erv\n    uart_id: erv_uart\n"
            + "  - id: other_erv\n    uart_id: erv_uart\n"
        ),
    )
    assert code != 0 and "used" in output, output


def test_two_hubs_on_separate_uarts(tmp_path):
    def change(s):
        s = s.split("\npanasonic_erv:\n", 1)[0]
        before, uart = s.split("\nuart:\n", 1)
        second = (
            uart.replace("erv_uart", "other_uart")
            .replace("${erv_rx_pin}", "GPIO4")
            .replace("${erv_tx_pin}", "GPIO5")
        )
        return (
            before
            + "\nuart:\n"
            + "\n".join("  " + line for line in ("- " + uart.lstrip()).splitlines())
            + "\n"
            + "\n".join("  " + line for line in ("- " + second.lstrip()).splitlines())
            + "\npanasonic_erv:\n  - id: erv\n    uart_id: erv_uart\n  - id: other_erv\n    uart_id: other_uart\n"
        )

    code, output = validate(tmp_path, change)
    assert code == 0, output
