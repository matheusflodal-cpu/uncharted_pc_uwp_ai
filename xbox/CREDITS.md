# Credits and provenance

This is an experimental **UWP runtime and graphics diagnostic**, not a playable
Uncharted Xbox release. No game binaries, assets or prepared game images are included.

- SuperMedo / Mohammed Albarghouthi and contributors:
  https://github.com/Supermedo/uncharted_pc at v0.3.6,
  `b787c1068c3a9a9ba3ec43dd8086af2bf7d35c7d`. Original Uncharted runtime,
  preparation/linking scripts, game-specific fixes and source tree. GPL-2.0.
- Cosmo and contributors: https://github.com/Cosmo/gow3_xbox at
  `d0975c91d827045724230476c1527cd38afd72f4`.
  UWP file/CRT adapters, sparse guest-memory backend, disk pager, memory contract tests and native preparation;
  adapted here with the `UNCHARTED_UWP` guard and `BB_` environment names. GPL-2.0.
- KytyPS5 Xbox pager design (`1303170`), credited by the GoW3 implementation:
  https://github.com/Cosmo/KytyPS5. GPL-2.0.
- winpthreads, MSYS2 UCRT64 14.0.0.r495.g63ebe6cdb-1. Its COPYING is copied into
  the diagnostic package. MIT / ZPL, as documented by the dependency.

The original project builds on deadinside28's Bloodborne native runtime and
the shadPS4 renderer. The original attribution and LICENSE remain in the root.
Version 0.1 tests CPU/memory only; 0.2 links the standalone presentation bridge;
the experimental 0.3 integration links the Uncharted renderer for synthetic
shader/texture tests. No game rendering or gameplay is claimed as validated.

Mesa/Dozen ships in the graphics builds. The tester confirmed the 0.2
presentation probe on Xbox; this does not validate Uncharted rendering.

New integration code is GPL-2.0-or-later, compatible with the inherited code.

## Graphics presentation probe

The optional graphics bridge and probe adapt Cosmo/gow3_xbox commit
`d0975c91d827045724230476c1527cd38afd72f4` (GPL-2.0-or-later).
Mesa/Dozen is built from Cosmo/mesa_ai_slop_version commit
`82804960ac7919b24dbf27d1cec2b4d637573731`; its MIT license ships with the package.
Vulkan headers and Microsoft DXC are pinned by archive SHA-256 in the build
scripts/workflow. Their license files ship under ThirdParty. No Sony game data
or shaders are included.

The 0.4 native preparer adapts the GoW3 parser/linker at the same commit above.
Its output magic and import canonicalization follow the original Uncharted
Python scripts. No GoW3 game-specific XML or graphics patches are applied.

## Native Xbox input 0.5

The Windows.Gaming.Input apartment, reading and vibration approach adapts
Cosmo/gow3_xbox `d0975c91d827045724230476c1527cd38afd72f4`,
`xbox/platform/input.cpp`, plus the Xbox Back-request handling in its UWP host. Guest pad layouts, errors and export contracts follow
Supermedo/uncharted_pc `src/runtime_pad.c`. Error constants were also checked
against shadPS4's pad_errors.h. New selection, mapping and contract tests are GPL-2.0-or-later.
