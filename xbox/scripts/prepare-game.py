"""Prepare one Uncharted executable for experimental boot diagnostics; never edits the dump.

Run on the PC with the user's decrypted Nathan Drake Collection dump. Outputs
contain game-derived code and must stay private; never commit or redistribute.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from prepare import prepare, sfo
from link_modules import link
from content_profile import prepare as content_profile

EXECUTABLES = {1: "eboot.bin", 2: "big2-ps4_Shipping.elf", 3: "big3-ps4_Shipping.elf"}


def prepare_game(dump, game, destination):
    dump = Path(dump).resolve(strict=True)
    destination = Path(destination).resolve()
    if destination == dump or dump in destination.parents:
        raise ValueError("Output must be outside the original dump")
    if destination.exists():
        raise FileExistsError(f"Output exists; select a new directory: {destination}")
    identity = sfo((dump / "sce_sys/param.sfo").read_bytes())
    if "UNCHARTED" not in str(identity.get("TITLE", "")).upper():
        raise ValueError("param.sfo does not identify an Uncharted dump")
    executable = EXECUTABLES[game]
    inputs = [executable, "sce_module/libc.prx", "sce_module/libSceFios2.prx", "sce_sys/param.sfo"]
    for name in inputs:
        file = dump / name
        if not file.is_file() or not file.stat().st_size:
            raise ValueError(f"Required input missing or empty: {name}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Publish only a complete preparation, avoiding mixed or stale output files.
    with tempfile.TemporaryDirectory(prefix="uncharted-prepare-", dir=destination.parent) as temporary:
        output = Path(temporary) / "result"
        output.mkdir()
        prepare(dump, output, executable)
        link(dump, output, exe=executable)
        content_profile(dump, output)
        metadata = {
            "format": "uncharted-uwp-diagnostic-1",
            "upstream": "Supermedo/uncharted_pc@b787c1068c3a9a9ba3ec43dd8086af2bf7d35c7d",
            "game": game, "executable": executable,
            "title_id": identity.get("TITLE_ID"), "app_version": identity.get("APP_VER"),
            "inputs": {name: hashlib.sha256((dump / name).read_bytes()).hexdigest() for name in inputs},
            "graphics": False, "gameplay_validated": False,
        }
        (output / "uwp-preparation.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        # These are the only two files consumed by the UWP loader. Other files
        # remain useful as diagnostics and have the same private status.
        if not (output / "boot-linked.bin").is_file() or not (output / "content.bin").is_file():
            raise RuntimeError("Preparation did not produce required outputs")
        shutil.move(str(output), str(destination))
    return metadata


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump", type=Path)
    parser.add_argument("--game", type=int, choices=EXECUTABLES, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    try:
        prepare_game(args.dump, args.game, args.out)
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f"Preparation failed: {error}\n")
    print(f"Prepared private diagnostic files: {args.out}")
