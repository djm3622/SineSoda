# Modern real-time UI evidence

Run date: 2026-08-03

`editor-default-modern.png` is a direct macOS window capture of the release
VST3 loaded in Steinberg's editor host at the default 1,100 x 700 logical
content size. It is not a design mockup.

Visible behavior:

- the left ruler is a piano keyboard from the configured low to high frequency,
  with semitone plot guides and labeled C octaves;
- the newest frame enters at the bright right-hand `NOW` line;
- older frames occupy successively smaller x positions, one pixel per analysis
  hop;
- the horizontal ruler uses negative age values because the display is a
  rolling real-time history, not a fixed audio file;
- the header reports the effective cadence and major division (`5.33 ms / px`
  and `500 ms / div` for the captured 48 kHz, 2,048 FFT, 1/8-FFT hop setup);
- the color bar remains explicitly calibrated in dBFS.

The editor-only host does not run audio processing, so the screenshot shows the
fully rendered axes at the dB floor. In FL Studio, every produced spectral frame
is appended at `NOW`; a note onset therefore moves left by one column per hop.
The ruler derives its seconds-per-column from observed frame sample timestamps,
falling back to the requested hop before the first frame.

Verification for the captured final build:

- internal tests: 29/29, including all nine QA sample rates and four speeds for
  time-ruler readability;
- format gate: passed;
- ASan + UBSan: passed;
- Steinberg VST3 validator: 47/47 standard and 537/537 extensive;
- bundle: ad-hoc signature valid, universal arm64 + x86_64;
- installed and build binary SHA-256:
  `9b117a06f24358f1870ec366a62ae2f7dd29a829100a5bc383c10951353ad259`.

The earlier exhaustive transform, sample-rate, resize, and FL Studio project
reload evidence remains in `../macos-exhaustive/`.
