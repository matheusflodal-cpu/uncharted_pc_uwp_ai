# Validation of the first UWP diagnostic milestone

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
- **38 Python tests passed, zero failures, zero errors, zero skipped.**
  Modules: `test_probe`, `test_prepare`, `test_link_libc`, `test_content`,
  `test_uwp_prepare`.
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

- Installation or execution of the UWP package on Xbox.
- Memory pressure under the Xbox's actual OS-enforced AppContainer budget.
- Uncharted's real executable initialization, any graphics, audio, input or gameplay.
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
Uncharted guest renderer. Xbox execution remains pending until a tester supplies
its new log. This milestone must not be described as playable.

## Xbox graphics bridge reported by the tester

The tester's 0.2 log reports `SraKmd_arden`, Vulkan 1.2, a 1280x720
shared presentation image, `CoreWindow first frame presented`, and exit 0.
Dynamic rendering and synchronization2 are available; bufferDeviceAddress and
sparseBinding are not. This validates presentation of the probe attachment,
not guest shaders, textures, game rendering, or sustained throughput.
