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

Explicitly **not validated**:

- Windows SDK/clang-cl compilation, PE imports, package signing or installation.
- Windows/UWP-specific memory mapping, physical aliases, actual paging or memory pressure.
- Uncharted's real executable initialization, any graphics, audio, input or gameplay.
- Frame rate, resolution, stability or progress through any of the three games.

The inherited UWP memory contract and swap-integrity tests are included for the
first Windows/Xbox run; their successful execution in the GoW3 project does not
establish that this adaptation passes them.
