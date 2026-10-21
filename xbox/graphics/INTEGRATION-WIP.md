# Experimental renderer integration

The signed 0.2 package validated standalone presentation on the tester's Xbox.
The next build, 0.3, links the Uncharted renderer and exercises synthetic texture
copies, detiling, sampling, and storage-image writes with checked GPU readbacks.
It then runs the presentation probe. A previous `graphics` run.txt mode also
selects the new tests; `memory` retains the original CPU/memory test.

Compilation and Xbox execution of 0.3 are pending. Do not describe this as
playable. No game assets or precompiled guest shaders are distributed.

`imports` and `boot` still need privately prepared game files. The experimental
`boot` mode enables the renderer, but audio, guest controller, and online APIs
remain unavailable and may stop startup with a named missing import.
Temporal upscalers and frame generation are disabled. Real game shaders,
rendering, RAM requirements, and performance still need validation.
