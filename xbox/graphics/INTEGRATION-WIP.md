# Renderer integration work in progress

The signed 0.2 package on `xbox/uwp-v0.3.6` only links the presentation bridge.
The changes in this local development branch stage Uncharted-specific renderer
adaptation; they have NOT been compiled or validated on Windows or Xbox.
`UNCHARTED_COMPILE_RENDERER` only builds a static library and deliberately does
not link it into the diagnostic host. Do not describe it as gameplay support.

Before enabling a game boot, compile this target with the pinned dependencies,
resolve the host API/import differences, connect real input/audio backends, and
validate the actual guest renderer. Upscalers, frame generation and online guest
imports remain unavailable. The tester still needs to validate the separate 0.2
CoreWindow graphics probe on the console.
