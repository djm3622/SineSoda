#include <array>
#include <stdexcept>

#include "TestSupport.h"
#include "sinesoda/dsp/AnalysisSettings.h"
#include "sinesoda/dsp/FrequencyScale.h"
#include "sinesoda/dsp/SpectralFrame.h"

using sinesoda::dsp::FrequencyScale;

SINESODA_TEST(FrequencyScalesRoundTrip) {
  constexpr std::array scales{FrequencyScale::Linear, FrequencyScale::Logarithmic,
                              FrequencyScale::Mel, FrequencyScale::Bark, FrequencyScale::Erb};
  constexpr std::array frequencies{20.0, 55.0, 440.0, 1000.0, 10000.0, 20000.0};
  for (const auto scale : scales) {
    for (const double frequency : frequencies) {
      const double coordinate = sinesoda::dsp::scaleFromHertz(frequency, scale);
      const double recovered = sinesoda::dsp::hertzFromScale(coordinate, scale);
      SINESODA_EXPECT_NEAR(recovered, frequency, frequency * 1.0e-10 + 1.0e-9);
    }
  }
}

SINESODA_TEST(NoteFrequencyConversionUsesConfigurableTuning) {
  SINESODA_EXPECT_NEAR(sinesoda::dsp::noteNumberFromHertz(440.0), 69.0, 1.0e-12);
  SINESODA_EXPECT_NEAR(sinesoda::dsp::hertzFromNoteNumber(69.0), 440.0, 1.0e-12);
  SINESODA_EXPECT_NEAR(sinesoda::dsp::noteNumberFromHertz(442.0, 442.0), 69.0, 1.0e-12);
}

SINESODA_TEST(DecibelConversionHandlesSilenceAndUnity) {
  SINESODA_EXPECT_NEAR(sinesoda::dsp::amplitudeToDb(1.0F), 0.0, 1.0e-6);
  SINESODA_EXPECT_NEAR(sinesoda::dsp::amplitudeToDb(0.0F, -120.0F), -120.0, 1.0e-6);
  SINESODA_EXPECT_NEAR(sinesoda::dsp::dbToAmplitude(-6.0205999F), 0.5, 1.0e-5);
}

SINESODA_TEST(AnalysisSettingsRejectInvalidRanges) {
  sinesoda::dsp::AnalysisSettings settings;
  settings.fftSize = 1000;
  SINESODA_EXPECT_THROW(settings.validate(), std::invalid_argument);
  settings.fftSize = 2048;
  settings.maxFrequency = 25000.0;
  SINESODA_EXPECT_THROW(settings.validate(), std::invalid_argument);
  settings.maxFrequency = 20000.0;
  settings.bandCount = 0;
  SINESODA_EXPECT_THROW(settings.validate(), std::invalid_argument);
}
