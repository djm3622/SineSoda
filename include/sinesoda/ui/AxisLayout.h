#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace sinesoda::ui {

struct TimeAxisScale {
  double secondsPerColumn = 0.01;
  double majorTickSeconds = 1.0;
};

struct FrequencyViewport {
  double lowHertz = 20.0;
  double highHertz = 20000.0;
  double centerMidiNote = 69.0;
  double visibleMidiNotes = 120.0;
  bool scrollable = false;
};

[[nodiscard]] inline double midiNoteFromHertz(double hertz) noexcept {
  return 69.0 + 12.0 * std::log2(std::max(hertz, std::numeric_limits<double>::min()) / 440.0);
}

[[nodiscard]] inline double hertzFromMidiNote(double midiNote) noexcept {
  return 440.0 * std::exp2((midiNote - 69.0) / 12.0);
}

[[nodiscard]] inline FrequencyViewport makeFrequencyViewport(
    double totalLowHertz, double totalHighHertz, double plotHeight, bool fitFullRange,
    double requestedCenterMidiNote = std::numeric_limits<double>::quiet_NaN(),
    double minimumKeyPixels = 6.0, double minimumVisibleNotes = 12.0) noexcept {
  if (!std::isfinite(totalLowHertz) || !std::isfinite(totalHighHertz) || totalLowHertz <= 0.0 ||
      totalHighHertz <= totalLowHertz) {
    totalLowHertz = 20.0;
    totalHighHertz = 20000.0;
  }

  const double totalLowNote = midiNoteFromHertz(totalLowHertz);
  const double totalHighNote = midiNoteFromHertz(totalHighHertz);
  const double totalNotes = totalHighNote - totalLowNote;
  const double totalCenter = 0.5 * (totalLowNote + totalHighNote);
  if (fitFullRange || !std::isfinite(plotHeight) || plotHeight <= 0.0 ||
      !std::isfinite(minimumKeyPixels) || minimumKeyPixels <= 0.0) {
    return {totalLowHertz, totalHighHertz, totalCenter, totalNotes, false};
  }

  const double readableNotes =
      std::max(std::max(minimumVisibleNotes, 1.0), plotHeight / minimumKeyPixels);
  const double visibleNotes = std::min(totalNotes, readableNotes);
  if (visibleNotes >= totalNotes - 1.0e-9) {
    return {totalLowHertz, totalHighHertz, totalCenter, totalNotes, false};
  }

  const double halfSpan = 0.5 * visibleNotes;
  const double requestedCenter =
      std::isfinite(requestedCenterMidiNote) ? requestedCenterMidiNote : totalCenter;
  const double center =
      std::clamp(requestedCenter, totalLowNote + halfSpan, totalHighNote - halfSpan);
  return {hertzFromMidiNote(center - halfSpan), hertzFromMidiNote(center + halfSpan), center,
          visibleNotes, true};
}

[[nodiscard]] inline double chooseTimeTickSeconds(double minimumSeconds) noexcept {
  if (!std::isfinite(minimumSeconds) || minimumSeconds <= 0.0) {
    return 1.0;
  }

  const double decade = std::pow(10.0, std::floor(std::log10(minimumSeconds)));
  const double normalized = minimumSeconds / decade;
  constexpr double steps[]{1.0, 2.0, 2.5, 5.0, 10.0};
  for (const double step : steps) {
    if (normalized <= step) {
      return step * decade;
    }
  }
  return 10.0 * decade;
}

[[nodiscard]] inline TimeAxisScale makeTimeAxisScale(double sampleRate, std::size_t hopSamples,
                                                     double targetTickPixels = 90.0) noexcept {
  if (!std::isfinite(sampleRate) || sampleRate <= 0.0 || hopSamples == 0 ||
      !std::isfinite(targetTickPixels) || targetTickPixels <= 0.0) {
    return {};
  }

  const double secondsPerColumn = static_cast<double>(hopSamples) / sampleRate;
  return {secondsPerColumn, chooseTimeTickSeconds(secondsPerColumn * targetTickPixels)};
}

}  // namespace sinesoda::ui
