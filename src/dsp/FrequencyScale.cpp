#include "sinesoda/dsp/FrequencyScale.h"

#include <cmath>
#include <stdexcept>

namespace sinesoda::dsp {

double scaleFromHertz(double hertz, FrequencyScale scale) {
  if (!std::isfinite(hertz) || hertz <= 0.0) {
    throw std::invalid_argument("frequency must be finite and positive");
  }
  switch (scale) {
    case FrequencyScale::Linear:
      return hertz;
    case FrequencyScale::Logarithmic:
      return std::log(hertz);
    case FrequencyScale::Mel:
      return 2595.0 * std::log10(1.0 + hertz / 700.0);
    case FrequencyScale::Bark:
      return 26.81 / (1.0 + 1960.0 / hertz) - 0.53;
    case FrequencyScale::Erb:
      return 21.4 * std::log10(1.0 + 0.00437 * hertz);
  }
  throw std::invalid_argument("unsupported frequency scale");
}

double hertzFromScale(double value, FrequencyScale scale) {
  if (!std::isfinite(value)) {
    throw std::invalid_argument("scale value must be finite");
  }
  double hertz = 0.0;
  switch (scale) {
    case FrequencyScale::Linear:
      hertz = value;
      break;
    case FrequencyScale::Logarithmic:
      hertz = std::exp(value);
      break;
    case FrequencyScale::Mel:
      hertz = 700.0 * (std::pow(10.0, value / 2595.0) - 1.0);
      break;
    case FrequencyScale::Bark: {
      const double denominator = 26.81 / (value + 0.53) - 1.0;
      if (denominator <= 0.0) {
        throw std::invalid_argument("Bark value is outside the invertible range");
      }
      hertz = 1960.0 / denominator;
      break;
    }
    case FrequencyScale::Erb:
      hertz = (std::pow(10.0, value / 21.4) - 1.0) / 0.00437;
      break;
  }
  if (!std::isfinite(hertz) || hertz <= 0.0) {
    throw std::invalid_argument("scale value does not map to a positive frequency");
  }
  return hertz;
}

double noteNumberFromHertz(double hertz, double tuningHertz) {
  if (!std::isfinite(hertz) || hertz <= 0.0 || !std::isfinite(tuningHertz) || tuningHertz <= 0.0) {
    throw std::invalid_argument("frequency and tuning must be finite and positive");
  }
  return 69.0 + 12.0 * std::log2(hertz / tuningHertz);
}

double hertzFromNoteNumber(double noteNumber, double tuningHertz) {
  if (!std::isfinite(noteNumber) || !std::isfinite(tuningHertz) || tuningHertz <= 0.0) {
    throw std::invalid_argument("note number must be finite and tuning positive");
  }
  return tuningHertz * std::exp2((noteNumber - 69.0) / 12.0);
}

}  // namespace sinesoda::dsp
