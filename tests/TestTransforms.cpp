#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <numeric>
#include <vector>

#include "TestSupport.h"
#include "sinesoda/dsp/AnalysisSettings.h"
#include "sinesoda/dsp/SpectralFrame.h"
#include "sinesoda/dsp/Transforms.h"

namespace {

std::vector<float> makeSine(double frequency, double sampleRate, std::size_t size,
                            float amplitude = 1.0F) {
  std::vector<float> signal(size);
  constexpr double twoPi = 2.0 * std::numbers::pi_v<double>;
  for (std::size_t index = 0; index < size; ++index) {
    signal[index] =
        amplitude *
        static_cast<float>(std::sin(twoPi * frequency * static_cast<double>(index) / sampleRate));
  }
  return signal;
}

std::size_t maximumIndex(const std::vector<float>& values) {
  return static_cast<std::size_t>(
      std::distance(values.begin(), std::max_element(values.begin(), values.end())));
}

}  // namespace

SINESODA_TEST(StftFindsBinCenteredSineWithCalibratedAmplitude) {
  sinesoda::dsp::AnalysisSettings settings;
  settings.fftSize = 4096;
  settings.hopSize = settings.fftSize;
  const std::size_t targetBin = 85;
  const double frequency =
      static_cast<double>(targetBin) * settings.sampleRate / static_cast<double>(settings.fftSize);
  const auto signal = makeSine(frequency, settings.sampleRate, settings.fftSize, 0.75F);
  auto transform = sinesoda::dsp::makeTransform(settings);
  sinesoda::dsp::SpectralFrame frame;
  transform->analyze(signal, frame);

  const auto peak = maximumIndex(frame.magnitudes);
  SINESODA_EXPECT_NEAR(frame.centerFrequencies[peak], frequency, 1.0e-4);
  SINESODA_EXPECT_NEAR(frame.magnitudes[peak], 0.75, 2.0e-3);
  SINESODA_EXPECT_EQ(frame.centerFrequencies.size(), frame.magnitudes.size());
}

SINESODA_TEST(FilterbanksAreOrderedAndLocalizeTone) {
  constexpr std::array kinds{sinesoda::dsp::TransformKind::LogFilterbank,
                             sinesoda::dsp::TransformKind::Mel, sinesoda::dsp::TransformKind::Bark,
                             sinesoda::dsp::TransformKind::Erb};
  for (const auto kind : kinds) {
    sinesoda::dsp::AnalysisSettings settings;
    settings.transform = kind;
    settings.fftSize = 4096;
    settings.bandCount = 64;
    auto transform = sinesoda::dsp::makeTransform(settings);
    const auto signal = makeSine(1000.0, settings.sampleRate, settings.fftSize);
    sinesoda::dsp::SpectralFrame frame;
    transform->analyze(signal, frame);

    SINESODA_EXPECT_EQ(frame.magnitudes.size(), settings.bandCount);
    SINESODA_EXPECT_TRUE(
        std::is_sorted(frame.centerFrequencies.begin(), frame.centerFrequencies.end()));
    const auto peak = maximumIndex(frame.magnitudes);
    const double error = std::abs(static_cast<double>(frame.centerFrequencies[peak]) - 1000.0);
    SINESODA_EXPECT_TRUE(error <= static_cast<double>(frame.bandwidths[peak]) * 1.5);
  }
}

SINESODA_TEST(CqtFindsA4AndPreservesSineAmplitude) {
  sinesoda::dsp::AnalysisSettings settings;
  settings.transform = sinesoda::dsp::TransformKind::Cqt;
  settings.minFrequency = 55.0;
  settings.maxFrequency = 1760.0;
  settings.binsPerOctave = 12;
  settings.bandCount = 61;
  auto transform = sinesoda::dsp::makeTransform(settings);
  const auto signal = makeSine(440.0, settings.sampleRate, transform->requiredSamples(), 0.5F);
  sinesoda::dsp::SpectralFrame frame;
  transform->analyze(signal, frame);

  const auto peak = maximumIndex(frame.magnitudes);
  SINESODA_EXPECT_NEAR(frame.centerFrequencies[peak], 440.0, 1.0e-3);
  SINESODA_EXPECT_NEAR(frame.magnitudes[peak], 0.5, 2.0e-3);
}

SINESODA_TEST(VqtGammaShortensTheLongestAnalysisWindow) {
  sinesoda::dsp::AnalysisSettings cqtSettings;
  cqtSettings.transform = sinesoda::dsp::TransformKind::Cqt;
  cqtSettings.minFrequency = 55.0;
  cqtSettings.maxFrequency = 1760.0;
  cqtSettings.bandCount = 61;
  const auto cqt = sinesoda::dsp::makeTransform(cqtSettings);

  auto vqtSettings = cqtSettings;
  vqtSettings.transform = sinesoda::dsp::TransformKind::Vqt;
  vqtSettings.gamma = 20.0;
  const auto vqt = sinesoda::dsp::makeTransform(vqtSettings);
  SINESODA_EXPECT_TRUE(vqt->requiredSamples() < cqt->requiredSamples());
}

SINESODA_TEST(AnalyticWaveletFamiliesLocalizeAStationaryTone) {
  constexpr std::array kinds{
      sinesoda::dsp::TransformKind::MorletCwt,
      sinesoda::dsp::TransformKind::MorseCwt,
      sinesoda::dsp::TransformKind::Stockwell,
      sinesoda::dsp::TransformKind::SynchrosqueezedCwt,
  };
  for (const auto kind : kinds) {
    sinesoda::dsp::AnalysisSettings settings;
    settings.transform = kind;
    settings.fftSize = 4096;
    settings.minFrequency = 110.0;
    settings.maxFrequency = 3520.0;
    settings.binsPerOctave = 24;
    settings.bandCount = 128;
    const auto transform = sinesoda::dsp::makeTransform(settings);
    const auto signal = makeSine(880.0, settings.sampleRate, transform->requiredSamples(), 0.6F);
    sinesoda::dsp::SpectralFrame frame;
    transform->analyze(signal, frame);

    const auto peak = maximumIndex(frame.magnitudes);
    SINESODA_EXPECT_TRUE(!frame.magnitudes.empty());
    if (std::abs(frame.centerFrequencies[peak] - 880.0F) > 30.0F) {
      sinesoda::test::fail("wavelet peak near 880 Hz", __FILE__, __LINE__,
                           std::string(sinesoda::dsp::transformName(kind)));
    }
    SINESODA_EXPECT_TRUE(frame.magnitudes[peak] > 0.35F);
    SINESODA_EXPECT_EQ(frame.transform, kind);
  }
}

SINESODA_TEST(ReassignedStftProducesFiniteFrequencyAndTimeCoordinates) {
  sinesoda::dsp::AnalysisSettings settings;
  settings.transform = sinesoda::dsp::TransformKind::ReassignedStft;
  settings.fftSize = 4096;
  settings.minFrequency = 100.0;
  settings.maxFrequency = 4000.0;
  const auto transform = sinesoda::dsp::makeTransform(settings);
  const auto signal = makeSine(997.0, settings.sampleRate, settings.fftSize, 0.7F);
  sinesoda::dsp::SpectralFrame frame;
  transform->analyze(signal, frame);

  const auto peak = maximumIndex(frame.magnitudes);
  const double binWidth = settings.sampleRate / static_cast<double>(settings.fftSize);
  SINESODA_EXPECT_NEAR(frame.centerFrequencies[peak], 997.0, binWidth * 1.1);
  SINESODA_EXPECT_EQ(frame.timeOffsetsSeconds.size(), frame.magnitudes.size());
  SINESODA_EXPECT_TRUE(std::isfinite(frame.timeOffsetsSeconds[peak]));
}

SINESODA_TEST(ChromaFindsThePitchClassesOfCMajor) {
  sinesoda::dsp::AnalysisSettings settings;
  settings.transform = sinesoda::dsp::TransformKind::Chroma;
  settings.fftSize = 8192;
  settings.minFrequency = 60.0;
  settings.maxFrequency = 5000.0;
  std::vector<float> signal(settings.fftSize, 0.0F);
  for (const double frequency : {261.625565, 329.627557, 391.995436}) {
    const auto tone = makeSine(frequency, settings.sampleRate, settings.fftSize, 0.3F);
    for (std::size_t index = 0; index < signal.size(); ++index) {
      signal[index] += tone[index];
    }
  }
  const auto transform = sinesoda::dsp::makeTransform(settings);
  sinesoda::dsp::SpectralFrame frame;
  transform->analyze(signal, frame);

  SINESODA_EXPECT_EQ(frame.magnitudes.size(), 12U);
  SINESODA_EXPECT_TRUE(frame.magnitudes[0] > frame.magnitudes[1]);  // C over C#
  SINESODA_EXPECT_TRUE(frame.magnitudes[4] > frame.magnitudes[3]);  // E over D#
  SINESODA_EXPECT_TRUE(frame.magnitudes[7] > frame.magnitudes[6]);  // G over F#
}

SINESODA_TEST(HaarWaveletPacketPreservesSignalEnergy) {
  sinesoda::dsp::AnalysisSettings settings;
  settings.transform = sinesoda::dsp::TransformKind::HaarWaveletPacket;
  settings.fftSize = 1024;
  settings.bandCount = 64;
  const auto signal = makeSine(1700.0, settings.sampleRate, settings.fftSize, 0.8F);
  const auto transform = sinesoda::dsp::makeTransform(settings);
  sinesoda::dsp::SpectralFrame frame;
  transform->analyze(signal, frame);

  double inputEnergy = 0.0;
  for (const float sample : signal) {
    inputEnergy += static_cast<double>(sample) * sample;
  }
  double packetEnergy = 0.0;
  const double coefficientsPerLeaf =
      static_cast<double>(settings.fftSize) / frame.magnitudes.size();
  for (const float magnitude : frame.magnitudes) {
    packetEnergy += static_cast<double>(magnitude) * magnitude * coefficientsPerLeaf;
  }
  SINESODA_EXPECT_NEAR(packetEnergy, inputEnergy, inputEnergy * 2.0e-6);
}

SINESODA_TEST(SmoothedPseudoWignerVilleLocalizesSingleTone) {
  sinesoda::dsp::AnalysisSettings settings;
  settings.transform = sinesoda::dsp::TransformKind::SmoothedPseudoWignerVille;
  settings.fftSize = 2048;
  settings.bandCount = 256;
  settings.minFrequency = 100.0;
  settings.maxFrequency = 6000.0;
  const auto transform = sinesoda::dsp::makeTransform(settings);
  const auto signal = makeSine(1200.0, settings.sampleRate, settings.fftSize, 0.5F);
  sinesoda::dsp::SpectralFrame frame;
  transform->analyze(signal, frame);

  const auto peak = maximumIndex(frame.magnitudes);
  SINESODA_EXPECT_NEAR(frame.centerFrequencies[peak], 1200.0, frame.bandwidths[peak] * 2.0);
  SINESODA_EXPECT_TRUE(frame.magnitudes[peak] > 0.05F);
}
