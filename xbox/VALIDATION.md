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

## Xbox native preparation 0.4 — tester report

The supplied Uncharted 3 log confirmed USB E:, plaintext dump discovery,
native preparation PASS (boot=33,391,052 bytes, content=28 bytes), and diagnostic
exit 20 with 124/552 imports unresolved. The tester also reports exit 20 for
games 1 and 2; only the game 3 log was supplied. No guest code was executed.

## Graphics registration fix 0.4.1

All 65 graphics NIDs in that report exist in GnmDriver/VideoOut registration
tables. Previously these tables were populated by `bbgpu_init`, after the
`--list-missing` return. Address registration is now independent of GPU/window
initialization on UWP. Desktop RegisterLib retains initialization then registration.

A Windows contract links the real renderer and CPU resolver and checks those
65 imports without a CoreWindow, presenter or device. Unknown/data imports and
unimplemented guest audio/input must remain unresolved. Windows build validation is recorded below; the next Xbox diagnostic is pending.

### Windows pipeline passed

Run: https://github.com/matheusflodal-cpu/uncharted_pc_uwp_ai/actions/runs/39626279682
Code: `67456251d507f3bbb87c949c01ed9a3f29fced66`.

- `Graphics import contract: checked=65 missing=0 window=none`.
- All six native preparation/Python parity methods passed on Windows.
- Memory backend: 8242 checks, zero corruption, result 0.
- Active package imports verified as UWP APIs, Store CRT or bundled dependencies.
- MakeAppx and SHA-256 signing of the 0.4.1.0 APPX passed.
- Linux portable CPU/I/O coverage and all 44 Python tests passed.

The expected reduction to 59 missing imports is specific to the supplied
Uncharted 3 executable; it remains to be confirmed on Xbox. This correction
does not implement audio/input/network APIs or validate graphical guest calls.

## Xbox confirmation of 0.4.1

The next Uncharted 3 log confirmed native preparation PASS and
`59 of 552 imports missing`, result 20. Graphics address registration appeared
before GPU initialization; the 65 previously misreported graphics entries disappeared.
No game execution occurred in this imports test.

## Native pad and POSIX memory 0.5 — portable checks

- 187 pad checks passed, including System V function-pointer calls, exact guest
  structure layouts, all 14 button mappings, analog ranges/Y inversion, trigger
  values, count/handle errors, disconnected output, hotplug/generation changes,
  overlay neutralization and vibration routing. These use a mock host input source.
- 32,786 anonymous-memory checks passed: guest mmap zero fill, rounding, msync,
  munmap, reuse and errno for invalid/unsupported mappings. File mappings remain unsupported.
- The existing paged I/O/file tests and 44 Python tests passed.

The app now links a WinRT pad backend plus a 20-second physical input probe.
Windows checks are recorded below; Xbox hardware input and game boot remain pending.
Audio, network, motion/touch sensors and Uncharted-specific SDL pad callbacks remain unavailable.

### Final 0.5 Windows package validated

Run: https://github.com/matheusflodal-cpu/uncharted_pc_uwp_ai/actions/runs/39631311445
Code: `e896471a4a48785ce03258ccd3d812bd43fd68fb`.

- Native input C/C++ and UWP host compiled and linked.
- Pad contract: 187 checks passed with simulated input, including System V calls.
- POSIX memory contract: 32,786 checks passed against the UWP FromApp backend.
- Memory/swap contract: 8242 checks, zero corruption, result 0.
- Graphics contract: 65 imports, zero missing, without a window/GPU initialization.
- All six native preparation/Python parity methods passed on Windows.
- Active package imports verified; MakeAppx and SHA-256 signing passed for 0.5.0.0.
- Linux ASan/UBSan passed the pad contract (LeakSanitizer disabled for the execution
  environment); UBSan passed the memory contract after guarding empty VMA erasure.
- FreeBSD stable/9 mman.h confirms guest MS_SYNC=0, MS_ASYNC=1, MS_INVALIDATE=2.

This validates the build and tested contracts, not the WinRT controller on Xbox
or real Uncharted boot/gameplay. The 20-second physical probe and game boot logs
are the next console checks. Audio and other listed guest APIs remain pending.

## Xbox v0.5 physical input and attempted boot (2026-10-21)

The tester's dedicated controller log reports PASS: 949 connected samples,
zero errors, face buttons observed, both stick axes reaching 0..255 and both
triggers reaching 255. The tester separately confirmed feeling the vibration.
This validates the dedicated input path, not input during gameplay.

The subsequent Uncharted 3 boot prepared big3-ps4_Shipping.elf successfully
(boot=33391052, content=28), detected Dozen on SraKmd_arden, and constructed
the 1920x1080 CoreWindow bridge. It then reported an execute fault at address
zero during GPU initialization, before the loader's GPU-ready message or
guest execution. No out-of-memory failure was reported.

## v0.5.1 overlay startup correction

Source inspection found that BbOverlay::Init ignored the return value of
ImGui_ImplVulkan_LoadFunctions. The bundled backend requires desktop WSI
entry points even when used with externally managed images. A missing entry
ends loading early, leaving later function pointers null; release assertions
are disabled. ImGui_ImplVulkan_Init would then use that incomplete table.
This is a concrete crash path consistent with the Xbox log; the original log
does not identify the exact missing function or prove this was the only cause.

The corrected caller checks the actual backend loader, including its dynamic
rendering lookups and core-to-KHR fallback, before backend initialization.
Missing functions disable only the overlay with a named diagnostic. Failed
overlay initialization cannot capture game input. Additional presenter and
driver milestones distinguish post-processing, overlay, and VideoOut startup.
Null-call crash reports retain the return-address candidate from the stack,
and CI archives a matching linker map for host symbol lookup.

The regression harness compiles the actual bundled ImGui Vulkan backend with
NDEBUG and injected missing entry points. It covers early/late table failures,
WSI, missing dynamic-rendering functions, valid Vulkan 1.2 KHR fallback, core
1.3 functions, and failure after a previous complete load. It requires partial
loads to return without initializing backend state or calling any GPU function.
Local validation passed: 31 overlay loader checks, 187 pad checks, 32786
POSIX memory checks, the existing C I/O checks, and 44 Python tests.
Windows CI passed for code 34ce4407fce52e1579f1a914d29750c354a7d253:
https://github.com/matheusflodal-cpu/uncharted_pc_uwp_ai/actions/runs/39635271859

- Full renderer and UWP application compiled and linked.
- Actual ImGui backend loader regression: 31 checks, no GPU calls.
- Native pad: 187 checks; POSIX memory: 32786 checks.
- Native preparation: six parity tests; headless graphics import contract passed.
- Swap: 8242 checks, zero corruption.
- UWP import allowlist passed; MakeAppx and signing succeeded for 0.5.1.0.
- Xbox boot confirmation remains pending; no gameplay claim is made.
