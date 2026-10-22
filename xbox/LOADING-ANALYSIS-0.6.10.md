# Xbox 0.6.10 loading-wait analysis

## Inputs and scope

User-provided SELF containers were inspected read-only. The existing
scripts/prepare.py parser reconstructed plaintext ELF segments in temporary
scratch files. No decryption was performed. Original inputs and reconstructed
game binaries must not be committed or packaged.

Input SHA256:
- big3-ps4_Shipping(1).elf: acc952e5a9ac7062f8ecfc52700faa599d5f29deb1e24f9e81e6de832326d1d0
- big2-ps4_Shipping(1).elf: 61c0456314a8de2dbd32e1c426f7828092de7d25a58b83386ced1a48d7765ebe
- eboot.bin: 8953b6ed5cf4d1d26aa3aa4cd1e0c72817ea155f6a5774fb17f6fec7d787f953
- sce_discmap.plt: c291f9f078841eda117919c6595cbc7b33fd416f7367c0c435693264c670fd88

The three executable inputs use SELF containers despite two filenames ending
in .elf. Their required executable segments are plaintext and supported by the
existing parser. The optional segment 10 is absent from reconstruction.
sce_discmap.plt begins with PLT and is not an ELF executable; no necessity for
changing its runtime support is established.

## Xbox observation

loader.log (9).txt reports source package 0.6.10, normal synchronization,
serial execution, task tracing on, and guest image base 0x400000.
The last snapshot at 310882 ms has 195593 draws, 55265 dispatches and 17918 flip
requests. Bridge acquire/present each have 17925 returns and no active call.
PM4 queue 0 has submitted/resumed/completed 340838. Main is repeatedly
sleeping at absolute call_site 0x88c54a; 62 snapshots show the same site.
Final main calls/returned: 145695/145694. FIOS reads are idle with no recorded
read error; last read completion is around 20.654 seconds. Final app memory
3285/5120 MiB; no swap in/out. No DEVICE_HUNG or named missing-import stop was
recorded. End of file alone does not establish a crash or exit reason.
These are API counters, not unique rendered frames.

## Address normalization and static result

Sleep call_site is an absolute guest address, not an ELF offset:
0x88c54a - 0x400000 = 0x48c54a.
Disassembling at 0x88c54a without subtracting the loaded image base produces
unrelated instructions; it is not evidence of an executable mismatch.

The uploaded big3 code has:
- 0x48c4c0: generic wait entry; saves object from RDI into RBX, target from RSI
  into R15, comparison mode from EDX into R14D.
- 0x48c540: requests a one-millisecond pause through helper 0x49c010.
  That helper multiplies the argument by 1000 and tail-jumps to its import.
  Tail calls retain the guest wait routine's original return address.
- 0x48c54a: reads the 64-bit field at object + 0x20.
- 0x48c554: calls comparator 0x49e1c0 with current field, target and mode.
- 0x48c55b: repeats when the comparator returns false.
- Comparator mode 0 means equality; mode 1 means signed less-than-or-equal;
  other modes return false.
- Job workers can use a different branch that calls helper 0x4a0050 while
  waiting. The observed main sleep site uses the pause branch.

The same generic wait has many static callers; a shared sleep address does
not identify which object or caller is blocking the main task.

Embedded assertion labels identify wrappers in the game-loading code:
- 0x4d1400: WaitForBinAvailability, target 10, comparison mode 1.
- 0x4d1460: WaitForDataAvailability, target 9, comparison mode 1.
- 0x4d14c0: WaitForCoreAvailability, target 8, comparison mode 1.
- 0x4d1520: WaitForSteadyState, target 0, comparison mode 0.
These wrappers read g_pGameLoadingState from image offset 0x161ba30.
Their existence does NOT prove that main is currently waiting on this global:
other call sites use the same generic wait.

## Controlled executable check

Only the inspected wait routine and comparator were copied into an isolated
temporary executable mapping. The job-manager global was zero, objects were
artificial, and the pause helper was replaced by a bounded test callback that
decremented the synthetic field. No game entry point, filesystem, scheduler,
renderer or Xbox execution was exercised.

Passed four cases:
- current 3, target 0, mode 0: three pause requests, stops at 0.
- current 3, target 1, mode 1: two pause requests, stops at 1.
- current 0, target 0, mode 0: no pause.
- current -1, target 0, mode 1: no pause.

This corroborates the static predicate; it does not reproduce the loading bug.

## Next investigation

Capture, per host thread, the wait object, current value, target, comparison
mode and original wait caller, using a tightly bounded opt-in profile validated
against the executable bytes. Correlate main's object with the loading-state
global and with the code that updates the field. Preserve all guest registers,
flags, ABI alignment and existing wait behavior; add a meaningful native
contract before packaging any instrumentation. Existing generic hook counters
share last arguments across threads and are insufficient by themselves.
No forced completion, arbitrary state write or API-success stub is justified.
No new signed package or gameplay fix is produced by this analysis.
