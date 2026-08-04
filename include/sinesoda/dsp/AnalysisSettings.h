#pragma once

#include <cstddef>
#include <string_view>

namespace sinesoda::dsp {

enum class TransformKind {
  Stft = 0,
  LogFilterbank,
  Mel,
  Bark,
  Erb,
  Cqt,
  Vqt,
  MorletCwt,
  MorseCwt,
  Stockwell,
  ReassignedStft,
  SynchrosqueezedCwt,
  Chroma,
  HaarWaveletPacket,
  SmoothedPseudoWignerVille,
};

enum class WindowKind {
  Hann = 0,
  BlackmanHarris,
  Hamming,
  FlatTop,
  Rectangular,
};

struct AnalysisSettings {
  TransformKind transform = TransformKind::Stft;
  WindowKind window = WindowKind::Hann;
  double sampleRate = 48000.0;
  std::size_t fftSize = 2048;
  std::size_t hopSize = 512;
  std::size_t bandCount = 192;
  double minFrequency = 20.0;
  double maxFrequency = 20000.0;
  int binsPerOctave = 24;
  double tuningCents = 0.0;
  double qScale = 1.0;
  double gamma = 0.0;
  double maximumWindowSeconds = 4.0;

  void validate() const;
};

[[nodiscard]] std::string_view transformName(TransformKind kind) noexcept;
[[nodiscard]] std::string_view windowName(WindowKind kind) noexcept;

}  // namespace sinesoda::dsp
