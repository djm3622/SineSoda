#include "sinesoda/dsp/AnalysisSettings.h"

#include <array>
#include <cmath>
#include <stdexcept>

#include "sinesoda/dsp/Fft.h"

namespace sinesoda::dsp {

void AnalysisSettings::validate() const {
  if (!std::isfinite(sampleRate) || sampleRate <= 0.0) {
    throw std::invalid_argument("sampleRate must be finite and positive");
  }
  if (!isPowerOfTwo(fftSize) || fftSize < 32) {
    throw std::invalid_argument("fftSize must be a power of two of at least 32");
  }
  if (hopSize == 0 || hopSize > fftSize * 16) {
    throw std::invalid_argument("hopSize must be positive and bounded");
  }
  if (bandCount == 0 || bandCount > 4096) {
    throw std::invalid_argument("bandCount must be in [1, 4096]");
  }
  const double nyquist = sampleRate * 0.5;
  if (!std::isfinite(minFrequency) || !std::isfinite(maxFrequency) || minFrequency <= 0.0 ||
      maxFrequency <= minFrequency || maxFrequency > nyquist) {
    throw std::invalid_argument("frequency range must lie inside (0, Nyquist]");
  }
  if (binsPerOctave < 1 || binsPerOctave > 192) {
    throw std::invalid_argument("binsPerOctave must be in [1, 192]");
  }
  if (!std::isfinite(tuningCents) || std::abs(tuningCents) > 1200.0) {
    throw std::invalid_argument("tuningCents must be finite and in [-1200, 1200]");
  }
  if (!std::isfinite(qScale) || qScale <= 0.0 || qScale > 16.0) {
    throw std::invalid_argument("qScale must be finite and in (0, 16]");
  }
  if (!std::isfinite(gamma) || gamma < 0.0 || gamma > sampleRate * 0.25) {
    throw std::invalid_argument("gamma must be finite and non-negative");
  }
  if (!std::isfinite(maximumWindowSeconds) || maximumWindowSeconds <= 0.0 ||
      maximumWindowSeconds > 30.0) {
    throw std::invalid_argument("maximumWindowSeconds must be in (0, 30]");
  }
}

std::string_view transformName(TransformKind kind) noexcept {
  constexpr std::array<std::string_view, 15> names{
      "STFT",
      "Log filterbank",
      "Mel",
      "Bark",
      "ERB",
      "CQT",
      "VQT",
      "Morlet CWT",
      "Morse CWT",
      "Stockwell",
      "Reassigned STFT (frequency)",
      "Synchrosqueezed CWT",
      "Chroma",
      "Haar wavelet packet",
      "Smoothed pseudo Wigner-Ville",
  };
  const auto index = static_cast<std::size_t>(kind);
  return index < names.size() ? names[index] : "Unknown";
}

std::string_view windowName(WindowKind kind) noexcept {
  constexpr std::array<std::string_view, 5> names{"Hann", "Blackman-Harris", "Hamming", "Flat top",
                                                  "Rectangular"};
  const auto index = static_cast<std::size_t>(kind);
  return index < names.size() ? names[index] : "Unknown";
}

}  // namespace sinesoda::dsp
