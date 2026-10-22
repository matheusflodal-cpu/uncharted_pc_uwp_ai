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

## Xbox v0.5.1: original Uncharted 3 code executed

Tester logs confirm the unavailable overlay entry was vkDestroySurfaceKHR.
Disabling the overlay allowed presenter/GnmDriver/VideoOut initialization,
then linked module initializers and the original guest entry at offset 0x4897a0.
The game created 124 threads, opened 112 files and read 8,943,786 bytes.
It stopped explicitly at sceAudioOutInit (JfEPXVxhFqA#G#H), caller 0x684953.
The bridge presented its first frame; this does not establish game imagery.
Ten missing path lookups were recorded, but numerous other reads succeeded
under the same app0 mount. No evidence currently establishes that all ten
paths are required or that the dump root is wrong. No game data is available
to inspect those files here; no fallback paths or fabricated assets are added.

## v0.6 audio integration

The UWP app links real AudioOut via XAudio2 and the upstream Uncharted AJM
ATRAC9/MP3 decoder runtime. Basic CPU and graphics resolver harnesses retain
their diagnostic audio stubs. Native audio device and submission failures
propagate explicit errors; only explicit BB_AUDIO=none uses a timer sink.
Audio conversion scratch storage is per port, outside the guest thread stack;
port locks persist through close/reopen and protect lookup-to-use transitions.

Local AudioOut contract passed 100 checks for ABI, all eight PCM formats,
channel ordering, stereo folding, atomic volume validation, buffer cadence,
close/reopen and submission errors. A separate device-init failure check
passed without silently marking the guest API initialized. Existing portable
checks (187 pad, 32786 POSIX memory and 44 Python tests) passed.
The Windows AJM contract uses a generated MP3 tone for actual decoding and
a known stereo 48 kHz configuration for ATRAC9 initialization. It also checks
batch parsing, malformed chunks and context ownership. Windows build/decoder
checks and actual Xbox sound/boot remain pending until recorded below.

The local AJM harness additionally passed 4648 checks using the system FFmpeg
6.1.1 libraries: real MP3 PCM output, ATRAC9 initialization, batches and errors.
ASan/UBSan initially exposed BitReverse32 shifting by 32 for a zero-width
operation in the pinned LibAtrac9 utility. The UWP build substitutes a small
MIT-licensed utility adaptation with defined zero-width reversal and unsigned
bit arithmetic for sign extension; the original submodule pin is preserved.
MP3 input is copied into an FFmpeg-owned zero-padded packet rather than
assuming padding after guest memory. After those fixes the AJM sanitizer
run passed with UBSAN_OPTIONS=halt_on_error=1 and no reported errors.
The AudioOut sanitizer run passed its 100 checks as well. Leak detection
was disabled for the local ASan runs under this execution environment.

## v0.6 Windows CI passed

Code: ec7a50af6121f5f61e3ede7dd3ab40ac97d9df3f
Run: https://github.com/matheusflodal-cpu/uncharted_pc_uwp_ai/actions/runs/39640377783

- Full renderer and app compiled and linked with native XAudio2 and AJM.
- AudioOut: 100 checks and explicit device-failure case passed.
- AJM: 4648 checks passed with the pinned FFmpeg 7.1.1 build; real MP3 PCM
  output and ATRAC9 initialization confirmed in the Windows harness.
- Pad: 187 checks; overlay loader: 31 checks; graphics resolver: 65 imports.
- Native preparation parity: six tests; swap: 8242 checks, zero corruption;
  POSIX memory: 32786 checks.
- UWP import allowlist, MakeAppx and APPX 0.6.0.0 signing passed.

Physical audio output, actual game audio decoding and this revision's guest
boot remain unverified until the tester provides new Xbox logs.

## Xbox v0.6: audible probe and black-screen boot

The tester confirmed hearing the dedicated tone. loader (5).log reports
XAudio2 at 48 kHz stereo, 375 buffers, one port and diagnostic exit 0.
The subsequent loader (6).log opened three AudioOut ports (main eight-channel
float folded to stereo, voice mono and pad mono), initialized AJM, continued
startup, invoked MsgDialog and reached guest shader/pipeline compilation.
The two errors logs supplied were byte-identical. Both ended at
`STOP: diagnostic phase watchdog`, with no named unsupported import or
access fault. No shader/pipeline completion timings were available, so the
90-second termination does not establish a driver hang or slow compilation.
No game image, menu or gameplay was confirmed.

## v0.6.1 timed boot diagnostics

Boot now has a fixed 300-second limit after preparation; imports retain 90 s.
RAII scopes record begins/returns, elapsed duration and C++ exception exits
for shader translation/SPIR-V, graphics/compute pipeline creation, actual
driver compilation calls, recording synchronization, copy waits, submissions,
timeline queries/waits, draw-pipe drains and the CoreWindow bridge.
Begin/return lines are limited to the first eight of each kind, plus operations
lasting at least one second or ending by exception. Heartbeats every five
seconds print aggregate counters, bounded pending scopes, cached timeline
values and CPU/app-memory samples. Counts indicate calls/returns, not rendered
images; cached timeline values are not fresh driver queries. Concurrent
snapshots are best-effort and never acquire renderer or driver locks.

MsgDialog diagnostics inspect only the recognized 136-byte PS4 parameter
layout with a 48-byte base (checked against shadPS4 system/msgdialog_ui.h).
ReadProcessMemory protects every guest read; strings are bounded to 1024
bytes and control characters are normalized. Unknown layouts/unreadable
pointers are reported without following them. Guest dialog completion and
result semantics remain unchanged; this adds observation, not dialog UI.

Local contracts passed: 17 trace checks covering cross-translation-unit
state, eight workers, scope cleanup, exceptions, bounded-slot saturation and
snapshots; 21 dialog checks covering modes, layouts, bad pointers and bounded
strings. Existing portable checks and 44 Python tests passed. Windows build
and new Xbox trace remain pending until recorded below.

## v0.6.1 Windows CI passed

Code: ee73515a4ac3adf188174686b08641e51fd5b891
Run: https://github.com/matheusflodal-cpu/uncharted_pc_uwp_ai/actions/runs/39645368261

The full renderer and app compiled and linked. The new boot trace (17 checks)
and protected dialog parsing (21 checks) passed on Windows, alongside pad
(187), overlay loader (31), AudioOut (100 plus device failure), AJM (4648),
native preparation parity (six tests), graphics import resolution, swap
(8242 checks, zero corruption) and POSIX memory (32786 checks).
The UWP import allowlist passed and APPX 0.6.1.0 was signed successfully.
The separate CPU diagnostic and graphics build workflows also succeeded.

The downloaded renderer artifact SHA-256 matches the GitHub Actions digest:
2c086f086eeb569f9662942438a958552bdf65b61575602bf8fba3b23c634810.
These are host build/contract results. The five-minute boot and diagnostic
output still require a new Xbox attempt; they do not establish a fix for
the black screen, an actual game image, menu or gameplay.

## Xbox v0.6.1: stable command counts and a controller alert

Attachments: loader.log (3).txt and errors.log (1).txt. The boot reached the
300-second diagnostic watchdog. At 5.114 s, draws=9, dispatches=4 and
flip_requests=0; these remained unchanged through 300.102 s. Every measured
scope had returned, with no pending measured operation. Memory settled at
2411/5120 MiB (first guest heartbeat 2513 MiB), so the samples do not suggest
budget exhaustion. Process CPU increased about 25 s per 5 s wall time; this
is aggregate CPU time, not a per-thread attribution or proof of a spin loop.
Six pipelines were preloaded, which explains why shader compilation scopes
need not appear in this attempt. The final GPU message was creation of the
Thin2DThin_64 detiler. That path called createComputePipelineUnique directly
and was not covered by the previous driver compilation scopes. A blocked
call there remains a hypothesis, not an established cause.

MsgDialog logged: Wireless controller not detected. Please check connection
and battery levels for the wireless controller. No pad service call history
was present. The physically validated standalone input probe does not prove
that the game's own opening/read sequence succeeded. No menu, game image
or gameplay was confirmed.

## v0.6.2: cover remaining recording/wait blind spots

Adds UWP-only scopes around the tiling pipeline path and its actual driver
creation call, plus DrawRecord/DispatchRecord. PM4 telemetry records submitted,
resumed/completed coroutine tasks, packets and last opcode/address per virtual
queue. Wait records cover graphics/compute memory or register comparisons,
video labels, memory semaphores and rewind-valid waits. The comparison,
scheduling, fence signalling and draw behavior are unchanged. Observations
are bounded atomic samples; no monitor-side guest reads, queue locks or GPU
queries. Command-thread wait sampling is rate-limited and may be stale while
a task is suspended. Snapshots are best-effort, not coherent transactions.

Native pad diagnostics retain real host results, report initial opening/info
calls and bounded read errors, and publish counters via trylock: the watchdog
skips a busy service rather than waiting for it. No forced connection,
repeated test requirement or timeout increase was introduced. The new build
and Xbox behavior require validation recorded below.

Local v0.6.2 contracts passed: 29 boot-trace checks (including independent
queue states, disabled/out-of-range safety, bounded reader calls and a real
coroutine suspended with a live wait), 193 pad checks (including non-blocking
snapshot of a busy service and host error counters), and the existing audio,
dialog, POSIX memory and 44 Python tests.

## v0.6.2 Windows CI passed

Code: fa9e3bc93d898178ba965cae8b43e4df0fefe06e
Run: https://github.com/matheusflodal-cpu/uncharted_pc_uwp_ai/actions/runs/39674690818

The full renderer/app compiled and linked. Passed on Windows: boot trace
29 checks, pad 193, overlay 31, protected dialogs 21, AudioOut 100 plus
device failure, AJM 4648 with actual MP3 decoding and ATRAC9 initialization,
native preparation parity six tests, graphics import resolution, swap
8242 checks with zero corruption, and POSIX memory 32786 checks. UWP import
validation and signing of APPX 0.6.2.0 succeeded. Separate CPU and graphics
build workflows also succeeded.

These results validate the build and host contracts. The added recording,
tiling, PM4 and controller traces need the next Xbox boot attempt; no fix
for the black screen or playable game has been established.

## Xbox 0.6.2 and the 0.6.3 memory correction

loader.log (4).txt and errors.log (2).txt: the detiler driver call returned
in 42 ms, all 8 measured tiling calls and 9 DrawRecord calls returned. The
fourth DispatchRecord started at 4417 ms and remained pending through
300105 ms (295688 ms pending). Queue 0 remained at 92 submitted tasks,
41 resumes, 40 completions and 752 packets, last opcode 0x15. No measured
PM4 wait was pending. That locates a block within DispatchRecord, not its
exact operation. Input opened successfully for user 1; its only host sample
reported absent, then no guest reads followed. Memory settled at 2420/5120 MiB.

Code inspection found a concrete reentrancy hazard in uwp_backing_write:
memcpy could read a guest source while holding uwp_commit_lock; a lazy/stale
source page fault calls a handler which takes that same exclusive SRW lock.
0.6.3 stages each source chunk into bounded host storage before locking the
destination. Small fences use 256 stack bytes; larger copies allocate one
16 KiB host buffer, avoiding a large guest-stack array. The destination
protection restore/alias/swap paths remain in the existing backend. This
corrects a real deadlock risk; the Xbox trace has not yet proven it was hit.
New Windows regression cases use untouched guest source pages across a
boundary, stale host PAGE_NOACCESS with logical RW permissions and a read-only
destination. Existing memory/swap checks still run. Added scopes measure
HLE shader dispatch, image-to-buffer synchronization, backing writes, resource
binding and pending scheduler operations.

Before guest boot, the real Gamepad API is polled for up to five seconds
(50 ms interval); absence or API failure stops with an explanatory message.
This does not fabricate a pad or modify guest pad ABI. A host call stuck
during this step is bounded by the separate 15-second phase watchdog.
Windows build, regression and Xbox validation are recorded below when done.

## v0.6.3 Windows CI passed

Code: 062a5820bc09258f57583eee5c05343e6d354f78
Run: https://github.com/matheusflodal-cpu/uncharted_pc_uwp_ai/actions/runs/39680411552

Full renderer/app compiled and linked; APPX 0.6.3.0 signed. The real FromApp
memory harness passed the new guest-source lazy/stale-protection copy
regression, including byte patterns and read-only destination restoration.
Swap: 8242 checks, zero corruption; POSIX memory: 32786 checks. Also passed:
pad 193, boot trace 29, protected dialogs 21, overlay 31, AudioOut 100 plus
device failure, AJM 4648 with actual MP3 decoding/ATRAC9 initialization, six
native preparation parity tests, graphics resolution and UWP import allowlist.
The separate CPU and graphics workflows also passed. The shared input header
was additionally syntax-checked with the Windows RPC small macro defined.

This validates the memory correction and package on Windows. Real controller
readiness and the effect on the blocked Xbox DispatchRecord remain unverified
until a new console boot. No game image, menu or gameplay is established.

## Xbox 0.6.3: prior block passed; new graphics-pipeline failure

loader (8).log: real input ready in 94 ms, guest pad info connected=1 and
one successful read. The fourth DispatchRecord (HLE 0x6a6422a9) returned in
36 ms; its 9 MiB backing write returned in 34 ms. Later work continued. At
5.024 s the snapshot showed 11 draws, 9 dispatch calls and zero flip requests.
Memory was 2553/5120 MiB. The final pipeline creation started at 5.963 s.
errors (4).log and errors (5).log are byte-identical: pipeline key
0xf43a301791304559, FS 0x81a0a982 / VS 0xf585649d, then
ErrorOutOfHostMemory and a library assertion. No game image was confirmed.

The pinned Dozen dzn_pipeline.c maps nir_to_dxil failure, DXIL validation
failure and failed D3D12 CreatePipelineState to that same Vulkan result.
There is no native HRESULT or validation detail in the attached logs. The
last memory sample is not an allocation peak and cannot distinguish causes.

## v0.6.4 driver error channel

A strict, hash-checked build-time patch to the same Mesa pin adds an MIT
callback sink in dzn_pipeline.c. It reports NIR-to-DXIL failures, DXIL validator
text, pipeline host-allocation failure, native HRESULT and device-removed
reason for failed graphics CreatePipelineState, plus final graphics VkResult.
All original validation, return values and cleanup remain intact. It does
not enable experimental mode, change shaders/features or retry failed work.
The host registers the callback before Vulkan initialization; this bypasses
the separate static-CRT stderr of Mesa. Shader compiler worker threads may
call it concurrently. Text is bounded and forwarded directly to errors.log.

The driver cache key includes both patch and sink hashes. A missing callback
entry point is an explicit startup error in the full renderer package. The
basic graphics probe still accepts its older uninstrumented driver.
The app records up to 32 pre-call pipeline states and every failure state,
including shader hashes, formats, blend/input/raster state and app memory
usage/limit. Memory values are samples around the call, not exact peaks; a
failed WinRT query is reported as unavailable. No Vulkan queries are made
from the diagnostic callback.

Local tests cover exact source-pin/context checks, rejection of changed or
already patched source, and native-code formatting, long-message bounds,
disabled callback and concurrent sink calls. Windows driver rebuild and
Xbox results are pending until recorded below.


## v0.6.4 Windows build and package

Code dc095368146f01854645c01678b438e9fefea7fc; Actions run
39683566279. The pinned Mesa source was rebuilt with MSVC 14.44 after
passing the strict patch contract. The resulting driver exports
`dznSetUwpDiagnosticCallback`; its DLL SHA256 is
45d284f8a08df1d5063ec95e755a2b63cf9c43d9f813561c90db2051dae79437.

The full Windows renderer compiled and linked. The native diagnostic sink
passed 26 checks, pad 193, overlay 31, boot trace 29, dialog 21, AudioOut
100 plus device failure, and AJM 4648 with actual MP3 decode/ATRAC9 init.
Native preparation passed against the original Python tools. Memory tests
passed guest-source page-fault regression, 8242 swap checks with zero
corruption, and 32786 POSIX checks. Active UWP imports passed the allowlist;
the APPX 0.6.4.0 was signed. Separate CPU and basic graphics builds passed.
These checks validate Windows builds/contracts, not an Xbox game boot.

The new driver error channel and the result of the next game boot still
require Xbox testing. No menu, game image or gameplay is confirmed.


## Xbox 0.6.4 and bounded integer sample lowering in 0.6.5

errors (6).log identifies DXIL validation rejection of sample.i32 in the
fragment shader stage, same FS 0x81a0a982 / pipeline 0xf43a301791304559.
Memory immediately before/after failure: 3065798656 / 3066449920 bytes,
limit 5368709120 bytes. This failure is shader validation, not demonstrated
RAM exhaustion. loader (9).log confirms the driver diagnostic callback.

0.6.5 adds UWP-only integer sampling lowering for one-level point-filtered
1D/2D/3D and 1D/2D-array views, excluding cube, anisotropy, custom borders
and non-point filtering. Implicit/explicit/gradient samples share the same
fetch since the sole view level is zero. Depth compare/gather and other
cases retain the original shader/validation. Unknown cases log actual
shader/image/sampler/view parameters and remain explicit failures.

Coordinates preserve normalized addressing (the existing IR pass normalizes
GCN unnormalized samples), floor before offsets, repeat and mirrored repeat,
mirror-once, edge and predefined borders. Array layers round and clamp.
Texel data stays signed/unsigned integer until the existing bitcast to IR
float registers. No sample is replaced with a constant or omitted.

The sampler raw words and integer view level count enter specialization;
shader binary/meta and pipeline key versions are incremented. This prevents
reusing a compiled one-level/wrap configuration after changing descriptors.

Local numerical cases exercise the production coordinate template. Sirit
contracts generate 80 signed/unsigned modules across dimensions/arrays/wrap
modes and assert integer fetch replaces sampling. The Windows ICD contract
first requires the old shader to reproduce a DXIL rejection, then requires
all 80 lowered fragment/graphics pipelines to compile through the exact packaged Dozen. This
checks compiler acceptance, not GPU execution or rendered pixels. Windows
and Xbox results remain pending until recorded below.


The initial Windows compile at 5ae1a00 succeeded, including 6683 local
emitter checks, but its compute-stage negative control did not reproduce
the fragment-stage rejection. The driver contract now uses a real vertex /
fragment graphics pipeline, implicit integer sampling and FragCoord input;
the negative control must reject before all lowered graphics variants run.
Tests also include signed constant offsets and predefined border swizzles,
including constant-zero/one channels. Border swizzle enters specialization.


Desktop WARP compiled the original integer sample at ps_6_8 and accepted all
80 lowered graphics variants; it did not reproduce the console rejection.
The host contract now explicitly preloads the packaged DXIL/DXC and uses a
new diagnostic-only setter to compile at SM 6.0. Both NIR and DXIL compiler
model caps are changed together by this test switch. Its default is zero;
the app never calls it and normal adapter model selection is preserved.
This tests an older compiler target, not a claimed match for every Xbox
capability. Validation remains enabled. The isolated compiler check runs
before the full renderer build, so shader test failures return sooner.


## v0.6.5 Windows validation

Code 965f526f5146e3122881b1ff02237c58829cd1dd; Actions run
39689740418, renderer job 119534171463.

The isolated graphics compiler contract preloaded packaged DXIL/DXC,
registered the diagnostic sink and selected the host-only SM 6.0 target.
The original implicit integer fragment sample was rejected by DXIL;
all 80 lowered signed/unsigned graphics pipelines compiled. Variants cover
1D/2D/3D, 1D/2D arrays, eight addressing modes, predefined borders and
signed constant offsets. No lowered pipeline produced a validator failure.
This reproduces the rejection class with a legacy target; it is not a
claim that the software adapter matches all Xbox capabilities.

Full renderer compile/link passed. The final emitter contract passed 6705
checks and the diagnostic sink 29. Native preparation passed six tests;
pad 193, overlay 31, boot trace 29, dialog 21, AudioOut 100 plus device
failure, AJM 4648 with actual MP3 decode and ATRAC9 init all passed.
Memory passed the guest-source page-fault regression, 8242 swap checks with
zero corruption and 32786 POSIX checks. UWP imports passed the allowlist;
APPX 0.6.5.0 was signed. Separate CPU and basic graphics builds passed.

Dozen was rebuilt from the same pinned source with the strict diagnostic
patch. DLL SHA256:
d48984f8315fd2343ecc6e7268bed0be9d49e81f0dab4d97cb7e214568ce7b3b.
Both diagnostic exports are present. The shader-model setter defaults off;
the app never calls it and uses the actual adapter model. Validation remains
active on normal and diagnostic paths.

This checks compiler acceptance and scalar addressing, not rendered pixels
or an Xbox game boot. The real FS texture/sampler parameters remain unknown
until the next console test; eligibility is explicitly logged. No game
image, menu or gameplay is established.


## Xbox 0.6.5: first game logos confirmed

Tester explicitly reports the logos, without reaching the menu. errors (7).log
shows the integer sample lowering for FS 0x81a0a982 and subsequent pipelines.
loader (10).log at 35.391 s: 7670 draw requests, 4502 dispatch requests, 741
flip requests, 743 bridge_present calls returned. Counts do not establish
distinct visible frames. Last memory sample 4244/5120 MiB is not a peak.
errors ends with Vulkan -4 at presentation wait, then null write in
vulkan_dzn.dll+0x4bca. Disassembly of the exact 0.6.5 DLL shows rep stos after
an unchecked resource Map. Function argument flow and allocation sequence
match dzn_CmdFillBuffer. This supports a secondary CPU map failure; it does
not identify why the graphics device was lost initially.

## v0.6.6 conservative synchronization and device diagnostics

Scheduler rendering-boundary barriers are adapted from Cosmo/gow3_xbox
5203195e3fad1703fcea26aa28a1290b17a70075. They execute outside dynamic
rendering and inside the same recorder order, with ALL_COMMANDS and
MEMORY_READ|MEMORY_WRITE in both directions. The default app mode enables
them; LocalState/gpu-sync.txt normal/conservative provides an A/B switch.
This investigates synchronization rather than claiming a proven loss fix.

The strict Mesa patch now checks both pinned pipeline and command files.
CmdFillBuffer and CmdUpdateBuffer check Map status and pointer before any
pointer arithmetic, write, Unmap or CopyBufferRegion. Failure queries the
native removed reason, logs both codes and sets the Vulkan command-buffer
recording error to UNKNOWN (or OUT_OF_HOST_MEMORY for a known E_OUTOFMEMORY
with no device removal). Native lost-device details remain in the log. It never returns success
for a failed recording. Successful paths retain their original operations.

A desktop-only Map injection setter defaults to zero and is never called
by the app. The native compiler contract injects failed maps into both real
commands, requires EndCommandBuffer to preserve UNKNOWN, then
also checks successful recordings. A standalone C contract covers null
success pointers, failure with nonnull pointer, removed-device classification
and injection reset. No GPU transfer execution is claimed by these tests.

The driver exports native GetDeviceRemovedReason reporting. The bridge logs
its own D3D12 device reason and requests the driver's reason on lost waits.
A native fence UINT64_MAX is treated as device removed. These confirmed
loss paths flush logs and exit, avoiding concurrent unwind after an unusable
device. Generic scheduler loss also requests the driver report. This is
failure handling, not device recovery or evidence that the menu is reached.

Windows validation and Xbox 0.6.6 behavior remain pending below.


## v0.6.6 Windows validation

Code 961091ec81c07655c1eeea492c6efcdda5183c41;
Actions run 39693331177 (renderer 119546077884).

The actual Dozen command contract passed two injected Map failures and two
successful command recordings (Fill and Update). EndCommandBuffer retained
UNKNOWN on injected E_FAIL; no null write occurred. Explicit device status
export was resolved/called. Legacy SM 6.0 reproduced the original shader
rejection; all 80 lowered graphics variants compiled.

Full renderer compile/link passed. Map guard passed 17 checks, diagnostic
sink 29, integer emitter 6705, native preparation six tests, pad 193,
overlay 31, boot trace 29, dialog 21, AudioOut 100 plus device failure,
AJM 4648 with actual MP3 decode and ATRAC9 init. Guest page-fault regression,
8242 swap checks with zero corruption, 32786 POSIX checks, and UWP import
allowlist passed. APPX 0.6.6.0 signed. Separate CPU/graphics builds passed.

The driver is built from the same Mesa pin; strict patch tests cover both
pipeline and command sources. DLL SHA256: 9309911ae5dae7aebbd132bf466ac597f8538c6c7524617153d0fd61c3948191.
The normal app leaves legacy-model and Map fault injection switched off.

This validates compiler/contracts and failure handling on Windows. The
conservative barriers' effect on Xbox device loss is unverified until a new
console test. Logos were confirmed on 0.6.5; menu/gameplay remain pending.

## v0.6.6 Xbox comparison and v0.6.7 loading investigation

Latest attachments loader (14).log / errors (9).log confirm normal mode.
At 300057 ms: 59783 draws, 18202 dispatches, 5563 flip requests; bridge
present 5571 returned; last memory 3368/5120 MiB. Guest controller reads
continued without reported errors. Native DEVICE_HUNG 0x887a0006 was captured.
No watchdog STOP line was present. Timing alone cannot establish whether
the configured 300-second limit affected device failure. User confirmed
logos followed by loading; menu/gameplay remain unconfirmed.

0.6.7 changes only the boot window (900 seconds), default sync mode (normal),
loading I/O observability and watchdog log flushing. It retains the real
device-loss failure path and does not bypass commands, errors or loading.
Windows package validation and Xbox results pending.

### v0.6.7 completed build validation

Source 4fe72ff9e95ea36c4a7e46719372485eaa63efac passed renderer run
39712704424 (job 119607602540). CPU run 39712704506 and graphics run
39712704392 also passed. The real Dozen map-recording and 80-variant
integer fragment driver contracts passed, along with native preparation,
pad, overlay, boot tracing, dialog, AudioOut, AJM, paging, POSIX memory and
UWP import checks. APPX 0.6.7.0 was signed successfully.

Final Linux portable checks passed (44 Python tests and native contracts).
The updated real paged-I/O regression additionally checks active-operation
balance, a failing positioned read, interval reset, and preservation of
active/last-completion state when reporting. Xbox 0.6.7 is untested.

## v0.6.7 Xbox loading result and v0.6.8 task tracing

loader.log (5).txt / errors.log (3).txt confirm 0.6.7 normal mode.
At 900072 ms the explicit watchdog terminated with exit code 124.
239914 draws, 67322 dispatches, 21937 flip requests; bridge presents
continued returning. Last memory 3348/5120 MiB. No DEVICE_HUNG recorded.
899 instrumented reads totaled 81571887 bytes. At termination the last
completed read was 872597 ms old (about 27.5 seconds into boot), with
zero active reads and zero reported read errors in the final interval.
This supports a persistent loading loop/wait, not a confirmed specific
deadlock or proof that every possible I/O path has completed.

0.6.8 adds bounded atomic host-task snapshots at existing five-second
intervals, including API wait type, target, return call site, age and
start/return counts. The API stores host-owned data only and preserves
errno/results. No monitor registry lock, guest-memory dereference, forced
thread suspension or wait bypass. No changes to scheduling or timeouts.
Guest fibers are not individually tracked; untraced work is not progress
evidence. Windows validation and Xbox results pending.

### v0.6.8 Windows validation completed

Tested source 9ed16df6b2c1db7e35f88ee6610f393a690a56f3 passed renderer
run 39717357296, job 119622666687. CPU run 39717357305 and graphics
run 39717357308 passed. The earlier source 0798004 failed to find the
new header in the actual UWP renderer CMake module; it was corrected
before a package was delivered.

The Windows guest wait trace contract passed real mutex and condition
blocking, wakeup, result/errno preservation, snapshots, thread exit and
capacity exhaustion. Existing driver map-recording, integer graphics
pipelines (80 variants), 6705 integer emitter checks, preparation parity,
pad, overlay, boot/dialog trace, AudioOut, AJM, paging (8242 checks, zero
corruption), POSIX memory and UWP import contracts passed. APPX 0.6.8.0
was signed. The final Linux trace contract also passed, alongside the
portable suite (44 Python tests plus native checks).

Xbox 0.6.8 remains untested. This is loading diagnosis, not a confirmed
fix for reaching the menu or gameplay.

## v0.6.8 Xbox result and v0.6.9 investigation

loader.log (6).txt / errors.log (4).txt: normal sync; counters stopped
near 40 seconds and DEVICE_HUNG 0x887a0006 was captured. Last snapshot
45663 ms: 7843 draws, 4540 dispatches, 747 flips, 4217/5120 MiB.
138 task slots registered, none dropped. FlipHandler/GfxEop waited on
events while bridge acquire and scheduler submit remained pending.
Compute handler waits alone are not proof of incorrect synchronization.
No missing-import termination was recorded.

Queue audit: graphics and presentation use the same queue; caller paths
already hold Scheduler::submit_mutex for acquire and presentation. No
new nested locking is introduced. The serial option disables existing
parallel draw recording, Vulkan recording and copy worker paths, without
skipping work. Detailed task tracing defaults off. Native DRED settings
are requested dynamically before device creation, with graceful absence.
The actual Dozen device is queried on failure; output traversal is bounded.
Next-operation inspection is omitted for overwritten history, unsampled
counters and completed/out-of-range lists. No exact fault claim is made.

References: Microsoft Learn use-dred and D3D12_AUTO_BREADCRUMB_NODE.
Support in Xbox Dev Mode is unverified. The defaults combine serial work,
less tracing and optional DRED; one successful run cannot isolate cause.
Windows build and Xbox test pending.

### v0.6.9 Windows validation completed

Tested source 651ed7231199da6b528491c84ae5b574f74e0366 passed renderer
run 39720375729 (Dozen job 119632214153; renderer job 119633010038).
CPU run 39720375723 and graphics run 39720375724 also passed.
Actual driver DRED QueryInterface returned S_OK; healthy desktop WARP
breadcrumb/page-fault queries returned DXGI_ERROR_UNSUPPORTED 0x887a0004
and were handled without failure. The host test does not enable DRED
settings, remove the device or execute GPU game work. Successful lost-device
DRED output traversal and Xbox support remain untested.

Real driver map injection (two failures, two successes), 80 integer shader
pipelines, 6705 emitter checks, preparation parity, pad, overlay, task/wait
tracing, boot/dialog tracing, AudioOut, AJM, paging (8242 checks, zero
corruption), POSIX memory and UWP import checks passed. APPX 0.6.9.0 signed.
Linux portable checks passed, including 44 Python tests and native contracts.

Packaged Dozen SHA256:
eb8f415e230984b240cf05237e33f69d637f5f900c6e5600d879edacc989473c
No game dump is available here; no menu/gameplay test was performed.

Primary API documentation:
https://learn.microsoft.com/windows/win32/direct3d12/use-dred
https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_auto_breadcrumb_node

## v0.6.9 serial Xbox results and v0.6.10 sleep-origin diagnosis

Serial graphics, normal synchronization, task tracing off: final 202411 ms,
124356 draws, 35838 dispatches, 11442 flip requests; no recorded device loss.
User reported smoother loading animation and faster entry, but no menu.
Task tracing on: final 155508 ms, 94426 draws, 27675 dispatches and 8721
flip requests. 136 task slots, none dropped. Main repeatedly requests 1 ms
sleeps; its call count increases throughout. Job-worker calls continue
returning. FIOS callback/IO condition waits settle around 20 seconds with no
active reads. Neither idle IO nor unused compute event waits establishes a
bug. Bridge presentation continues returning. No DEVICE_HUNG was recorded.
These counters measure runtime calls, not unique frames or gameplay FPS.

The main sleep call_site in 0.6.9 is inside the native kernel wrapper:
sleep_ns captured its own return address. Version 0.6.10 explicitly captures
return address zero at each exported sleep ABI entry (noinline when tracing
is compiled) and forwards it through helpers. sceKernelNanosleep no longer
captures the intermediate POSIX entry. No guest stack unwind, code/data
reads, forced wakeup, sleep duration change or shader skipping is added.
Other APIs with intermediate helpers may still expose native helper sites.
This narrows the next investigation; it does not identify the polling
predicate, prove why loading is incomplete or fix the menu transition.

The new contract calls all five actual exported sleep entries through an
assembly SysV caller without unwind metadata or a frame pointer. It checks
the exact return label, nanosecond conversion, successful return, guest and
host errno, remainder clearing, disabled tracing, invalid pointer/time
handling and snapshot output. Linux optimized contract passed, and the same contract rejects the old
source at the exact-caller assertion. All 44 Python tests and portable
native contracts passed. Clang is unavailable locally; Windows clang-cl
build remains pending. Xbox 0.6.10 is untested.
