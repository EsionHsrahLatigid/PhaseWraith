# PhaseWraith Design

PhaseWraith is a focused stereo barberpole phaser. It is not an instrument in hosted formats: hosted VST3/AU processing must never generate sound from silence.

## Product Checklist

- [x] Product version is `0.1.0`.
- [x] App ID is `jp.ehl.phasewraith`.
- [x] Plugin ID is `jp.ehl.phasewraith`.
- [x] AU subtype is `PhWr`.
- [x] State header is product-unique: `PWR1`, version `1`.
- [x] Public host parameters are exactly `Rate`, `Depth`, `Center`, `Spread`, `Feedback`, `Direction`, and `Mix`.
- [x] Plugin format is stereo input/output effect; `PLUGIN_IS_SYNTH` is off.
- [x] MIDI input/output is disabled.

## DSP Checklist

- [x] Signal path order is input + signed feedback -> two six-stage allpass banks -> equal-power wrapped crossfade -> dry/wet -> bounded output.
- [x] Output samples are finite and hard bounded to +/-0.98.
- [x] Silence through the hosted/effect path remains <= 1e-7.
- [x] Deterministic rendering for identical inputs stays within 1e-6.
- [x] Denormal-scale input is flushed to practical silence.
- [x] `Rate`, `Depth`, `Center`, `Spread`, signed `Feedback`, `Direction`, and `Mix` have regression coverage.
- [x] Audio rendering performs no heap allocation, locks, I/O, logging, or UI calls.

## Standalone Isolation Checklist

- [x] Audition source and controls are compiled only when `YUP_AUDIO_PLUGIN_ENABLE_STANDALONE` is defined.
- [x] Hosted bridge tests compile the processor without the standalone macro and prove silence preservation.
- [x] Standalone bridge tests compile with the standalone macro and prove default audition RMS >= 1e-4.
- [x] Audition enable and type are processor-owned runtime/UI state, not host parameters.
- [x] Audition enable and type are not saved or restored in product state.
- [x] Standalone meters use atomics written by the processor and do not affect audio.
- [x] If standalone wrapper detection is unavailable, the editor exposes no audition UI.

## UI Checklist

- [x] Uses the native YUP parameter grid and rotary controls.
- [x] Standalone-only strip provides `Audition On/Off`, audition type, and input/output meters.
- [x] Visual language is high-contrast corroded municipal machinery with a punched grid.
- [x] No external assets or dependencies are added.
- [x] Hosted editor builds as parameters only; generator controls are absent.

## CI and Release Checklist

- [x] CI actions are pinned to immutable action SHAs.
- [x] Heavy CI builds Debug tests and Release bundles for macOS arm64 and Windows x64.
- [x] Debug tests include engine, hosted bridge, and standalone bridge targets.
- [x] Artifacts include strict SHA-256 manifests and expire after 14 days.
- [x] Release workflow promotes exact-SHA artifacts only and performs no build.
- [x] Release provenance fails closed on missing, expired, duplicate, or mismatched artifacts.
- [x] Effect targets are `phasewraith_release_bundles`, `phasewraith_standalone_plugin`, `phasewraith_vst3_plugin`, and Apple-only `phasewraith_au_plugin`.
