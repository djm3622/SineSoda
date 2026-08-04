# Product-page visual system

The product page uses the repository's transform implementations for every
spectrogram and the captured FL Studio-hosted SineSoda surface for every plugin
animation. The packager preserves the captured host bar, control geometry,
keyboard, level meter, borders, and rounded outer frame. It updates only native
value text needed by the two tours and replaces only the exact spectrogram
viewport.

Frequency-domain snapshots use 8,192-point analysis on a 20 Hz–20 kHz
logarithmic display. Chroma keeps its intrinsic twelve pitch-class rows. The
linear display appears only in the scale feature tour.

No source name or editorial comparison label is baked into a GIF or snapshot.
The page provides descriptive text as accessible HTML.

## Product-page spaces

| Site space | Source and view | Delivered assets |
|---|---|---|
| Transform atlas | All 15 transforms at the native plugin viewport aspect ratio | `assets/transform-*.webp` |
| Harmonic-resolution story | Sustained extended chords and playhead synchronized to a scrolling CQT in the real plugin surface | `assets/harmonic-resolution.gif` |
| Transient-localization story | Short ascending notes and playhead synchronized to frequency-reassigned STFT | `assets/transient-localization.gif` |
| Pitch-class story | Arpeggiated harmonic functions and playhead synchronized to Chroma | `assets/pitch-class-motion.gif` |
| Transform tutorial | Sleek repo-rendered piano roll above the real plugin surface while all 15 transform values change | `assets/tutorial-transform-tour.mp4` |
| Scale tutorial | Native and Linear demonstrate vertical domain navigation; Log remains fixed at its full range | `assets/tutorial-scale-tour.gif` |

## Source design

### Harmonic resolution

The sustained progression uses six-note voicings with low roots, fifths,
extensions, and close upper voices. Long note support exposes harmonic ridges;
the compact upper structure makes frequency-resolution differences visible.

### Pitch structure

Four five-note arpeggios move through major, minor, predominant, and dominant
colors. Notes cross octaves while retaining pitch classes.

### Transient motion

Twenty-four ascending short notes alternate duration and velocity. Octave
punctuations every six steps expose onset localization, ridge concentration,
and cross-term behavior.

## Render and animation conditions

- Sample rate: 48 kHz
- Analysis size: 8,192 points
- Hop size: 1,024 samples
- Snapshot frequency mapping: logarithmic, 20 Hz–20 kHz
- Frequency-oriented density: 384 bands, 48 bins per octave
- Native plugin editor: 1,100 × 700 logical pixels, captured at 2× scale
- Native spectrogram viewport: 946 × 420 logical pixels
- Feature GIF frame rate: 20 fps (`50 ms` per frame)
- Tutorial GIF frame rate: 30 fps (`33.3 ms` per frame)
- Feature loops: 96 frames (4.8 seconds)
- Transform tutorial: 2,250 frames (75 seconds)
- Scale tutorial: 360 frames (12 seconds)
- Tutorial analyzer, playhead, and cursor motion all run at 30 fps

The tutorial cursor follows eased paths between controls. Discrete menus make
large endpoint changes. Slider interactions land on the real knob, press, drag
vertically up or down while the knob traverses the track, release, and hold.
The displayed sweeps cover 1,024–8,192 FFT points, 24–512 bands, 12–48 bins per
octave, 0.25–4.00 Q scale, and 0–200 Hz VQT gamma. The piano-roll
playhead completes an eight-second left-to-right pass and highlights sounding
notes. The analyzer history advances from the same normalized loop position, so
the transform continues to move during cursor pauses and view changes. Each
parameter drag also blends between analyzer renders computed at its true low and
high settings.

Each example uses its own source pattern: sustained extended chords for CQT,
short onset-dense motion for reassigned STFT, and octave-crossing arpeggios for
Chroma. The C++ signal generator and Python piano roll share the same 96-hop
timing definition; playhead position, active notes, and incoming transform
updates therefore remain aligned frame by frame.

In the scale tutorial, Native and Linear move from the display selector to the
left edge of the analyzer, press, drag through both vertical directions, and
release. The plotted frequency viewport follows the same 30 fps drag curve.
Logarithmic display remains fitted to its complete range and does not receive a
domain-scroll gesture.

The compositor pastes into a fixed `(168, 494)–(2060, 1334)` rectangle in the
2,228 × 1,470 source capture. It reapplies the native viewport border and live
edge after the paste, then downsamples the complete captured window. This keeps
resampling inside the viewport and prevents transparent corners, edge bleed,
or host-chrome artifacts.

## Reproduction

Run:

```sh
make site-visuals
```

The packaging step requires Pillow. A non-default interpreter can be supplied
with `SITE_VISUALS_PYTHON=/path/to/python`.

`SineSodaSiteVisuals` writes raw transform frames to
`artifacts/qa/site-visual-export/`. `tools/package_site_visuals.py` clips them
into the captured SineSoda surface, packages the WebP snapshots, and assembles
the smooth feature and tutorial GIFs.
