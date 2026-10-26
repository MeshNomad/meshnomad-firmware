#!/usr/bin/env python3
"""Release-notes line: which boards changed their partition table since the previous release.

Compares the partition table (bytes 0x8000-0x8C00 of every ``<env>-<version>-merged.bin``)
of a new release with the previous release of the same role (tag prefix ``rpt-``, ``cmp-``,
``rs-``). A board whose table changed cannot be updated with the app image at 0x10000; it
needs the ``-merged.bin`` at 0x0 (full reset). Prints Markdown for the release notes.

Usage: partition_changes.py <tag> <dir with the new .bin files>
Needs ``gh`` with read access to the releases (``GH_TOKEN``); the repository is taken from
``GITHUB_REPOSITORY`` when set.
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

TABLE = slice(0x8000, 0x8C00)
TAG_RE = re.compile(r"^(rpt|cmp|rs)-v(\d+)\.(\d+)\.(\d+)-meshnomad\.(\d+)$")
MERGED_RE = re.compile(r"^(.+?)-v\d.*-merged\.bin$")


def gh(*args: str) -> str:
    repo = os.environ.get("GITHUB_REPOSITORY")
    extra = ["--repo", repo] if repo else []
    return subprocess.run(["gh", *args, *extra], check=True, capture_output=True, text=True).stdout


def previous_tag(tag: str) -> str | None:
    match = TAG_RE.match(tag)
    if not match:
        sys.exit(f"not a release tag: {tag}")
    role, current = match.group(1), tuple(int(n) for n in match.groups()[1:])
    older = []
    for name in gh("release", "list", "--limit", "200", "--json", "tagName",
                   "--jq", ".[].tagName").split():
        m = TAG_RE.match(name)
        if m and m.group(1) == role:
            key = tuple(int(n) for n in m.groups()[1:])
            if key < current:
                older.append((key, name))
    return max(older)[1] if older else None


def tables(folder: Path) -> dict[str, bytes]:
    found = {}
    for path in folder.glob("*-merged.bin"):
        match = MERGED_RE.match(path.name)
        if match:
            found[match.group(1)] = path.read_bytes()[TABLE]
    return found


def main() -> int:
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    tag, new_dir = sys.argv[1], Path(sys.argv[2])
    prev = previous_tag(tag)
    if prev is None:
        print("First release of this role: no earlier partition table to compare with.")
        return 0
    with tempfile.TemporaryDirectory() as tmp:
        gh("release", "download", prev, "--pattern", "*-merged.bin", "--dir", tmp)
        old = tables(Path(tmp))
    new = tables(new_dir)
    changed = sorted(env for env in new if env in old and new[env] != old[env])
    if changed:
        envs = ", ".join(f"`{env}`" for env in changed)
        print(f"**Full reset required** for {envs}: the partition table changed since {prev}. "
              "Over OTA the larger image is refused (`start ota` fails during the upload, "
              "`net ota` refuses it before the upload with \"image larger than the OTA slot\"); "
              "the `.bin` flashed over USB at 0x10000 will not start. "
              "Flash the `-merged.bin` file over USB at 0x0; the node starts with a new identity "
              "and default settings.")
    else:
        print(f"Partition tables unchanged since {prev}: the `.bin` file at 0x10000 updates "
              "every board and keeps its settings.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
