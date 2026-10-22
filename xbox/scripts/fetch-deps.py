"""Download hash-pinned UWP dependencies; graphics sources are optional.

Pins are inherited from Cosmo/gow3_xbox d0975c91. Python zstandard is required
on Windows: python -m pip install zstandard==0.23.0.
"""
import argparse
import json
import zipfile
import hashlib
import io
from pathlib import Path
import shutil
import tarfile
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1] / ".local/deps"
VERSION = "14.0.0.r495.g63ebe6cdb-1"
PACKAGES = {
    "libwinpthread": "1f847f4f5d56d8f54cbc9b5ce339c36f4f208353e813d4b1e5648cf33138b9a2",
    "winpthreads": "d7d9e9dc493f2096fb441da8d6f5cb39cf90475d4b02e4808d73e3194a87ab17",
}


def extract(data, url, staging):
    if url.endswith(".zip"):
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            for item in archive.infolist():
                target = (staging / item.filename).resolve()
                if not target.is_relative_to(staging.resolve()):
                    raise RuntimeError("Unsafe ZIP path")
                if (item.external_attr >> 16) & 0o170000 == 0o120000:
                    raise RuntimeError("ZIP symlinks are unsupported")
            archive.extractall(staging)
    elif url.endswith(".zst"):
        import zstandard
        with zstandard.ZstdDecompressor().stream_reader(io.BytesIO(data)) as decoded:
            with tarfile.open(fileobj=decoded, mode="r|") as archive:
                archive.extractall(staging, filter="data")
    else:
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:*") as archive:
            archive.extractall(staging, filter="data")


def fetch(graphics=False, graphics_probe=False):
    ROOT.mkdir(parents=True, exist_ok=True)
    sources = {name: (
        f"https://repo.msys2.org/mingw/ucrt64/mingw-w64-ucrt-x86_64-{name}-{VERSION}-any.pkg.tar.zst", expected, "")
        for name, expected in PACKAGES.items()}
    if graphics or graphics_probe:
        extra = json.loads(Path(__file__).with_name("graphics-dependencies.json").read_text())
        sources.update(extra if graphics else {"vulkan-headers": extra["vulkan-headers"]})
    for name, (url, expected, license_file) in sources.items():
        destination = ROOT / name
        marker = destination / ".archive-sha256"
        if marker.is_file() and marker.read_text().strip() == expected:
            continue
        if destination.exists():
            raise RuntimeError(f"Unverified dependency directory exists: {destination}; inspect and remove it before retrying")
        request = urllib.request.Request(url, headers={"User-Agent": "Uncharted-UWP-diagnostic"})
        print(f"Fetching {name}", flush=True)
        with urllib.request.urlopen(request, timeout=120) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != expected:
            raise RuntimeError(f"SHA-256 mismatch: {name}")
        with tempfile.TemporaryDirectory(prefix=name + "-", dir=ROOT) as temporary:
            staging = Path(temporary)
            extract(data, url, staging)
            if name in PACKAGES:
                source = staging / "ucrt64"
            else:
                roots = list(staging.iterdir())
                source = roots[0] if len(roots) == 1 and roots[0].is_dir() else staging
            if not source.is_dir():
                raise RuntimeError(f"Unexpected archive layout: {name}")
            if license_file and not (source / license_file).is_file():
                raise RuntimeError(f"Missing dependency license: {name}/{license_file}")
            (source / ".archive-sha256").write_text(expected + "\n")
            shutil.move(str(source), str(destination))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--graphics", action="store_true")
    group.add_argument("--graphics-probe", action="store_true")
    args = parser.parse_args()
    fetch(args.graphics, args.graphics_probe)
