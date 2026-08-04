# Testing and validation

## Automated core checks

`SineSodaTests` is a dependency-free test executable registered with CTest.
The current suite covers:

- FFT impulse and inverse reconstruction;
- frequency-scale round trips and monotonicity;
- calibrated STFT amplitude and frequency localization;
- log, mel, Bark, and ERB filterbank ordering/localization;
- CQT pitch/amplitude, VQT window shortening, and all analytic wavelet modes;
- reassigned coordinates, chroma pitch classes, Haar energy conservation, and
  smoothed pseudo-Wigner-Ville localization;
- streaming exact-hop behavior and block-size invariance;
- real-time time-axis cadence, speed-dependent tick selection, and invalid-host
  fallbacks;
- complete logarithmic frequency fitting, clamped scrollable pitch viewports,
  continuous display-band interpolation, and resize-safe raster/time mapping;
- SPSC order, wraparound, overload discard, background frame production,
  complete frame-queue delivery, runtime reconfiguration, closed-editor queue
  draining, and strict high-resolution display-backlog bounds.

Run normal and sanitizer configurations with:

```sh
make test
make sanitize
```

Sanitizers are enabled for both compilation and linking of the DSP/runtime
library and test executable on Clang/GCC platforms.

## Exhaustive macOS analyzer matrix

`make qa-macos` builds and runs `SineSodaExhaustiveQA`. The matrix covers all
15 transforms at 22.05, 32, 44.1, 48, 88.2, 96, 176.4, 192, and 384 kHz. Each
of those 135 cases is run as one contiguous buffer, at fixed host blocks of 1,
32, 64, 512, and 2,048 samples, and with the repeating variable pattern
`1, 17, 64, 511, 3, 1024, 29`.

The gate checks frame dimensions and metadata, finite nonnegative magnitudes
and frequency coordinates, Nyquist bounds, reassigned-time coordinates,
measurable response to a deterministic multicomponent signal, and exact final
frame equality across every host block partition. The current macOS run passed
135/135 cases with a maximum block-partition delta of zero. The same executable
also completed under ASan and UBSan.

Generated evidence is written to `artifacts/qa/macos-exhaustive/`:

- `transform-matrix.json` is the machine-readable result;
- `transform-matrix.md` is the complete human-readable table;
- `transform-previews.svg` and the checked PNG rendering show actual analyzer
  frames for every transform at 48 kHz using a deterministic chirp, two tones,
  and three transients.

The proof image uses the plug-in's viridis mapping and fixed -96 to 0 dBFS
range. It intentionally preserves transform-dependent calibration differences;
it is not contrast-normalized per panel. With a fixed 2,048-sample analysis
window, conventional FFT and wavelet localization becomes coarser as sample
rate increases. CQT/VQT instead preserve frequency geometry by increasing
their required sample support, which is visible in the report and should be
treated as a latency/resolution tradeoff rather than a block-size failure.

## Performance audit

`make benchmark` builds an optimized manual benchmark. It reports median
single-frame latency and a transform-only maximum frame rate for all catalog
entries at 48 kHz, FFT 2,048, and 192 requested bands. It is intentionally not
a CTest pass/fail gate because shared CI machines have unstable timing.

Interpret CQT/VQT results together with their required window: the default
20 Hz CQT needs about 81,906 trailing samples (1.71 seconds at 48 kHz), even
when its CPU time is low enough for live updates.

## VST3 validation

Build an optimized plug-in, then run Steinberg's validator:

```sh
cmake --preset release
cmake --build --preset release --target SineSoda-vst3
/path/to/validator build/release/out/SineSoda.vst3
/path/to/validator -e build/release/out/SineSoda.vst3
```

The standard suite exercises factory discovery, buses, 32/64-bit processing,
silence, flush, state, parameters, editor classes, and lifecycle behavior. The
`-e` suite substantially expands randomized process/state/parameter cases.

For the pre-1.0 macOS development build, observed results were:

- standard Steinberg suite: 47/47 passed;
- extensive Steinberg suite: 537/537 passed;
- CTest: 32/32 internal cases passed after the rendering/performance fixes;
- ASan + UBSan: passed;
- clean dependency-cache configure, build, ad-hoc sign, and CTest: passed;
- release bundle: universal `x86_64 arm64`, strict code-sign verification
  passed;
- FL Studio 2026 v26.1.1.5313: VST3 scan and plug-in verification passed;
  SineSoda was registered as an effect in the FL Studio plug-in database;
- native transform-menu interaction, constrained resizing, and an isolated
  save/close/clean-process reload passed. The reload restored Master Slot 1,
  Morlet CWT, Freeze ON, and the `Morlet CWT | PAUSED` display caption.

The 2026-08-04 rendering/performance evidence, including the direct final-host
capture and updated exhaustive matrix, is in
`artifacts/qa/macos-performance-fix/`.

These observations describe the tested machine/build; they are not a substitute
for release CI artifacts or notarization.

## FL Studio smoke test

1. Copy the bundle to the VST3 directory documented in the README.
2. Open **Options > Manage plugins** and run **Find installed plugins**.
3. Confirm SineSoda reports `ok`, `VST3`, and `Effect`.
4. Insert it in a mixer effect slot and verify that audio remains bit-identical
   with the effect enabled.
5. Play a sine sweep and inspect ridge direction and hover frequency.
6. Exercise every transform, palette, display scale, freeze/unfreeze, and editor
   resize. In Native/Linear modes, wheel-scroll over the piano or plot and
   confirm the keys, grid, spectrogram, and teal viewport indicator move
   together. Confirm Log returns to the complete piano range.
7. Save, close, and reload a project to verify parameter/editor state.
8. Repeat at 22.05, 32, 44.1, 48, 88.2, 96, 176.4, 192, and 384 kHz and
   with host block sizes 1, 32, 64, 512, and 2,048 where the host permits.

The Steinberg validator is the automated host-contract gate. FL Studio testing
is the final integration/UI gate because it catches host-specific scanning,
resizing, focus, and state behavior the validator cannot cover.

## Platform matrix

| Platform | Core | VST3 build | Validator | FL Studio |
|---|---:|---:|---:|---:|
| macOS arm64 | Verified | Verified | Verified | Scan, mixer UI, native menus, resize, and project reload verified |
| macOS x86_64 slice | Built and inspected | Built | Covered in universal bundle | Rosetta host pass still recommended |
| Windows x64 | CI configuration provided | CI configuration provided | Not locally verified | Not locally verified |
