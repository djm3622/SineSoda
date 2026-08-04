#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>

namespace sinesoda::ui {

struct BandInterpolation {
  std::size_t lowerBand = 0;
  std::size_t upperBand = 0;
  float upperFraction = 0.0F;
};

struct RasterDimensions {
  int width = 1;
  int height = 1;
};

[[nodiscard]] inline RasterDimensions makeRasterDimensions(float displayWidth, float displayHeight,
                                                           int maximumWidth = 1024,
                                                           int maximumHeight = 720) noexcept {
  const int safeMaximumWidth = std::max(maximumWidth, 1);
  const int safeMaximumHeight = std::max(maximumHeight, 1);
  const int width = std::isfinite(displayWidth) ? static_cast<int>(std::floor(displayWidth)) : 1;
  const int height = std::isfinite(displayHeight) ? static_cast<int>(std::floor(displayHeight)) : 1;
  return {std::clamp(width, 1, safeMaximumWidth), std::clamp(height, 1, safeMaximumHeight)};
}

[[nodiscard]] inline double rasterSecondsPerDisplayPixel(double secondsPerColumn,
                                                         float displayWidth,
                                                         int rasterWidth) noexcept {
  if (!std::isfinite(secondsPerColumn) || secondsPerColumn <= 0.0 || !std::isfinite(displayWidth) ||
      displayWidth <= 0.0F || rasterWidth <= 0) {
    return 0.01;
  }
  return secondsPerColumn * static_cast<double>(rasterWidth) / static_cast<double>(displayWidth);
}

[[nodiscard]] inline float rasterColumnOffsetFromRight(float x, float plotLeft, float plotRight,
                                                       int rasterWidth) noexcept {
  if (!std::isfinite(x) || !std::isfinite(plotLeft) || !std::isfinite(plotRight) ||
      plotRight <= plotLeft || rasterWidth <= 0) {
    return 0.0F;
  }
  const float fraction = std::clamp((plotRight - x) / (plotRight - plotLeft), 0.0F, 1.0F);
  return fraction * static_cast<float>(rasterWidth);
}

inline void makeBandInterpolations(std::span<const float> normalizedBandPositions,
                                   std::span<BandInterpolation> rows) noexcept {
  if (normalizedBandPositions.empty()) {
    std::fill(rows.begin(), rows.end(), BandInterpolation{});
    return;
  }

  for (std::size_t row = 0; row < rows.size(); ++row) {
    const float normalized =
        1.0F - (static_cast<float>(row) + 0.5F) / static_cast<float>(rows.size());
    const auto upper = std::lower_bound(normalizedBandPositions.begin(),
                                        normalizedBandPositions.end(), normalized);
    if (upper == normalizedBandPositions.end()) {
      const std::size_t lastBand = normalizedBandPositions.size() - 1;
      rows[row] = {lastBand, lastBand, 0.0F};
      continue;
    }

    const std::size_t upperBand = static_cast<std::size_t>(upper - normalizedBandPositions.begin());
    const std::size_t lowerBand = upperBand == 0 ? 0 : upperBand - 1;
    float fraction = 0.0F;
    if (lowerBand != upperBand) {
      const float lowerPosition = normalizedBandPositions[lowerBand];
      const float upperPosition = normalizedBandPositions[upperBand];
      fraction = (normalized - lowerPosition) /
                 std::max(upperPosition - lowerPosition, std::numeric_limits<float>::epsilon());
    }
    rows[row] = {lowerBand, upperBand, std::clamp(fraction, 0.0F, 1.0F)};
  }
}

[[nodiscard]] inline float interpolateBandValue(std::span<const float> values,
                                                const BandInterpolation& interpolation,
                                                float fallback = 0.0F) noexcept {
  if (values.empty()) {
    return fallback;
  }
  const std::size_t lowerBand = std::min(interpolation.lowerBand, values.size() - 1);
  const std::size_t upperBand = std::min(interpolation.upperBand, values.size() - 1);
  return std::lerp(values[lowerBand], values[upperBand], interpolation.upperFraction);
}

}  // namespace sinesoda::ui
