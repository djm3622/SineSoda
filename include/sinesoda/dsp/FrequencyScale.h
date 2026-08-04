#pragma once

namespace sinesoda::dsp {

enum class FrequencyScale {
  Linear,
  Logarithmic,
  Mel,
  Bark,
  Erb,
};

[[nodiscard]] double scaleFromHertz(double hertz, FrequencyScale scale);
[[nodiscard]] double hertzFromScale(double value, FrequencyScale scale);
[[nodiscard]] double noteNumberFromHertz(double hertz, double tuningHertz = 440.0);
[[nodiscard]] double hertzFromNoteNumber(double noteNumber, double tuningHertz = 440.0);

}  // namespace sinesoda::dsp
