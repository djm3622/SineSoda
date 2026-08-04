#pragma once

#include <cstdint>
#include <deque>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "IControl.h"
#include "sinesoda/dsp/AnalysisSettings.h"
#include "sinesoda/dsp/SpectralFrame.h"
#include "sinesoda/runtime/AnalysisEngine.h"
#include "sinesoda/ui/AxisLayout.h"
#include "sinesoda/ui/SpectrogramRaster.h"

inline constexpr char kSineSodaFontId[] = "SineSodaSans";

enum class SineSodaPalette {
  Viridis = 0,
  Magma,
  Inferno,
  Turbo,
  Grayscale,
  WaveCandy,
};

enum class SineSodaDisplayScale {
  Native = 0,
  Logarithmic,
  Linear,
};

class SpectrogramControl final : public iplug::igraphics::IControl {
 public:
  SpectrogramControl(const iplug::igraphics::IRECT& bounds,
                     sinesoda::runtime::AnalysisEngine& engine);

  void Draw(iplug::igraphics::IGraphics& graphics) override;
  void OnMouseWheel(float x, float y, const iplug::igraphics::IMouseMod& modifier,
                    float delta) override;
  void OnMouseOver(float x, float y, const iplug::igraphics::IMouseMod& modifier) override;
  void OnMouseOut() override;
  void OnResize() override;

  void Tick();
  void SetDisplaySettings(SineSodaPalette palette, SineSodaDisplayScale scale, float floorDb,
                          float ceilingDb, bool paused,
                          sinesoda::dsp::TransformKind selectedTransform, double sampleRate,
                          std::size_t requestedHopSamples, double minimumFrequency,
                          double maximumFrequency);

 private:
  struct HistoryColumn {
    std::vector<float> decibels;
    std::uint64_t endSample = 0;
  };

  [[nodiscard]] float frequencyToNormalized(float frequency, std::size_t index) const noexcept;
  [[nodiscard]] float frequencyToNormalizedForAxis(float frequency) const noexcept;
  [[nodiscard]] double fractionalBandIndex(float frequency) const noexcept;
  [[nodiscard]] std::pair<float, float> analysisFrequencyRange() const noexcept;
  [[nodiscard]] sinesoda::ui::FrequencyViewport frequencyViewport() const noexcept;
  [[nodiscard]] iplug::igraphics::IRECT plotBounds() const noexcept;
  [[nodiscard]] float normalizedToY(float normalized,
                                    const iplug::igraphics::IRECT& plot) const noexcept;
  [[nodiscard]] double analysisSecondsPerColumn() const noexcept;
  [[nodiscard]] double secondsPerDisplayPixel(const iplug::igraphics::IRECT& plot) const noexcept;
  [[nodiscard]] iplug::igraphics::IColor paletteColor(float normalized) const noexcept;
  void drawSpectrogram(iplug::igraphics::IGraphics& graphics, const iplug::igraphics::IRECT& plot);
  void drawSpectrogramFallback(iplug::igraphics::IGraphics& graphics,
                               const iplug::igraphics::IRECT& plot) const;
  [[nodiscard]] bool updateSpectrogramRaster(iplug::igraphics::IGraphics& graphics,
                                             const iplug::igraphics::IRECT& plot);
  void drawPianoKeyboard(iplug::igraphics::IGraphics& graphics,
                         const iplug::igraphics::IRECT& keyboard,
                         const iplug::igraphics::IRECT& plot) const;
  void drawTimeGrid(iplug::igraphics::IGraphics& graphics, const iplug::igraphics::IRECT& plot,
                    const iplug::igraphics::IRECT& ruler) const;
  void drawColorBar(iplug::igraphics::IGraphics& graphics,
                    const iplug::igraphics::IRECT& bounds) const;
  void drawHoverReadout(iplug::igraphics::IGraphics& graphics,
                        const iplug::igraphics::IRECT& plot) const;

  sinesoda::runtime::AnalysisEngine& engine_;
  sinesoda::dsp::TransformKind transform_ = sinesoda::dsp::TransformKind::Stft;
  sinesoda::dsp::TransformKind selectedTransform_ = sinesoda::dsp::TransformKind::Stft;
  std::vector<float> centerFrequencies_;
  std::deque<HistoryColumn> history_;
  SineSodaPalette palette_ = SineSodaPalette::Viridis;
  SineSodaDisplayScale displayScale_ = SineSodaDisplayScale::Logarithmic;
  float floorDb_ = -96.0F;
  float ceilingDb_ = 0.0F;
  bool paused_ = false;
  double sampleRate_ = 48000.0;
  std::size_t requestedHopSamples_ = 512;
  float requestedMinimumFrequency_ = 20.0F;
  float requestedMaximumFrequency_ = 20000.0F;
  double viewportCenterMidiNote_ = std::numeric_limits<double>::quiet_NaN();
  float mouseX_ = 0.0F;
  float mouseY_ = 0.0F;
  std::uint64_t historyRevision_ = 1;
  std::uint64_t uploadedRevision_ = 0;
  std::uint64_t lastDroppedSamples_ = 0;
  std::uint64_t lastDroppedFrames_ = 0;
  std::string lastStatusMessage_;
  std::vector<std::uint8_t> rasterPixels_;
  std::vector<sinesoda::ui::BandInterpolation> rasterBandInterpolations_;
  std::vector<float> rasterNormalizedBands_;
  void* rasterContext_ = nullptr;
  int rasterImageId_ = -1;
  int rasterWidth_ = 0;
  int rasterHeight_ = 0;
};
