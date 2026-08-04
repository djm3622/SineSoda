# macOS rendering and performance-fix evidence

Run date: 2026-08-04

## What was wrong

- The analysis-to-display queue held 1,024 vector-backed spectral frames. A queue slot retains its
  vector capacity after a frame is consumed, so an 8,192-point STFT could leave roughly 40 MiB of
  high-water storage in that queue alone. The worker also kept producing frames while the editor
  was closed. This was bounded retention rather than an infinitely growing DSP allocation, but it
  presented like a large leak in a host process.
- The plot issued one rectangle draw for every visible time column and frequency band on every
  repaint. A full default-width 8,192-point STFT could exceed three million rectangle calls per
  redraw. That caused graphics-driver allocation pressure, lag, and large host memory spikes.
- STFT and filterbank bands were drawn as constant-height cells. Stretching those cells onto a log
  frequency axis made the discrete blocks especially visible; the blockiness was in the display
  resampling, not the transform output.
- Every intermediate slider value could rebuild transform state immediately, making continuous
  interaction feel uneven.

## Implemented fixes

- Reduced the display queue to 32 frames, added strict queue-bound tests, and stop analysis plus
  drain queued audio whenever the editor is closed or frozen.
- Replaced the per-cell draw loop on macOS/NanoVG with one bounded RGBA texture update and one
  image draw. The raster is capped at 1,024 x 720 pixels.
- Linearly interpolate calibrated dB values between adjacent transform band centers before color
  mapping. A deterministic five-row interpolation test checks the resulting continuous gradient.
- Coalesce transform configuration changes until controls have been stable for 30 ms while the
  currently active analyzer continues consuming audio.
- Redraw only for new frames, changed status, display settings, resize, or pointer interaction.
- Reworked the control area into three clean rows, replaced rotary knobs with horizontal sliders,
  separated slider values from the tracks, and added clearer typography and labels.
- Set new instances to a +6 dBFS display ceiling, format positive color-bar ticks with an explicit
  plus sign, and draw a 0 dBFS reference line.

## Visual proof

- `editor-positive-ceiling.png`: final release VST3 captured directly in Steinberg's macOS editor
  host. It shows the slider layout, separated values, piano ruler, dynamic time ruler, `6.0 dBFS`
  ceiling control, `+6` color-bar tick, and 0 dBFS reference line.
- `exhaustive/transform-previews.svg`: actual transform outputs for every implemented transform.
- `exhaustive/transform-matrix.md`: full transform/sample-rate results.

The editor host does not process audio, so the UI capture intentionally shows an empty live plot.
The exhaustive transform previews contain the generated spectral data. FL Studio save/reload proof
from the unchanged state-serialization path remains in `../macos-exhaustive/`.

## Final verification

- Internal tests: **32/32 passed**, including closed-editor retention, strict high-resolution queue
  bounds, and interpolated raster sampling.
- ASan + UBSan: passed.
- Exhaustive analyzer matrix: **135/135 passed** across 15 transforms and nine sample rates, with
  exact output invariance across host block partitions.
- Steinberg VST3 validator: **47/47 standard** and **537/537 extensive** tests passed.
- Bundle: strict ad-hoc code-sign verification passed; universal `arm64` + `x86_64`.
- Installed binary and validated build SHA-256:
  `bb5ed940d7abfd5d15b4193683818780efd3de8598d403d0b8938260c665f215`.
