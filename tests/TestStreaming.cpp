#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "TestSupport.h"
#include "sinesoda/dsp/AnalysisSettings.h"
#include "sinesoda/dsp/SpectralFrame.h"
#include "sinesoda/dsp/StreamingAnalyzer.h"

namespace {

std::vector<float> makeSignal(std::size_t size, double sampleRate) {
  std::vector<float> signal(size);
  constexpr double twoPi = 2.0 * std::numbers::pi_v<double>;
  for (std::size_t index = 0; index < size; ++index) {
    const double time = static_cast<double>(index) / sampleRate;
    signal[index] = static_cast<float>(0.6 * std::sin(twoPi * 440.0 * time) +
                                       0.2 * std::sin(twoPi * 1700.0 * time));
  }
  return signal;
}

}  // namespace

SINESODA_TEST(StreamingAnalyzerEmitsAtExactHopBoundaries) {
  sinesoda::dsp::AnalysisSettings settings;
  settings.fftSize = 1024;
  settings.hopSize = 256;
  sinesoda::dsp::StreamingAnalyzer analyzer(settings);
  const auto signal = makeSignal(4096, settings.sampleRate);
  std::vector<std::uint64_t> positions;
  analyzer.push(signal, [&](const sinesoda::dsp::SpectralFrame& frame) {
    positions.push_back(frame.endSample);
  });
  SINESODA_EXPECT_EQ(positions.size(), 16U);
  for (std::size_t index = 0; index < positions.size(); ++index) {
    SINESODA_EXPECT_EQ(positions[index], static_cast<std::uint64_t>((index + 1) * 256));
  }
}

SINESODA_TEST(StreamingResultDoesNotDependOnHostBlockSizes) {
  sinesoda::dsp::AnalysisSettings settings;
  settings.fftSize = 2048;
  settings.hopSize = 512;
  const auto signal = makeSignal(8192, settings.sampleRate);

  sinesoda::dsp::StreamingAnalyzer contiguous(settings);
  sinesoda::dsp::SpectralFrame contiguousLast;
  contiguous.push(signal,
                  [&](const sinesoda::dsp::SpectralFrame& frame) { contiguousLast = frame; });

  sinesoda::dsp::StreamingAnalyzer chunked(settings);
  sinesoda::dsp::SpectralFrame chunkedLast;
  constexpr std::size_t chunkPattern[]{1, 17, 64, 511, 3, 1024, 29};
  std::size_t offset = 0;
  std::size_t patternIndex = 0;
  while (offset < signal.size()) {
    const std::size_t count =
        std::min(chunkPattern[patternIndex % std::size(chunkPattern)], signal.size() - offset);
    chunked.push(std::span<const float>(signal).subspan(offset, count),
                 [&](const sinesoda::dsp::SpectralFrame& frame) { chunkedLast = frame; });
    offset += count;
    ++patternIndex;
  }

  SINESODA_EXPECT_EQ(chunkedLast.endSample, contiguousLast.endSample);
  SINESODA_EXPECT_EQ(chunkedLast.magnitudes.size(), contiguousLast.magnitudes.size());
  for (std::size_t index = 0; index < chunkedLast.magnitudes.size(); ++index) {
    SINESODA_EXPECT_NEAR(chunkedLast.magnitudes[index], contiguousLast.magnitudes[index], 1.0e-7);
  }
}

SINESODA_TEST(StreamingResetClearsPositionAndHistory) {
  sinesoda::dsp::AnalysisSettings settings;
  settings.fftSize = 512;
  settings.hopSize = 128;
  sinesoda::dsp::StreamingAnalyzer analyzer(settings);
  const auto signal = makeSignal(512, settings.sampleRate);
  analyzer.push(signal, [](const auto&) {});
  SINESODA_EXPECT_EQ(analyzer.samplesProcessed(), 512U);
  analyzer.reset();
  SINESODA_EXPECT_EQ(analyzer.samplesProcessed(), 0U);

  sinesoda::dsp::SpectralFrame first;
  analyzer.push(std::span<const float>(signal).first(128),
                [&](const auto& frame) { first = frame; });
  SINESODA_EXPECT_EQ(first.endSample, 128U);
}
