# PhaseWraith

PhaseWraith is a YUP-based stereo barberpole phaser. Two six-stage allpass banks per channel sweep with wrapped phase offsets, equal-power crossfade, signed feedback, stereo spread, dry/wet blend, and bounded output. It builds from this project directory, using the adjacent `../yup` checkout when present.

## Identity

- App ID: `jp.ehl.phasewraith`
- Plugin ID: `jp.ehl.phasewraith`
- AU subtype: `PhWr`
- Vendor: `ehl_`; AU manufacturer: `EHL1`
- Version: `0.1.0`
- Type: stereo input/output effect, no MIDI
- macOS formats: Standalone, VST3, AUv2
- Windows formats: Standalone, VST3

## Parameters

- `Rate`: wrapped sweep speed.
- `Depth`: allpass coefficient excursion.
- `Center`: center of the swept phase range.
- `Spread`: stereo and per-stage phase offset amount.
- `Feedback`: signed resonant feedback around the allpass banks.
- `Direction`: forward or reverse sweep direction.
- `Mix`: dry/wet blend.

## Research basis

The implementation follows the first-order phase-shifting network described in [Physical Audio Signal Processing: First-Order Allpass Filters](https://www.dsprelated.com/freebooks/pasp/First_Order_Allpass.html) and the cascading principle in [Allpass Filters](https://www.dsprelated.com/freebooks/pasp/Allpass_Filters.html). The barberpole survey also included the classic AES record on continuously rising or falling phasing structures, [AES E-Library 3268](https://secure.aes.org/forum/pubs/journal/?elib=3268). PhaseWraith's paired six-stage banks, wrapped crossfade, signed feedback, and stereo offsets are deliberate product synthesis.

## Standalone Audition

Standalone builds compile a small audition source behind `YUP_AUDIO_PLUGIN_ENABLE_STANDALONE`. The audition enable and type controls are runtime/UI state only: they are not host parameters and are not serialized. VST3/AU builds compile the no-generator branch, keep the signal path strictly input -> effect -> output, and preserve hosted silence.

The standalone editor shows input/output meters and audition controls. If the YUP standalone macro is unavailable, the editor fails closed as a plain parameter grid with no audition path.

## Build

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug
ctest --preset engine-debug
```

```sh
cmake --preset plugin-release
cmake --build --preset plugin-release
ctest --preset plugin-release
```

Release bundles are staged under the stable `artifacts/plugin-release/<platform-arch>/` tree. `build/` is CMake's internal workspace:

- `phasewraith_release_bundles`
- `phasewraith_standalone_plugin`
- `phasewraith_vst3_plugin`
- `phasewraith_au_plugin` on Apple platforms

On macOS, the local bundle paths are:

- `artifacts/plugin-release/macos-arm64/standalone/phasewraith_standalone_plugin.app`
- `artifacts/plugin-release/macos-arm64/vst3/phasewraith_vst3_plugin.vst3`
- `artifacts/plugin-release/macos-arm64/au/phasewraith_au_plugin.component`

Windows uses `artifacts/plugin-release/windows-x64/` with `standalone/` and `vst3/` directories.

## CI

`.github/workflows/ci.yml` is the required CI entrypoint for pushes to `main`, pull requests, and manual runs. A lightweight Linux classifier always runs. Changes limited to `README.md`, `DESIGN.md`, `LICENSE`, `docs/**`, or `.github/ISSUE_TEMPLATE/**` skip the heavy jobs; every other change runs Debug tests and Release bundle builds on macOS arm64 and Windows x64. Manual dispatches default to forcing both heavy jobs.

Successful heavy runs upload two immutable, 14-day artifacts:

- `PhaseWraith-latest-macos-arm64`, containing `PhaseWraith-latest-macos-arm64.zip` and `SHA256SUMS.txt`
- `PhaseWraith-latest-windows-x64`, containing `PhaseWraith-latest-windows-x64.zip` and `SHA256SUMS.txt`

`.github/workflows/release.yml` is the only `v*` tag workflow. It performs no compilation. The Ubuntu release job resolves lightweight or annotated tags to a commit, requires the tag version to match the CMake project version, requires one successful `CI` push run on `main` for that exact SHA, downloads exactly the two expected artifacts, verifies their SHA-256 manifests and ZIP integrity, then publishes versioned assets such as `PhaseWraith-0.1.0-macos-arm64.zip` and `PhaseWraith-0.1.0-windows-x64.zip`. Publication uses a draft release whose asset list is sanitized and rechecked to contain exactly those two ZIPs. A missing, expired, ambiguous, or mismatched provenance chain fails closed.

Release operator sequence: merge or push the version commit to `main`, wait for both platform jobs and `CI Summary` to pass, then create and push the version tag. GitHub CLI 2.x or newer is required by the release runner. Never move or reuse a published tag; correct the source and use the next patch version instead.

## Layout

- `include/phasewraith/` contains the realtime-safe DSP engine API and local DSP primitives.
- `source/` contains the engine implementation and YUP plugin/editor/state wrapper.
- `tests/` contains deterministic engine regression tests plus hosted and standalone plugin-wrapper bridge tests.
- `cmake/` contains the project-local macOS icon conversion workaround used by the standalone target.
