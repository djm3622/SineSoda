#include <array>
#include <cmath>

#include "TestSupport.h"
#include "sinesoda/ui/AxisLayout.h"
#include "sinesoda/ui/SpectrogramRaster.h"

SINESODA_TEST(TimeAxisTickSpacingTracksAnalyzerSpeed) {
  struct Expectation {
    std::size_t hopSamples;
    double tickSeconds;
  };
  constexpr std::array expectations{
      Expectation{1024, 2.0},
      Expectation{512, 1.0},
      Expectation{256, 0.5},
      Expectation{128, 0.25},
  };

  for (const auto& expectation : expectations) {
    const auto scale = sinesoda::ui::makeTimeAxisScale(48000.0, expectation.hopSamples);
    SINESODA_EXPECT_NEAR(scale.secondsPerColumn,
                         static_cast<double>(expectation.hopSamples) / 48000.0, 1.0e-12);
    SINESODA_EXPECT_NEAR(scale.majorTickSeconds, expectation.tickSeconds, 1.0e-12);
    SINESODA_EXPECT_NEAR(scale.majorTickSeconds / scale.secondsPerColumn, 93.75, 1.0e-12);
  }
}

SINESODA_TEST(TimeAxisUsesReadableNiceNumberIntervals) {
  SINESODA_EXPECT_NEAR(sinesoda::ui::chooseTimeTickSeconds(0.021), 0.025, 1.0e-12);
  SINESODA_EXPECT_NEAR(sinesoda::ui::chooseTimeTickSeconds(0.28), 0.5, 1.0e-12);
  SINESODA_EXPECT_NEAR(sinesoda::ui::chooseTimeTickSeconds(2.1), 2.5, 1.0e-12);
  SINESODA_EXPECT_NEAR(sinesoda::ui::chooseTimeTickSeconds(6.0), 10.0, 1.0e-12);
}

SINESODA_TEST(TimeAxisRemainsReadableAcrossQASampleRatesAndSpeeds) {
  constexpr std::array sampleRates{22050.0, 32000.0,  44100.0,  48000.0, 88200.0,
                                   96000.0, 176400.0, 192000.0, 384000.0};
  constexpr std::array<std::size_t, 4> hopSamples{1024, 512, 256, 128};

  for (const double sampleRate : sampleRates) {
    for (const std::size_t hop : hopSamples) {
      const auto scale = sinesoda::ui::makeTimeAxisScale(sampleRate, hop);
      const double majorTickPixels = scale.majorTickSeconds / scale.secondsPerColumn;
      SINESODA_EXPECT_TRUE(std::isfinite(majorTickPixels));
      SINESODA_EXPECT_TRUE(majorTickPixels >= 90.0);
      SINESODA_EXPECT_TRUE(majorTickPixels <= 180.0);
    }
  }
}

SINESODA_TEST(TimeAxisFallsBackForInvalidHostValues) {
  const auto scale = sinesoda::ui::makeTimeAxisScale(0.0, 0);
  SINESODA_EXPECT_NEAR(scale.secondsPerColumn, 0.01, 1.0e-12);
  SINESODA_EXPECT_NEAR(scale.majorTickSeconds, 1.0, 1.0e-12);
}

SINESODA_TEST(LogFrequencyViewportFitsTheCompletePianoRange) {
  const auto viewport = sinesoda::ui::makeFrequencyViewport(20.0, 20000.0, 180.0, true, 90.0);
  SINESODA_EXPECT_NEAR(viewport.lowHertz, 20.0, 1.0e-12);
  SINESODA_EXPECT_NEAR(viewport.highHertz, 20000.0, 1.0e-9);
  SINESODA_EXPECT_TRUE(!viewport.scrollable);
}

SINESODA_TEST(ScrollableFrequencyViewportKeepsPianoKeysReadableAndClamped) {
  const auto centered = sinesoda::ui::makeFrequencyViewport(20.0, 20000.0, 180.0, false);
  SINESODA_EXPECT_NEAR(centered.visibleMidiNotes, 30.0, 1.0e-12);
  SINESODA_EXPECT_TRUE(centered.scrollable);

  const auto highest = sinesoda::ui::makeFrequencyViewport(20.0, 20000.0, 180.0, false, 1000.0);
  SINESODA_EXPECT_NEAR(highest.highHertz, 20000.0, 1.0e-8);
  SINESODA_EXPECT_TRUE(highest.lowHertz > 20.0);

  const auto lowest = sinesoda::ui::makeFrequencyViewport(20.0, 20000.0, 180.0, false, -1000.0);
  SINESODA_EXPECT_NEAR(lowest.lowHertz, 20.0, 1.0e-10);
  SINESODA_EXPECT_TRUE(lowest.highHertz < 20000.0);
}

SINESODA_TEST(RasterGeometryRemainsAlignedWhenTheDisplayExceedsTheTextureCap) {
  const auto dimensions = sinesoda::ui::makeRasterDimensions(1600.0F, 900.0F);
  SINESODA_EXPECT_EQ(dimensions.width, 1024);
  SINESODA_EXPECT_EQ(dimensions.height, 720);
  SINESODA_EXPECT_NEAR(sinesoda::ui::rasterSecondsPerDisplayPixel(0.02, 1600.0F, 1024), 0.0128,
                       1.0e-12);
  SINESODA_EXPECT_NEAR(sinesoda::ui::rasterColumnOffsetFromRight(800.0F, 0.0F, 1600.0F, 1024),
                       512.0, 1.0e-6);
}

SINESODA_TEST(SpectrogramRasterInterpolatesBetweenAnalysisBands) {
  constexpr std::array normalizedBands{0.0F, 0.5F, 1.0F};
  constexpr std::array decibels{-100.0F, -50.0F, 0.0F};
  std::array<sinesoda::ui::BandInterpolation, 5> rows{};
  sinesoda::ui::makeBandInterpolations(normalizedBands, rows);

  constexpr std::array expected{-10.0F, -30.0F, -50.0F, -70.0F, -90.0F};
  for (std::size_t row = 0; row < rows.size(); ++row) {
    SINESODA_EXPECT_NEAR(sinesoda::ui::interpolateBandValue(decibels, rows[row]), expected[row],
                         1.0e-5);
  }
}
