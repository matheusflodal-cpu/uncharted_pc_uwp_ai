# Credits and provenance

This is an experimental **CPU-loader and memory diagnostic**, not a playable
Uncharted Xbox release. No game binaries, assets or prepared game images are included.

- SuperMedo / Mohammed Albarghouthi and contributors:
  https://github.com/Supermedo/uncharted_pc at v0.3.6,
  `b787c1068c3a9a9ba3ec43dd8086af2bf7d35c7d`. Original Uncharted runtime,
  preparation/linking scripts, game-specific fixes and source tree. GPL-2.0.
- Cosmo and contributors: https://github.com/Cosmo/gow3_xbox at
  `d0975c91d827045724230476c1527cd38afd72f4`.
  UWP file/CRT adapters, sparse guest-memory backend, disk pager and memory contract tests;
  adapted here with the `UNCHARTED_UWP` guard and `BB_` environment names. GPL-2.0.
- KytyPS5 Xbox pager design (`1303170`), credited by the GoW3 implementation:
  https://github.com/Cosmo/KytyPS5. GPL-2.0.
- winpthreads, MSYS2 UCRT64 14.0.0.r495.g63ebe6cdb-1. Its COPYING is copied into
  the diagnostic package. MIT / ZPL, as documented by the dependency.

The original project builds on deadinside28's Bloodborne native runtime and
the shadPS4 renderer. The original attribution and LICENSE remain in the root.
The renderer is preserved in the source tree but **not linked into this diagnostic**.

Mesa/Dozen is the proposed graphics bridge for a later milestone; this CPU-only
package neither bundles it nor claims that it renders Uncharted on Xbox.

New integration code is GPL-2.0-or-later, compatible with the inherited code.
