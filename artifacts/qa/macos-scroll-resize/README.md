# macOS piano viewport and resize evidence

Run date: 2026-08-04

## Implemented behavior

- Logarithmic display scale fits the complete analyzed piano range without a
  scroll offset.
- Native and Linear scales allocate approximately six logical pixels per key.
  Mouse-wheel input pans by three semitones; Shift-wheel pans by one.
- A slim teal indicator at the left edge of the keyboard shows the visible
  portion of the full analyzed pitch range.
- The bounded 1,024 x 720 NanoVG texture now fills the complete plot after a
  resize. Time-grid and hover coordinates use the raster-to-display scale, so
  the image, age ruler, and pointer readout remain aligned above the cap.

## Captures

- `default-log-full-range.png`: the default 1,100 x 700 editor with the full
  logarithmic piano ruler.
- `wide-log-after-resize.png`: the same editor after a host resize beyond the
  1,024-pixel raster-width cap. The plot remains fully covered and the displayed
  cadence updates from 5.23 to 4.74 ms/pixel.
- `native-scrolled-up.png`: Native mode after six upward wheel events. The
  visible keys move from the centered C2-C8 region toward C3-C10 and the teal
  viewport indicator moves upward with them.

Steinberg's editor host initializes the editor but does not process audio, so
these captures verify editor geometry and interaction with an empty live plot.
The active-raster resize mapping is covered by the deterministic
`RasterGeometryRemainsAlignedWhenTheDisplayExceedsTheTextureCap` regression
test.

## Verification

- Internal tests: 35/35 passed.
- ASan and UBSan: passed.
- Universal release VST3: `x86_64 arm64`; strict ad-hoc signature verification
  passed.
- Steinberg validator: 47/47 standard and 537/537 extensive tests passed.
