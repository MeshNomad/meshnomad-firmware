#!/usr/bin/env python3
"""Remove the MeshCore board code MeshNomad does not support (boards without ESP32).

Reads the resolved PlatformIO configuration (``pio project config --json-output``) and
removes from the branch, with ``git rm``:

* every ``variants/<dir>`` none of whose envs uses the ``espressif32`` platform
* every ``boards/<name>.json`` used only by such envs

Shared platform code (``arch/``, ``src/helpers/nrf52|stm32|rp2040``, ``lib/``) and the
root ``platformio.ini`` stay as in MeshCore. Run it after every MeshCore merge: it also
settles the modify/delete conflicts such a merge produces for the removed files.

Usage: drop_unsupported.py [--check]   (--check: exit 1 if any such files are present)
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ENV_HEADER = re.compile(r"^\[env:([^\]]+)\]", re.M)


def pio_config() -> list:
    pio = os.environ.get("PIO", "pio")
    out = subprocess.run(
        [pio, "project", "config", "--json-output"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    return json.loads(out)


def is_esp32(opts: dict) -> bool:
    return "espressif32" in str(opts.get("platform", ""))


def unsupported(config: list) -> list[str]:
    """Paths (relative to ROOT) of variant dirs and board files without any ESP32 env."""
    envs = {name[4:]: dict(opts) for name, opts in config if name.startswith("env:")}
    paths: list[str] = []
    for ini in sorted((ROOT / "variants").glob("*/platformio.ini")):
        defined = [env for env in ENV_HEADER.findall(ini.read_text()) if env in envs]
        if defined and not any(is_esp32(envs[env]) for env in defined):
            paths.append(f"variants/{ini.parent.name}")
    for board in sorted((ROOT / "boards").glob("*.json")):
        users = [opts for opts in envs.values() if opts.get("board") == board.stem]
        if users and not any(is_esp32(opts) for opts in users):
            paths.append(f"boards/{board.name}")
    return paths


def main() -> int:
    parser = argparse.ArgumentParser(description=(__doc__ or "").splitlines()[0])
    parser.add_argument("--check", action="store_true", help="fail if any such files exist")
    args = parser.parse_args()
    paths = unsupported(pio_config())
    if args.check:
        if paths:
            print("\n".join(paths))
            print(f"{len(paths)} unsupported paths - run tool/meshnomad/drop_unsupported.py")
            return 1
        print("no unsupported board code")
        return 0
    if paths:
        subprocess.run(["git", "rm", "-r", "-q", "--", *paths], cwd=ROOT, check=True)
    dirs = sum(p.startswith("variants/") for p in paths)
    print(f"removed {dirs} variant dirs, {len(paths) - dirs} board files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
