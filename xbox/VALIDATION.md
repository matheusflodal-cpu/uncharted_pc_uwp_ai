# Validation of the UWP adaptation

Environment: Linux x86-64, system C compiler and Python. No game dump was used.

Command:

```sh
python xbox/scripts/verify-portable.py
```

Result: **PASS**.

- Diagnostic native loader compiled with `-Wall -Wextra -Werror`, allowing unused
  static functions inherited from the upstream runtime with `-Wno-unused-function`.
- Paged I/O test passed: multi-chunk data integrity, injected short reads,
  positioned and sequential offsets, EOF, unmapped-buffer fallback and errors.
- Guest mod-file test passed: read-only game assets and writable user files.
- **44 Python tests passed, zero failures, zero errors, zero skipped.**
  Modules: `test_probe`, `test_prepare`, `test_link_libc`, `test_content`,
  `test_uwp_prepare`, `test_native_prepare`.
- The Appx manifest parsed as XML, the Actions workflow parsed as YAML,
  and `git diff --check` passed.

Windows CI validation also **passed**:

- Windows SDK 10.0.26100.0 / clang-cl 19.1.5 compilation.
- The desktop test harness executed the same UWP FromApp memory backend: sparse commits,
  live aliases, protections, remaps, concurrent readers and actual file-backed eviction/restoration.
- 8,242 checksum checks, zero corruption. The release probe reduced private commit from
  149 MiB to 101 MiB after paging out 48 MiB; all 64 MiB byte patterns were restored.
- Package import verification against WindowsApp.lib, Store CRT and bundled dependencies.
- MakeAppx packaging and SHA-256 test-certificate signing.
- Run: https://github.com/matheusflodal-cpu/uncharted_pc_uwp_ai/actions/runs/39531180717
- Code commit: `8f5910c5327b08d8d36598273cd0fc6fd7128979`.

Explicitly **not validated**:

- Memory pressure under the Xbox's actual OS-enforced AppContainer budget.
- Uncharted's real executable initialization, game rendering, audio, input or gameplay.
- Frame rate, resolution, stability or progress through any of the three games.

The contract ran in a Windows desktop harness with a simulated 5 GiB policy budget,
not in the packaged UWP app. This validates the adapted memory mechanisms on that host;
it does not establish Xbox compatibility or game performance.

## Xbox diagnostic reported by the tester

The tester's `loader.log` reports diagnostic exit 0, executable System V code
returning 42, and 8242 memory/swap checksum checks with zero corruptions.
The swap test released 48 MiB (reported usage fell from 312 to 264 MiB).
This validates those small diagnostic workloads on that console; it does not
establish a sufficient RAM budget or playable performance for Uncharted.

## Graphics bridge milestone (0.2)

An optional `-GraphicsProbe` build enables a standalone Vulkan/Dozen color
attachment and shared D3D12/CoreWindow presentation test. It does not link the
Uncharted guest renderer. The tester subsequently supplied the successful Xbox
log summarized below. This milestone must not be described as playable.

## Xbox graphics bridge reported by the tester

The tester's 0.2 log reports `SraKmd_arden`, Vulkan 1.2, a 1280x720
shared presentation image, `CoreWindow first frame presented`, and exit 0.
Dynamic rendering and synchronization2 are available; bufferDeviceAddress and
sparseBinding are not. This validates presentation of the probe attachment,
not guest shaders, textures, game rendering, or sustained throughput.

## Renderer integration 0.3 — Windows build validated

Run: https://github.com/matheusflodal-cpu/uncharted_pc_uwp_ai/actions/runs/39538299760
Code commit: `ebf15e89a42b52f7d13d85087316dfcb49335b42`.

- clang-cl 22.1.3 compiled and linked the Uncharted renderer into the UWP host.
- Native-resolution backends replace unsupported temporal upscalers and frame generation.
- The SDL runtime is omitted; shared event declarations remain header-only.
- ImGui's desktop shell/clipboard/IME defaults are disabled.
- The desktop memory contract passed: 8242 checks, zero corruptions.
- Import verification passed against WindowsApp.lib, Store CRT and bundled DLLs.
- MakeAppx and SHA-256 signing of `UnchartedUwpProbe_0.3.0.0_x64.appx` passed.
- The portable CPU/I/O regression checks and 38 Python tests passed again.

## Xbox renderer probe 0.3 — tester log received

The tester's log reports successful detile tests at 8/16/32/64/128 bits,
three mips and two layers, with zero incorrect bytes. R8/RGBA8/BC1/BC3
copies, padded rows, layers and mips also reported zero incorrect bytes.
RGBA8/BC1 sampling with 128 SSBO slots and descriptor gaps, and storage-image
writes, reported zero bad components. `Renderer texture/shader self-test: PASS`,
CoreWindow presentation and diagnostic exit 0 were recorded.
This validates those synthetic workloads on Xbox, not actual game shaders.

## Native preparation 0.4 — portable checks

The native C++20 preparer was compiled on Linux with `-Wall -Wextra -Werror`.
Six test methods compare complete BBPROBE5/BBCONT01 files against the original
Python prepare/link/content tools using invented SELF inputs. Cases cover all
43 host library spellings, foreign-module fallback, identity version mismatch,
native exports, main/module TLS, relative/absolute/weak relocations, FS→GS,
and the libc RET flag. Plaintext ELF output also matches the equivalent SELF.
Incomplete, encrypted and malformed inputs and wrong titles are rejected.
Together with existing CPU/I/O coverage, all 44 Python tests passed.

The 0.4 UI, USB discovery and preparation have not yet run on Xbox.
Real guest initialization, game shaders, audio, input and gameplay remain
unvalidated. Compilation/signing and portable fixtures do not establish them.

## Native preparation 0.4 — Windows build validated

Run: https://github.com/matheusflodal-cpu/uncharted_pc_uwp_ai/actions/runs/39540766217
Code commit: `9bdbe239e769f30229c820ba1585b7e98adb5a28`.

- The UWP host, popup menus, native preparer and full renderer compiled and linked.
- All six native/Python parity methods passed on Windows.
- The same UWP memory backend passed 8242 checks, zero corruption, result 0.
- UWP import verification, MakeAppx and SHA-256 signing passed for 0.4.0.0.
- Linux ASan/UBSan also passed the six preparation methods. LeakSanitizer was
  disabled because this execution environment blocks its process inspection.

These are host/fixture checks; USB discovery, native preparation of a real dump
and guest boot on Xbox still require the tester's logs.

The final USB-fallback code also passed the same full Windows pipeline:
https://github.com/matheusflodal-cpu/uncharted_pc_uwp_ai/actions/runs/39541060331
Code commit: `eb5e4cda7db0ca33a0f57bc6e6b94d22af73a7fc`.
The signed 0.4.0.0 package delivered for testing is from this run.
