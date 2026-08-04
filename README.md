# SineSoda

SineSoda is a free, open-source VST3 plug-in for exploring time-frequency
analysis on live audio. It gives researchers, engineers, educators, and curious
listeners a shared interface for comparing 15 spectral and time-frequency
representations while the audio passes through unchanged.

The project is intended to be useful beyond a single production workflow. It
can help with choosing a representation for an analysis pipeline, investigating
an unfamiliar signal, demonstrating time-frequency trade-offs, or testing a new
idea against an existing method. The implementation, algorithm notes, tests,
and build configuration are open for inspection and extension.

[Download for macOS](https://github.com/djm3622/SineSoda/releases/latest/download/SineSoda-macOS-universal.zip)
| [Download for Windows](https://github.com/djm3622/SineSoda/releases/latest/download/SineSoda-Windows-x64.zip)
| [Project page](https://djm3622.github.io/SineSoda/)

The current 1.0.0 implementation includes:

- calibrated STFT plus logarithmic, mel, Bark, and ERB filterbanks;
- exact-reference CQT and VQT with selectable bins/octave, Q, tuning, and gamma;
- Morlet and generalized Morse CWTs, Stockwell transform, frequency-reassigned
  STFT, synchrosqueezed CWT, chroma, Haar wavelet packets, and a smoothed
  pseudo-Wigner-Ville distribution;
- 1,024-8,192 point FFTs, five windows, four hop ratios, 24-512 display bands,
  configurable analysis range, and native/log/linear display axes;
- viridis, magma, inferno, turbo, grayscale, and Wave Candy-inspired palettes;
- a calibrated dBFS color bar with +6 dB headroom, adjustable floor/ceiling,
  hover readout, freeze, a piano-key frequency ruler with octave labels,
  scrollable Native/Linear pitch viewports, a full-range logarithmic piano
  view, and a resizable dark UI with horizontal parameter sliders;
- mono and stereo VST3 layouts with bit-exact pass-through and automatic host
  parameter/state persistence.

## Architecture and real-time behavior

`ProcessBlock()` only copies pass-through audio and writes mono samples into a
preallocated lock-free SPSC queue. It performs no allocation and takes no
locks. A dedicated `std::jthread` owns all transform state and publishes
immutable spectral frames to the UI. The UI consumes a bounded frame queue on
the host idle callback. That display queue is limited to 32 frames, and analysis
is suspended while the editor is closed or frozen so a hidden analyzer cannot
retain a large high-resolution STFT backlog.

Each completed analysis hop becomes one new pixel column at the right-hand
`NOW` edge. Existing columns shift one pixel left, so a note onset moves into
the past at exactly `sampleRate / effectiveHop` pixels per second. The time
ruler measures the actual recent frame cadence (including automatic CQT/VQT
rate limiting) and selects readable 1/2/2.5/5-style tick intervals whenever
speed, FFT size, sample rate, or transform cadence changes.

On macOS, visible history is resampled between adjacent analysis-band centers
into one bounded NanoVG texture. This removes flat per-bin display blocks and
replaces hundreds of thousands of per-cell graphics calls with one image draw.
When the editor is wider than the texture cap, the texture is fitted to the
complete plot and time/hover coordinates are scaled to the displayed pixels.

If analysis falls behind by more than 500 ms, stale queued audio is discarded
and the transform history is reset so the display returns to the live signal
instead of accumulating latency. Exact CQT/VQT analysis is automatically
rate-limited against a conservative per-worker compute budget; the effective
hop and any overload counters are shown under the plot. These measures never
alter the audio path.

See [docs/ALGORITHMS.md](docs/ALGORITHMS.md) for definitions, parameter effects,
and implementation boundaries.

## Build

Requirements:

- CMake 3.25 or newer;
- Ninja;
- a C++20 compiler (AppleClang/Xcode command-line tools on macOS, or Visual
  Studio 2022 Build Tools on Windows);
- Git and network access for the first configure.

The first plug-in configure downloads pinned iPlug2 and Steinberg VST3 SDK
sources into the ignored `_deps/` directory. Subsequent builds are offline.

```sh
make bootstrap
make build
```

The debug bundle is written to `build/debug/out/SineSoda.vst3`. For an optimized
universal macOS bundle (arm64 + x86_64):

```sh
cmake --preset release
cmake --build --preset release
```

On Windows, run the same commands from a Developer PowerShell. To build only
the native architecture on macOS, configure with
`-DSINESODA_UNIVERSAL_MACOS=OFF`. Local macOS bundles are ad-hoc signed by
default; production distribution still requires an Apple Developer ID
signature and notarization.

To use an existing iPlug2 checkout:

```sh
cmake --preset release -DIPLUG2_DIR=/absolute/path/to/iPlug2
```

If its `Dependencies/IPlug/VST3_SDK` placeholder is empty, CMake populates it
from the pinned SDK source.

## Test and validate

```sh
make check       # format check (when clang-format is installed) + core tests
make sanitize    # ASan + UBSan core tests
make benchmark   # manual release-mode transform timing table
make qa-macos    # 15 transforms x 9 rates x 6 host block patterns + proof artifacts
```

VST3 host validation, FL Studio installation, and the detailed test matrix are
documented in [docs/TESTING.md](docs/TESTING.md).

## Install in FL Studio

Download and extract the archive for your system, then copy `SineSoda.vst3` to
the per-user VST3 directory:

- macOS: `~/Library/Audio/Plug-Ins/VST3/`
- Windows: `%LOCALAPPDATA%\Programs\Common\VST3\` or another folder configured
  in FL Studio's Plugin Manager

Then open **Options > Manage plugins**, run **Find installed plugins**, and add
SineSoda to a mixer effect slot. It is an analyzer effect, not a generator.
See [docs/INSTALL.md](docs/INSTALL.md) for platform-specific Gatekeeper and
Windows troubleshooting.

## Project status

The DSP, streaming runtime, VST3 wrapper, and UI are implemented. The macOS
bundle has passed the Steinberg standard and extensive validator suites and a
fresh-build test. FL Studio 2026 v26.1.1.5313 also verified and registered the
installed VST3 as an effect. Windows support is represented in the build and CI
configuration but has not yet been exercised on local Windows hardware.

SineSoda is licensed under Apache-2.0. Contributions, validation on additional
hosts and hardware, and discussions about new analysis methods are welcome.
Third-party dependency pins and notices are in
[THIRD_PARTY.md](THIRD_PARTY.md).
