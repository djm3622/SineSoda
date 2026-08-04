#pragma once

#include <cstdint>
#include <vector>

#include "sinesoda/dsp/AnalysisSettings.h"

namespace sinesoda::dsp {

struct SpectralFrame {
  TransformKind transform = TransformKind::Stft;
  std::uint64_t endSample = 0;
  std::size_t analysisWindowSamples = 0;
  std::vector<float> centerFrequencies;
  std::vector<float> bandwidths;
  std::vector<float> magnitudes;
  // Optional time reassignments relative to endSample. Empty for transforms
  // that live on the regular streaming time grid.
  std::vector<float> timeOffsetsSeconds;
};

[[nodiscard]] float amplitudeToDb(float amplitude, float floorDb = -160.0F) noexcept;
[[nodiscard]] float dbToAmplitude(float decibels) noexcept;

}  // namespace sinesoda::dsp
