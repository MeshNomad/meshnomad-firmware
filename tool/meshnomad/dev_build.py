#!/usr/bin/env python3
"""Pick MeshNomad dev firmware and start the "Build Dev Firmware" workflow on dev-meshnomad.

A build path is ``<vendor>[/<board>[/<role>]]``, e.g. ``heltec/heltec_v4/rpt``,
``heltec/heltec_v4`` (all roles), ``heltec`` (all its boards), ``heltec/*/rpt``, ``*/*/cmp``;
several are separated by commas. Vendor and board names are those of the supported-boards
table in README.md, in lower case; ``all`` is the same as ``*``.

  dev_build.py                        pick vendors, boards and role from numbered lists
  dev_build.py heltec/heltec_v4/rpt   build these paths
  dev_build.py --resolve --target T --role R [--paths P]
                                      print the matching envs as JSON (used by the workflow)

The workflow builds what is pushed to origin/dev-meshnomad and publishes one prerelease per
role (tags dev-rpt, dev-cmp, dev-rs), replacing the previous one. Needs ``gh`` logged in to
an account with write access to the repository.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_envs import ROLES, TARGET, vendor_of  # noqa: E402

REPO = "MeshNomad/meshnomad-firmware"
WORKFLOW = "build-dev-firmware.yml"
BRANCH = "dev-meshnomad"
ENV_RE = re.compile(r"^\[env:(.+)_mn_(rpt|cmp|rs)\]$", re.M)


def catalog() -> list[tuple[str, str, str, str]]:
    """(vendor, board, role, env) for every MeshNomad env, board in lower case."""
    rows = []
    for board, role in ENV_RE.findall(TARGET.read_text()):
        vendor = vendor_of(board)
        if vendor is None:
            sys.exit(f"no vendor for {board} - add it to VENDORS in gen_envs.py")
        rows.append((vendor, board.lower(), role, f"{board}_mn_{role}"))
    return rows


def parse(path: str) -> tuple[str, str, str]:
    parts = [p.strip().lower() for p in path.strip().strip("/").split("/")]
    if not parts[0] or len(parts) > 3:
        sys.exit(f"bad path {path!r}: use vendor[/board[/role]]")
    parts += ["*"] * (3 - len(parts))
    vendor, board, role = ("*" if p in ("", "all") else p for p in parts)
    return vendor, board, role


def resolve(paths: list[str]) -> list[str]:
    rows = catalog()
    wanted: set[str] = set()
    for path in paths:
        vendor, board, role = parse(path)
        hits = {
            env
            for v, b, r, env in rows
            if vendor in ("*", v) and board in ("*", b) and role in ("*", r)
        }
        if not hits:
            vendors = sorted({v for v, *_ in rows})
            boards = sorted({f"{v}/{b}" for v, b, *_ in rows if vendor in ("*", v)})
            sys.exit(
                f"{path!r} matches no firmware.\nvendors: {' '.join(vendors)}\n"
                f"boards: {' '.join(boards)}\nroles: {' '.join(ROLES)}"
            )
        wanted |= hits
    return [env for *_, env in rows if env in wanted]


def form_paths(target: str, role: str, paths: str) -> list[str]:
    """Paths from the workflow form: the text field wins over the two lists."""
    if paths.strip():
        return [p for p in paths.split(",") if p.strip()]
    parts = (target.strip() or "all").split("/")
    parts += ["*"] * (2 - len(parts))
    return ["/".join([*parts, role or "all"])]


def pick(title: str, items: list[str]) -> list[str]:
    print(f"\n{title}")
    for i, item in enumerate(items, 1):
        print(f"  {i:2}  {item}")
    answer = input("numbers (e.g. 1 3 5), empty = all: ").replace(",", " ").split()
    if not answer:
        return items
    try:
        return [items[int(n) - 1] for n in answer]
    except (ValueError, IndexError):
        sys.exit("unknown number")


def interactive() -> list[str]:
    rows = catalog()
    vendors = pick("Vendors:", sorted({v for v, *_ in rows}))
    boards = pick("Boards:", sorted({f"{v}/{b}" for v, b, *_ in rows if v in vendors}))
    role = pick("Role (pick one, empty = all):", list(ROLES))
    roles = role if len(role) == 1 else ["*"]
    return [f"{board}/{r}" for board in boards for r in roles]


def created(stamp: str) -> float:
    return datetime.fromisoformat(stamp.replace("Z", "+00:00")).timestamp()


def gh(*args: str) -> str:
    # The repository is named explicitly: gh does not know the SSH host alias of origin
    # and would fall back to the upstream remote.
    result = subprocess.run(["gh", *args, "--repo", REPO], capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f"gh {' '.join(args)}: {result.stderr.strip()}")
    return result.stdout


def dispatch(paths: list[str], yes: bool, watch: bool) -> int:
    envs = resolve(paths)
    print(f"\n{len(envs)} firmware: {' '.join(envs)}")
    local = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True).stdout
    remote = subprocess.run(
        ["git", "rev-parse", f"origin/{BRANCH}"], capture_output=True, text=True
    ).stdout
    if local.strip() != remote.strip():
        print(f"note: the workflow builds origin/{BRANCH}, not your local HEAD")
    if not yes and input("start the build? [y/N] ").strip().lower() != "y":
        return 1
    started = time.time()
    gh("workflow", "run", WORKFLOW, "--ref", BRANCH, "-f", f"paths={','.join(paths)}")
    run_id = None
    for _ in range(30):
        time.sleep(2)
        runs = json.loads(
            gh("run", "list", "--workflow", WORKFLOW, "--branch", BRANCH, "--limit", "5",
               "--json", "databaseId,createdAt,url")
        )
        fresh = [r for r in runs if created(r["createdAt"]) >= started - 5]
        if fresh:
            run_id = str(fresh[0]["databaseId"])
            print(f"run: {fresh[0]['url']}")
            break
    if run_id is None:
        print("started; the run did not show up yet - see the Actions tab")
        return 0
    if not watch:
        return 0
    result = subprocess.run(["gh", "run", "watch", run_id, "--exit-status", "--repo", REPO])
    if result.returncode != 0:
        return result.returncode
    for role in sorted({env.rsplit("_mn_", 1)[1] for env in envs}):
        print(gh("release", "view", f"dev-{role}", "--json", "url", "--jq", ".url").strip())
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=(__doc__ or "").splitlines()[0])
    parser.add_argument("paths", nargs="*", help="build paths, e.g. heltec/heltec_v4/rpt")
    parser.add_argument("--resolve", action="store_true", help="print envs as JSON and exit")
    parser.add_argument("--target", default="all", help="--resolve: form list 'target'")
    parser.add_argument("--role", default="all", help="--resolve: form list 'role'")
    parser.add_argument("--paths", dest="form_paths", default="", help="--resolve: text field")
    parser.add_argument("--yes", action="store_true", help="do not ask before starting")
    parser.add_argument("--no-watch", action="store_true", help="do not wait for the result")
    args = parser.parse_args()
    if args.resolve:
        print(json.dumps(resolve(form_paths(args.target, args.role, args.form_paths))))
        return 0
    paths = [p for arg in args.paths for p in arg.split(",") if p.strip()]
    if not paths:
        if not sys.stdin.isatty():
            sys.exit("give build paths, or run in a terminal to pick them")
        paths = interactive()
    return dispatch(paths, args.yes, not args.no_watch)


if __name__ == "__main__":
    sys.exit(main())
