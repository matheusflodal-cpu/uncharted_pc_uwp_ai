"""Run portable CPU/I/O regression checks; does not validate Windows or Xbox."""
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "xbox/.local/portable"


def run(*args, **kwargs):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True, **kwargs)


def main():
    if sys.platform != "linux":
        raise SystemExit("This portable check uses Linux/GCC. Build the UWP target separately on Windows.")
    compiler = os.environ.get("CC") or shutil.which("cc")
    if not compiler:
        raise SystemExit("C compiler not found")
    OUT.mkdir(parents=True, exist_ok=True)
    common = [compiler, "-std=c11", "-O2", "-g", "-Wall", "-Wextra", "-Werror",
              "-Wno-unused-function", "-pthread", "-I.", "-Isrc"]
    names = ["runtime", "runtime_content", "runtime_file", "runtime_kernel", "runtime_memory",
             "runtime_mutex", "runtime_rtc", "runtime_rwlock", "runtime_savedata", "runtime_sema",
             "runtime_services", "runtime_thread", "runtime_time", "runtime_fiber", "runtime_wait_trace", "compat_win"]
    run(*common, "-no-pie", "-rdynamic", "src/probe.c", *[f"src/{n}.c" for n in names],
        "xbox/platform/diagnostic_services.c", "-lm", "-o", OUT / "bb-probe")
    for source, name in [("tests/test_uwp_paged_io.c", "paged-io"),
                         ("tests/test_file_mods.c", "file-mods")]:
        run(*common, source, "-o", OUT / name)
        run(OUT / name)
    run(*common,"tests/test_uwp_wait_trace.c","-o",OUT/"wait-trace-contract")
    run(OUT/"wait-trace-contract")
    run(*common,"tests/test_uwp_sleep_trace.c","-o",OUT/"sleep-trace-contract")
    run(OUT/"sleep-trace-contract")
    run(*common,"tests/test_uwp_poll_profile.c","-o",OUT/"poll-profile-contract")
    run(OUT/"poll-profile-contract")
    run(*common,"tests/test_uwp_dzn_map.c","-o",OUT/"dzn-map-contract")
    run(OUT/"dzn-map-contract")
    run(*common, "tests/test_uwp_dzn_diagnostics.c", "-o", OUT / "dzn-diagnostic-contract")
    run(OUT / "dzn-diagnostic-contract")
    run(*common, "tests/test_uwp_pad.c", "-o", OUT / "pad-contract")
    run(OUT / "pad-contract")
    run(*common, "-Ixbox/platform", "tests/test_uwp_audio.c", "-lm", "-o", OUT / "audio-contract")
    run(OUT / "audio-contract")
    run(OUT / "audio-contract", "--device-failure")
    run(*common, "tests/test_uwp_dialog_trace.c", "-o", OUT / "dialog-trace-contract")
    run(OUT / "dialog-trace-contract")
    run(*common, "-DUNCHARTED_STANDALONE_MEMORY_CONTRACT", "tests/test_uwp_posix_memory.c",
        "src/runtime_memory.c", "-o", OUT / "posix-memory-contract")
    run(OUT / "posix-memory-contract")
    run(*common, "-D_GNU_SOURCE", "tests/test_content.c", "src/runtime_content.c", "-o", OUT / "content-test")
    cxx = os.environ.get("CXX") or shutil.which("c++")
    if not cxx:
        raise SystemExit("C++20 compiler not found")
    run(cxx,"-std=c++20","-O2","-Wall","-Wextra","-Werror",
        "tests/test_uwp_copy_ranges.cpp","-o",OUT/"copy-ranges-contract")
    run(OUT/"copy-ranges-contract")
    run(cxx,"-std=c++20","-O2","-Wall","-Wextra","-Werror",
        "tests/test_uwp_resolution.cpp","-o",OUT/"resolution-contract")
    run(OUT/"resolution-contract")
    sirit=ROOT / "gpu/third_party/sirit"
    sirit_sources=[sirit/"src/sirit.cpp",sirit/"src/stream.cpp",*sorted((sirit/"src/instructions").glob("*.cpp"))]
    run(cxx,"-std=c++20","-O1","-Igpu/shadps4",f"-I{sirit}/include",f"-I{sirit}/src",
        f"-I{sirit}/externals/SPIRV-Headers/include","tests/test_uwp_integer_sample.cpp",*sirit_sources,
        "-o",OUT/"integer-sample-contract")
    run(OUT/"integer-sample-contract")
    run(cxx, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
        "xbox/platform/prepare_game.cpp", "xbox/platform/prepare_contract.cpp", "-o", OUT / "prepare-contract")
    run(cxx, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-pthread", "-Ixbox/platform",
        "tests/test_uwp_boot_trace.cpp", "tests/test_uwp_boot_trace_peer.cpp", "-o", OUT / "boot-trace-contract")
    run(OUT / "boot-trace-contract")
    environment = dict(os.environ, BB_TEST_NATIVE_PREPARE=str(OUT / "prepare-contract"), BB_TEST_PROBE=str(OUT / "bb-probe"), BB_TEST_CONTENT=str(OUT / "content-test"))
    modules = ["test_probe", "test_prepare", "test_link_libc", "test_content", "test_uwp_prepare", "test_native_prepare"]
    run(sys.executable, "-m", "unittest", *modules, "-v", env=dict(environment,
        PYTHONPATH=os.pathsep.join([str(ROOT / "tests"), str(ROOT / "scripts")])) )


if __name__ == "__main__":
    main()
