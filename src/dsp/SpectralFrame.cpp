#include "sinesoda/dsp/SpectralFrame.h"

#include <algorithm>
#include <cmath>

namespace sinesoda::dsp {

float amplitudeToDb(float amplitude, float floorDb) noexcept {
  if (!std::isfinite(amplitude) || amplitude <= 0.0F) {
    return floorDb;
  }
  return std::max(floorDb, 20.0F * std::log10(amplitude));
}

float dbToAmplitude(float decibels) noexcept { return std::pow(10.0F, decibels / 20.0F); }

}  // namespace sinesoda::dsp
