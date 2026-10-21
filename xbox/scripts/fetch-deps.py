"""Download only the pinned UCRT winpthreads packages for the CPU milestone.

Pins are inherited from Cosmo/gow3_xbox d0975c91. Python zstandard is required
on Windows: python -m pip install zstandard==0.23.0.
"""
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


def fetch():
    import zstandard
    ROOT.mkdir(parents=True, exist_ok=True)
    for name, expected in PACKAGES.items():
        destination = ROOT / name
        marker = destination / ".archive-sha256"
        if marker.is_file() and marker.read_text().strip() == expected:
            continue
        if destination.exists():
            raise RuntimeError(f"Unverified dependency directory exists: {destination}; inspect and remove it before retrying")
        url = f"https://repo.msys2.org/mingw/ucrt64/mingw-w64-ucrt-x86_64-{name}-{VERSION}-any.pkg.tar.zst"
        request = urllib.request.Request(url, headers={"User-Agent": "Uncharted-UWP-diagnostic"})
        print(f"Fetching {name}", flush=True)
        with urllib.request.urlopen(request, timeout=120) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != expected:
            raise RuntimeError(f"SHA-256 mismatch: {name}")
        with tempfile.TemporaryDirectory(prefix=name + "-", dir=ROOT) as temporary:
            staging = Path(temporary)
            with zstandard.ZstdDecompressor().stream_reader(io.BytesIO(data)) as decoded:
                with tarfile.open(fileobj=decoded, mode="r|") as archive:
                    archive.extractall(staging, filter="data")
            source = staging / "ucrt64"
            if not source.is_dir():
                raise RuntimeError(f"Unexpected archive layout: {name}")
            (source / ".archive-sha256").write_text(expected + "\n")
            shutil.move(str(source), str(destination))


if __name__ == "__main__":
    fetch()
