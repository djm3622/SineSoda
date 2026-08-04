# SineSoda macOS exhaustive QA evidence

Run date: 2026-08-03

## Result

- Analyzer matrix: **135/135 passed**.
- Coverage: all 15 transforms at 22.05, 32, 44.1, 48, 88.2, 96, 176.4,
  192, and 384 kHz.
- Host partitions per case: contiguous reference; fixed blocks of 1, 32, 64,
  512, and 2,048 samples; variable pattern `1, 17, 64, 511, 3, 1024, 29`.
- Maximum final-frame difference between any host partition and its contiguous
  reference: **0**.
- Steinberg VST3 validator: **47/47 standard** and **537/537 extensive** tests
  passed on the final bundle.
- Core CTest, formatting gate, ASan, and UBSan passed.
- Final installed VST3: strict code-sign verification passed; universal
  `arm64` + `x86_64`; installed binary matched the validated build with SHA-256
  `77503f213278c729afaa55d655b0bb3f0ca822a72a0e971c998493daeae98bb7`.
- Windows was intentionally not exercised in this run.

The matrix validates dimensions and metadata, finite nonnegative magnitudes and
frequency coordinates, Nyquist bounds, reassigned-time coordinates, measurable
response to a deterministic chirp/tone/transient signal, and exact streaming
invariance across every block partition.

## Transform and sample-rate evidence

- `transform-matrix.json`: machine-readable results for all 135 cases.
- `transform-matrix.md`: full human-readable table, including required analysis
  support, output bands, peak response, and block delta.
- `transform-previews.svg` and `transform-previews.png`: actual 48 kHz analyzer
  frames for STFT, log/Mel/Bark/ERB filterbanks, CQT, VQT, Morlet CWT, Morse CWT,
  Stockwell, reassigned STFT, synchrosqueezed CWT, chroma, Haar wavelet packet,
  and smoothed pseudo Wigner-Ville.

The preview uses one fixed -96 to 0 dBFS viridis mapping, not per-panel contrast
normalization. Brightness differences therefore retain transform-dependent
calibration. A fixed 2,048-sample FFT/wavelet window has coarser frequency
localization as sample rate rises. CQT preserves its low-frequency geometry by
growing from 29,784 samples at 48 kHz to 238,270 samples at 384 kHz; this is an
intentional latency/resolution tradeoff. VQT's gamma term shortens the longest
low-frequency kernels.

## Resize evidence

The editor was rendered in Steinberg's macOS editor host at these logical
content sizes:

| Evidence | Logical content size | Result |
|---|---:|---|
| `editor-minimum-720x420.png` | 720 x 420 | PASS |
| `editor-default-1100x700.png` | 1,100 x 700 | PASS |
| `editor-wide-1275x600.png` | 1,275 x 600 | PASS |
| `editor-tall-800x778.png` | 800 x 778 | PASS |

A host request for 500 x 300 was clamped to the declared 720 x 420 minimum.
Labels, controls, frequency grid, color bar, `dBFS` footer, and corner resizer
remain visible in all captures. The configured upper bound is 2,560 x 1,600;
that exact size was not visually reachable on the test display, so it is
code-constrained but not represented by a screenshot.

## FL Studio save/reload evidence

Host: FL Studio 2026 v26.1.1.5313 on Apple Silicon macOS.

1. SineSoda was inserted in Master mixer Slot 1.
2. The native transform menu was opened in FL Studio without a crash.
3. The instance was set to **Morlet CWT** with **Freeze ON**.
4. The project was saved, the isolated FL Studio process was closed, and the
   `.flp` was opened in a new process.
5. Master Slot 1 restored SineSoda; its transform, freeze state, and plot caption
   restored as **Morlet CWT | PAUSED**.

Artifacts:

- `SineSoda_QA_Roundtrip.flp`: saved reload fixture.
- `fl-studio-native-transform-menu.png`: native popup proof.
- `fl-studio-before-reload.png`: state before saving.
- `fl-studio-after-reload.png`: state after a clean-process reload.

The host pass found and fixed four integration defects: a missing bundled font,
a clipped color-bar footer, acceptance of undersized host resize requests, and
an FL Studio crash in iPlug's custom popup renderer. The final build uses native
macOS popup menus and also clears pre-restore display history so a frozen
reloaded transform cannot retain a stale STFT caption.
