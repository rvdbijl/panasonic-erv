import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("source", ["replay_protocol_test.cpp", "controller_test.cpp"])
def test_native(source, tmp_path):
    binary = tmp_path / source.removesuffix(".cpp")
    subprocess.run(
        [
            "g++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I" + str(ROOT / "components/panasonic_erv"),
            str(ROOT / "tests" / source),
            "-o",
            str(binary),
        ],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
