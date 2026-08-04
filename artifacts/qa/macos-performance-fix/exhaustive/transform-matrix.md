# SineSoda macOS exhaustive transform QA

Passed **135/135** transform/sample-rate cases over nine standard host rates from 22.05 to 384 kHz. Each case was compared across fixed host blocks of 1, 32, 64, 512, and 2,048 samples plus the variable pattern `1, 17, 64, 511, 3, 1024, 29`.

| Transform | Rate | Window | Bands | Peak | Max block delta | Result |
|---|---:|---:|---:|---:|---:|:---:|
| STFT | 22.1 kHz | 2048 | 967 | 441.4 Hz | 0.0e+00 | PASS |
| STFT | 32.0 kHz | 2048 | 969 | 437.5 Hz | 0.0e+00 | PASS |
| STFT | 44.1 kHz | 2048 | 926 | 430.7 Hz | 0.0e+00 | PASS |
| STFT | 48.0 kHz | 2048 | 851 | 445.3 Hz | 0.0e+00 | PASS |
| STFT | 88.2 kHz | 2048 | 463 | 430.7 Hz | 0.0e+00 | PASS |
| STFT | 96.0 kHz | 2048 | 425 | 421.9 Hz | 0.0e+00 | PASS |
| STFT | 176.4 kHz | 2048 | 232 | 430.7 Hz | 0.0e+00 | PASS |
| STFT | 192.0 kHz | 2048 | 213 | 468.8 Hz | 0.0e+00 | PASS |
| STFT | 384.0 kHz | 2048 | 106 | 375.0 Hz | 0.0e+00 | PASS |
| Log filterbank | 22.1 kHz | 2048 | 192 | 434.6 Hz | 0.0e+00 | PASS |
| Log filterbank | 32.0 kHz | 2048 | 192 | 435.0 Hz | 0.0e+00 | PASS |
| Log filterbank | 44.1 kHz | 2048 | 192 | 425.9 Hz | 0.0e+00 | PASS |
| Log filterbank | 48.0 kHz | 2048 | 192 | 439.1 Hz | 0.0e+00 | PASS |
| Log filterbank | 88.2 kHz | 2048 | 192 | 413.1 Hz | 0.0e+00 | PASS |
| Log filterbank | 96.0 kHz | 2048 | 192 | 400.6 Hz | 0.0e+00 | PASS |
| Log filterbank | 176.4 kHz | 2048 | 192 | 388.6 Hz | 0.0e+00 | PASS |
| Log filterbank | 192.0 kHz | 2048 | 192 | 425.9 Hz | 0.0e+00 | PASS |
| Log filterbank | 384.0 kHz | 2048 | 192 | 286.3 Hz | 0.0e+00 | PASS |
| Mel | 22.1 kHz | 2048 | 192 | 447.8 Hz | 0.0e+00 | PASS |
| Mel | 32.0 kHz | 2048 | 192 | 438.2 Hz | 0.0e+00 | PASS |
| Mel | 44.1 kHz | 2048 | 192 | 439.6 Hz | 0.0e+00 | PASS |
| Mel | 48.0 kHz | 2048 | 192 | 439.6 Hz | 0.0e+00 | PASS |
| Mel | 88.2 kHz | 2048 | 192 | 420.3 Hz | 0.0e+00 | PASS |
| Mel | 96.0 kHz | 2048 | 192 | 401.2 Hz | 0.0e+00 | PASS |
| Mel | 176.4 kHz | 2048 | 192 | 401.2 Hz | 0.0e+00 | PASS |
| Mel | 192.0 kHz | 2048 | 192 | 439.6 Hz | 0.0e+00 | PASS |
| Mel | 384.0 kHz | 2048 | 192 | 293.5 Hz | 0.0e+00 | PASS |
| Bark | 22.1 kHz | 2048 | 192 | 440.6 Hz | 0.0e+00 | PASS |
| Bark | 32.0 kHz | 2048 | 192 | 439.0 Hz | 0.0e+00 | PASS |
| Bark | 44.1 kHz | 2048 | 192 | 425.4 Hz | 0.0e+00 | PASS |
| Bark | 48.0 kHz | 2048 | 192 | 438.8 Hz | 0.0e+00 | PASS |
| Bark | 88.2 kHz | 2048 | 192 | 412.2 Hz | 0.0e+00 | PASS |
| Bark | 96.0 kHz | 2048 | 192 | 399.2 Hz | 0.0e+00 | PASS |
| Bark | 176.4 kHz | 2048 | 192 | 399.2 Hz | 0.0e+00 | PASS |
| Bark | 192.0 kHz | 2048 | 192 | 425.4 Hz | 0.0e+00 | PASS |
| Bark | 384.0 kHz | 2048 | 192 | 287.7 Hz | 0.0e+00 | PASS |
| ERB | 22.1 kHz | 2048 | 192 | 445.4 Hz | 0.0e+00 | PASS |
| ERB | 32.0 kHz | 2048 | 192 | 434.4 Hz | 0.0e+00 | PASS |
| ERB | 44.1 kHz | 2048 | 192 | 428.6 Hz | 0.0e+00 | PASS |
| ERB | 48.0 kHz | 2048 | 192 | 443.3 Hz | 0.0e+00 | PASS |
| ERB | 88.2 kHz | 2048 | 192 | 414.3 Hz | 0.0e+00 | PASS |
| ERB | 96.0 kHz | 2048 | 192 | 400.2 Hz | 0.0e+00 | PASS |
| ERB | 176.4 kHz | 2048 | 192 | 400.2 Hz | 0.0e+00 | PASS |
| ERB | 192.0 kHz | 2048 | 192 | 428.6 Hz | 0.0e+00 | PASS |
| ERB | 384.0 kHz | 2048 | 192 | 286.7 Hz | 0.0e+00 | PASS |
| CQT | 22.1 kHz | 13682 | 182 | 440.0 Hz | 0.0e+00 | PASS |
| CQT | 32.0 kHz | 19856 | 192 | 440.0 Hz | 0.0e+00 | PASS |
| CQT | 44.1 kHz | 27364 | 192 | 440.0 Hz | 0.0e+00 | PASS |
| CQT | 48.0 kHz | 29784 | 192 | 440.0 Hz | 0.0e+00 | PASS |
| CQT | 88.2 kHz | 54728 | 192 | 440.0 Hz | 0.0e+00 | PASS |
| CQT | 96.0 kHz | 59568 | 192 | 440.0 Hz | 0.0e+00 | PASS |
| CQT | 176.4 kHz | 109455 | 192 | 440.0 Hz | 0.0e+00 | PASS |
| CQT | 192.0 kHz | 119135 | 192 | 440.0 Hz | 0.0e+00 | PASS |
| CQT | 384.0 kHz | 238270 | 192 | 440.0 Hz | 0.0e+00 | PASS |
| VQT | 22.1 kHz | 1021 | 182 | 440.0 Hz | 0.0e+00 | PASS |
| VQT | 32.0 kHz | 1481 | 192 | 440.0 Hz | 0.0e+00 | PASS |
| VQT | 44.1 kHz | 2041 | 192 | 440.0 Hz | 0.0e+00 | PASS |
| VQT | 48.0 kHz | 2222 | 192 | 440.0 Hz | 0.0e+00 | PASS |
| VQT | 88.2 kHz | 4082 | 192 | 440.0 Hz | 0.0e+00 | PASS |
| VQT | 96.0 kHz | 4443 | 192 | 440.0 Hz | 0.0e+00 | PASS |
| VQT | 176.4 kHz | 8163 | 192 | 440.0 Hz | 0.0e+00 | PASS |
| VQT | 192.0 kHz | 8885 | 192 | 440.0 Hz | 0.0e+00 | PASS |
| VQT | 384.0 kHz | 17769 | 192 | 440.0 Hz | 0.0e+00 | PASS |
| Morlet CWT | 22.1 kHz | 2048 | 182 | 466.2 Hz | 0.0e+00 | PASS |
| Morlet CWT | 32.0 kHz | 2048 | 192 | 415.3 Hz | 0.0e+00 | PASS |
| Morlet CWT | 44.1 kHz | 2048 | 192 | 370.0 Hz | 0.0e+00 | PASS |
| Morlet CWT | 48.0 kHz | 2048 | 192 | 479.8 Hz | 0.0e+00 | PASS |
| Morlet CWT | 88.2 kHz | 2048 | 192 | 403.5 Hz | 0.0e+00 | PASS |
| Morlet CWT | 96.0 kHz | 2048 | 192 | 380.8 Hz | 0.0e+00 | PASS |
| Morlet CWT | 176.4 kHz | 2048 | 192 | 415.3 Hz | 0.0e+00 | PASS |
| Morlet CWT | 192.0 kHz | 2048 | 192 | 508.4 Hz | 0.0e+00 | PASS |
| Morlet CWT | 384.0 kHz | 2048 | 192 | 370.0 Hz | 0.0e+00 | PASS |
| Morse CWT | 22.1 kHz | 2048 | 182 | 466.2 Hz | 0.0e+00 | PASS |
| Morse CWT | 32.0 kHz | 2048 | 192 | 415.3 Hz | 0.0e+00 | PASS |
| Morse CWT | 44.1 kHz | 2048 | 192 | 380.8 Hz | 0.0e+00 | PASS |
| Morse CWT | 48.0 kHz | 2048 | 192 | 479.8 Hz | 0.0e+00 | PASS |
| Morse CWT | 88.2 kHz | 2048 | 192 | 415.3 Hz | 0.0e+00 | PASS |
| Morse CWT | 96.0 kHz | 2048 | 192 | 392.0 Hz | 0.0e+00 | PASS |
| Morse CWT | 176.4 kHz | 2048 | 192 | 427.5 Hz | 0.0e+00 | PASS |
| Morse CWT | 192.0 kHz | 2048 | 192 | 493.9 Hz | 0.0e+00 | PASS |
| Morse CWT | 384.0 kHz | 2048 | 192 | 370.0 Hz | 0.0e+00 | PASS |
| Stockwell | 22.1 kHz | 2048 | 182 | 466.2 Hz | 0.0e+00 | PASS |
| Stockwell | 32.0 kHz | 2048 | 192 | 415.3 Hz | 0.0e+00 | PASS |
| Stockwell | 44.1 kHz | 2048 | 192 | 370.0 Hz | 0.0e+00 | PASS |
| Stockwell | 48.0 kHz | 2048 | 192 | 479.8 Hz | 0.0e+00 | PASS |
| Stockwell | 88.2 kHz | 2048 | 192 | 403.5 Hz | 0.0e+00 | PASS |
| Stockwell | 96.0 kHz | 2048 | 192 | 380.8 Hz | 0.0e+00 | PASS |
| Stockwell | 176.4 kHz | 2048 | 192 | 415.3 Hz | 0.0e+00 | PASS |
| Stockwell | 192.0 kHz | 2048 | 192 | 493.9 Hz | 0.0e+00 | PASS |
| Stockwell | 384.0 kHz | 2048 | 192 | 370.0 Hz | 0.0e+00 | PASS |
| Reassigned STFT (frequency) | 22.1 kHz | 2048 | 967 | 441.4 Hz | 0.0e+00 | PASS |
| Reassigned STFT (frequency) | 32.0 kHz | 2048 | 969 | 437.5 Hz | 0.0e+00 | PASS |
| Reassigned STFT (frequency) | 44.1 kHz | 2048 | 926 | 430.7 Hz | 0.0e+00 | PASS |
| Reassigned STFT (frequency) | 48.0 kHz | 2048 | 851 | 445.3 Hz | 0.0e+00 | PASS |
| Reassigned STFT (frequency) | 88.2 kHz | 2048 | 463 | 430.7 Hz | 0.0e+00 | PASS |
| Reassigned STFT (frequency) | 96.0 kHz | 2048 | 425 | 421.9 Hz | 0.0e+00 | PASS |
| Reassigned STFT (frequency) | 176.4 kHz | 2048 | 232 | 430.7 Hz | 0.0e+00 | PASS |
| Reassigned STFT (frequency) | 192.0 kHz | 2048 | 213 | 468.8 Hz | 0.0e+00 | PASS |
| Reassigned STFT (frequency) | 384.0 kHz | 2048 | 106 | 375.0 Hz | 0.0e+00 | PASS |
| Synchrosqueezed CWT | 22.1 kHz | 2048 | 182 | 466.2 Hz | 0.0e+00 | PASS |
| Synchrosqueezed CWT | 32.0 kHz | 2048 | 192 | 415.3 Hz | 0.0e+00 | PASS |
| Synchrosqueezed CWT | 44.1 kHz | 2048 | 192 | 370.0 Hz | 0.0e+00 | PASS |
| Synchrosqueezed CWT | 48.0 kHz | 2048 | 192 | 479.8 Hz | 0.0e+00 | PASS |
| Synchrosqueezed CWT | 88.2 kHz | 2048 | 192 | 403.5 Hz | 0.0e+00 | PASS |
| Synchrosqueezed CWT | 96.0 kHz | 2048 | 192 | 370.0 Hz | 0.0e+00 | PASS |
| Synchrosqueezed CWT | 176.4 kHz | 2048 | 192 | 415.3 Hz | 0.0e+00 | PASS |
| Synchrosqueezed CWT | 192.0 kHz | 2048 | 192 | 508.4 Hz | 0.0e+00 | PASS |
| Synchrosqueezed CWT | 384.0 kHz | 2048 | 192 | 370.0 Hz | 0.0e+00 | PASS |
| Chroma | 22.1 kHz | 2048 | 12 | 440.0 Hz | 0.0e+00 | PASS |
| Chroma | 32.0 kHz | 2048 | 12 | 440.0 Hz | 0.0e+00 | PASS |
| Chroma | 44.1 kHz | 2048 | 12 | 440.0 Hz | 0.0e+00 | PASS |
| Chroma | 48.0 kHz | 2048 | 12 | 440.0 Hz | 0.0e+00 | PASS |
| Chroma | 88.2 kHz | 2048 | 12 | 440.0 Hz | 0.0e+00 | PASS |
| Chroma | 96.0 kHz | 2048 | 12 | 415.3 Hz | 0.0e+00 | PASS |
| Chroma | 176.4 kHz | 2048 | 12 | 440.0 Hz | 0.0e+00 | PASS |
| Chroma | 192.0 kHz | 2048 | 12 | 466.2 Hz | 0.0e+00 | PASS |
| Chroma | 384.0 kHz | 2048 | 12 | 370.0 Hz | 0.0e+00 | PASS |
| Haar wavelet packet | 22.1 kHz | 2048 | 128 | 473.7 Hz | 0.0e+00 | PASS |
| Haar wavelet packet | 32.0 kHz | 2048 | 128 | 437.5 Hz | 0.0e+00 | PASS |
| Haar wavelet packet | 44.1 kHz | 2048 | 128 | 430.7 Hz | 0.0e+00 | PASS |
| Haar wavelet packet | 48.0 kHz | 2048 | 128 | 468.8 Hz | 0.0e+00 | PASS |
| Haar wavelet packet | 88.2 kHz | 2048 | 128 | 516.8 Hz | 0.0e+00 | PASS |
| Haar wavelet packet | 96.0 kHz | 2048 | 128 | 562.5 Hz | 0.0e+00 | PASS |
| Haar wavelet packet | 176.4 kHz | 2048 | 128 | 344.5 Hz | 0.0e+00 | PASS |
| Haar wavelet packet | 192.0 kHz | 2048 | 128 | 375.0 Hz | 0.0e+00 | PASS |
| Haar wavelet packet | 384.0 kHz | 2048 | 128 | 750.0 Hz | 0.0e+00 | PASS |
| Smoothed pseudo Wigner-Ville | 22.1 kHz | 2048 | 192 | 733.3 Hz | 0.0e+00 | PASS |
| Smoothed pseudo Wigner-Ville | 32.0 kHz | 2048 | 192 | 725.5 Hz | 0.0e+00 | PASS |
| Smoothed pseudo Wigner-Ville | 44.1 kHz | 2048 | 192 | 418.6 Hz | 0.0e+00 | PASS |
| Smoothed pseudo Wigner-Ville | 48.0 kHz | 2048 | 192 | 418.6 Hz | 0.0e+00 | PASS |
| Smoothed pseudo Wigner-Ville | 88.2 kHz | 2048 | 192 | 418.6 Hz | 0.0e+00 | PASS |
| Smoothed pseudo Wigner-Ville | 96.0 kHz | 2048 | 192 | 418.6 Hz | 0.0e+00 | PASS |
| Smoothed pseudo Wigner-Ville | 176.4 kHz | 2048 | 192 | 418.6 Hz | 0.0e+00 | PASS |
| Smoothed pseudo Wigner-Ville | 192.0 kHz | 2048 | 192 | 418.6 Hz | 0.0e+00 | PASS |
| Smoothed pseudo Wigner-Ville | 384.0 kHz | 2048 | 192 | 106.9 Hz | 0.0e+00 | PASS |

Notes:

- The deterministic signal contains dominant 440 Hz energy, a quieter 997 Hz tone, a low-level chirp, and a transient.
- CQT/VQT window lengths grow with sample rate because their time support is defined in seconds/cycles, not a fixed FFT count.
- Chroma reports a representative pitch-class frequency rather than a literal spectral-bin peak.
- A zero maximum block delta proves that host buffer partitioning did not change the final frame for this deterministic input.
