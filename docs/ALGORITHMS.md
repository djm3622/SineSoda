# Algorithms and parameters

All magnitudes use linear amplitude internally and are converted to dBFS only
for display. The STFT and exact CQT are normalized so a bin-centered sinusoid
of amplitude `A` produces approximately `A`. Filterbanks aggregate power and
then return root-mean-square amplitude.

## Transform catalog

| Transform | Best suited to | Main trade-off and relevant parameters |
|---|---|---|
| STFT | General spectral inspection, transients, mixing | Fixed linear frequency resolution `sampleRate / FFT size`. Larger FFTs sharpen frequency and smear time. Window controls leakage; speed controls hop. |
| Log filterbank | Musical spectra with compact log-frequency rows | Triangular filters over one STFT. `Bands`, low/high frequency, FFT size, window. |
| Mel | Perceptual speech/audio overview | Mel-spaced triangular filters emphasize resolution at lower frequencies. Same filterbank parameters. |
| Bark | Critical-band-style auditory analysis | Bark-spaced filters approximate human critical bands. Same filterbank parameters. |
| ERB | Cochlear/auditory resolution | ERB-rate-spaced filters provide a smooth auditory scale. Same filterbank parameters. |
| CQT | Notes, harmonics, tuning, wide musical ranges | Geometric centers and constant relative bandwidth. Long low-frequency windows create inherent latency. `Bins/octave`, Q, tuning, low/high, bands, window. |
| VQT | CQT-like pitch resolution with shorter low-frequency windows | Uses bandwidth `f/Q + gamma`; increasing gamma shortens low-frequency windows and weakens strict constant-Q behavior. |
| Morlet CWT | Ridges, chirps, multiscale events | Analytic Gaussian/Morlet frequency responses evaluated on an FFT frame. Q controls concentration; FFT size limits time support and low-frequency resolution. |
| Morse CWT | Tunable analytic wavelet analysis | Peak-normalized generalized Morse response with gamma fixed to 3 and beta scaled by Q. |
| Stockwell | Frequency-dependent localization while retaining absolute phase | Gaussian S-transform response; Q scales its frequency concentration. |
| Reassigned STFT (frequency) | Sharper sinusoidal and chirp ridges | Three STFTs estimate local frequency and time coordinates. The current renderer uses frequency reassignment; time offsets remain available in `SpectralFrame` for a future nonuniform-time renderer. |
| Synchrosqueezed CWT | Concentrated instantaneous-frequency ridges | Reassigns Morlet scale coefficients by phase derivative. Peak aggregation avoids artificial gain from duplicated scales. |
| Chroma | Harmony, chords, pitch-class activity | Folds STFT power into 12 pitch classes. Tuning, FFT size, window, and low/high range matter; octave information is intentionally discarded. |
| Haar wavelet packet | Orthogonal multiresolution/transient analysis | Full dyadic Haar packet tree. The actual band count is the largest power of two no greater than requested bands and FFT size. |
| Smoothed pseudo-Wigner-Ville | High-resolution single-component structure | Time- and lag-smoothed quadratic distribution. Cross-terms can appear for multicomponent signals. Negative display energy is clipped to zero before square root. |

## Core parameters

- **FFT size (1,024-8,192):** analysis frame for FFT-based transforms. Larger
  values improve linear frequency resolution at the cost of temporal smearing
  and compute.
- **Window:** Hann is the balanced default. Blackman-Harris suppresses sidelobes,
  Hamming is a classic alternative, flat-top favors amplitude measurement, and
  rectangular gives maximum nominal resolution with high leakage.
- **Speed:** selects a hop of FFT/2, FFT/4, FFT/8, or FFT/16. Smaller hops add
  more columns and overlap. Exact CQT/VQT can impose a larger effective hop to
  preserve live operation.
- **Bands:** output rows for filterbanks and most bank-based transforms.
- **Bins/octave:** geometric density for CQT/VQT and analytic wavelet banks.
- **Low/high:** transform construction and rendered frequency limits. High is
  clamped to the current sample-rate Nyquist frequency.
- **Q:** multiplier on the nominal CQT Q,
  `1 / (2^(1 / binsPerOctave) - 1)`, and a concentration multiplier for
  analytic wavelet responses.
- **Gamma:** additive VQT bandwidth in hertz. It is ignored by CQT.
- **Floor/ceiling:** dBFS endpoints of the palette and color bar; they never
  modify analysis values.
- **Display scale:** native row placement, logarithmic hertz, or linear hertz.
  Logarithmic mode fits the complete analyzed piano range. Native and linear
  modes keep keys readable and support mouse-wheel pitch scrolling; Shift-wheel
  moves by one semitone. Chroma always uses native pitch-class placement.

## Boundary assumptions

FFT-based wavelet and Stockwell banks evaluate the coefficient at the newest
point of a finite, periodically interpreted frame. This is appropriate for a
real-time display but differs from a padded offline CWT near record boundaries.
CQT/VQT use causal trailing kernels and therefore require the complete longest
window before old zero-padding has left the history. The UI begins displaying
immediately, so the earliest low-frequency columns contain startup padding.

The analyzer downsamples stereo to `0.5 * (left + right)`. Phase-opposed stereo
content can therefore cancel in the visualization even though pass-through
audio remains unchanged. Mid/side and per-channel display modes are natural
future additions.
