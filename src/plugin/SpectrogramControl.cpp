#include "src/plugin/SpectrogramControl.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

#include "IGraphics.h"
#ifdef IGRAPHICS_NANOVG
#include "nanovg.h"
#endif
#include "sinesoda/dsp/FrequencyScale.h"
#include "sinesoda/ui/AxisLayout.h"

using namespace iplug::igraphics;

namespace {

struct PaletteStop {
  float position;
  IColor color;
};

template <std::size_t Size>
IColor interpolatePalette(const std::array<PaletteStop, Size>& stops, float normalized) noexcept {
  const float value = std::clamp(normalized, 0.0F, 1.0F);
  for (std::size_t index = 1; index < stops.size(); ++index) {
    if (value <= stops[index].position) {
      const float width = stops[index].position - stops[index - 1].position;
      const float fraction = width > 0.0F ? (value - stops[index - 1].position) / width : 0.0F;
      return IColor::LinearInterpolateBetween(stops[index - 1].color, stops[index].color, fraction);
    }
  }
  return stops.back().color;
}

std::string formatFrequency(float frequency) {
  char text[32]{};
  if (frequency >= 1000.0F) {
    std::snprintf(text, sizeof(text), "%.2f kHz", frequency / 1000.0F);
  } else {
    std::snprintf(text, sizeof(text), "%.1f Hz", frequency);
  }
  return text;
}

int positiveModulo(int value, int modulus) noexcept {
  const int remainder = value % modulus;
  return remainder < 0 ? remainder + modulus : remainder;
}

bool isBlackKey(int midiNote) noexcept {
  const int pitchClass = positiveModulo(midiNote, 12);
  return pitchClass == 1 || pitchClass == 3 || pitchClass == 6 || pitchClass == 8 ||
         pitchClass == 10;
}

std::string noteName(int midiNote, bool includeOctave = true) {
  constexpr std::array<std::string_view, 12> names{"C",  "C#", "D",  "D#", "E",  "F",
                                                   "F#", "G",  "G#", "A",  "A#", "B"};
  std::string result(names[static_cast<std::size_t>(positiveModulo(midiNote, 12))]);
  if (includeOctave) {
    const int octave = static_cast<int>(std::floor(static_cast<double>(midiNote) / 12.0)) - 1;
    result += std::to_string(octave);
  }
  return result;
}

std::string noteNameFromFrequency(float frequency) {
  const int midiNote = static_cast<int>(
      std::lround(sinesoda::dsp::noteNumberFromHertz(static_cast<double>(frequency))));
  return noteName(midiNote);
}

std::string formatTimeTick(double seconds, bool showSign = true) {
  char text[32]{};
  const char* sign = showSign && seconds > 0.0 ? "-" : "";
  if (seconds < 1.0) {
    std::snprintf(text, sizeof(text), "%s%.0f ms", sign, seconds * 1000.0);
  } else if (seconds < 10.0) {
    std::snprintf(text, sizeof(text), "%s%.2g s", sign, seconds);
  } else {
    std::snprintf(text, sizeof(text), "%s%.0f s", sign, seconds);
  }
  return text;
}

std::string formatDbTick(float decibels) {
  if (std::abs(decibels) < 0.05F) {
    return "0";
  }
  char text[16]{};
  const bool wholeNumber = std::abs(decibels - std::round(decibels)) < 0.05F;
  std::snprintf(text, sizeof(text), wholeNumber ? "%+.0f" : "%+.1f", decibels);
  return text;
}

}  // namespace

SpectrogramControl::SpectrogramControl(const IRECT& bounds,
                                       sinesoda::runtime::AnalysisEngine& engine)
    : IControl(bounds), engine_(engine) {}

void SpectrogramControl::Tick() {
  if (paused_) {
    return;
  }

  sinesoda::dsp::SpectralFrame frame;
  const auto maximumColumns = static_cast<std::size_t>(std::clamp(mRECT.W(), 64.0F, 1024.0F));
  std::size_t framesRead = 0;
  while (framesRead < 256 && engine_.tryPopFrame(frame)) {
    const bool nonMonotonicTime = !history_.empty() && frame.endSample <= history_.back().endSample;
    const bool geometryChanged = frame.centerFrequencies.size() != centerFrequencies_.size() ||
                                 (!frame.centerFrequencies.empty() && !centerFrequencies_.empty() &&
                                  (frame.centerFrequencies.front() != centerFrequencies_.front() ||
                                   frame.centerFrequencies.back() != centerFrequencies_.back()));
    if (frame.transform != transform_ || nonMonotonicTime || geometryChanged) {
      history_.clear();
      transform_ = frame.transform;
      centerFrequencies_ = frame.centerFrequencies;
      viewportCenterMidiNote_ = std::numeric_limits<double>::quiet_NaN();
    }

    HistoryColumn column;
    column.endSample = frame.endSample;
    column.decibels.reserve(frame.magnitudes.size());
    for (const float magnitude : frame.magnitudes) {
      column.decibels.push_back(sinesoda::dsp::amplitudeToDb(magnitude));
    }
    history_.push_back(std::move(column));
    while (history_.size() > maximumColumns) {
      history_.pop_front();
    }
    ++framesRead;
  }

  const std::string status = engine_.statusMessage();
  const std::uint64_t droppedSamples = engine_.droppedSamples();
  const std::uint64_t droppedFrames = engine_.droppedFrames();
  const bool statusChanged = status != lastStatusMessage_ ||
                             droppedSamples != lastDroppedSamples_ ||
                             droppedFrames != lastDroppedFrames_;
  lastStatusMessage_ = status;
  lastDroppedSamples_ = droppedSamples;
  lastDroppedFrames_ = droppedFrames;
  if (framesRead != 0) {
    ++historyRevision_;
  }
  if (framesRead != 0 || statusChanged) {
    SetDirty(false);
  }
}

void SpectrogramControl::SetDisplaySettings(SineSodaPalette palette, SineSodaDisplayScale scale,
                                            float floorDb, float ceilingDb, bool paused,
                                            sinesoda::dsp::TransformKind selectedTransform,
                                            double sampleRate, std::size_t requestedHopSamples,
                                            double minimumFrequency, double maximumFrequency) {
  bool displayChanged = false;
  bool rasterChanged = false;
  if (selectedTransform != selectedTransform_) {
    selectedTransform_ = selectedTransform;
    transform_ = selectedTransform;
    centerFrequencies_.clear();
    history_.clear();
    viewportCenterMidiNote_ = std::numeric_limits<double>::quiet_NaN();
    displayChanged = true;
    rasterChanged = true;
  }
  const float nextFloor = std::min(floorDb, ceilingDb - 6.0F);
  const double nextSampleRate =
      std::isfinite(sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
  const std::size_t nextHopSamples = std::max<std::size_t>(requestedHopSamples, 1);
  const float nextMinimumFrequency = std::isfinite(minimumFrequency) && minimumFrequency > 0.0
                                         ? static_cast<float>(minimumFrequency)
                                         : 20.0F;
  const float nextMaximumFrequency =
      std::isfinite(maximumFrequency) && maximumFrequency > nextMinimumFrequency
          ? static_cast<float>(maximumFrequency)
          : std::max(nextMinimumFrequency + 1.0F, 20000.0F);

  rasterChanged = rasterChanged || palette != palette_ || scale != displayScale_ ||
                  nextFloor != floorDb_ || ceilingDb != ceilingDb_;
  displayChanged = displayChanged || rasterChanged || paused != paused_ ||
                   nextSampleRate != sampleRate_ || nextHopSamples != requestedHopSamples_ ||
                   nextMinimumFrequency != requestedMinimumFrequency_ ||
                   nextMaximumFrequency != requestedMaximumFrequency_;

  palette_ = palette;
  displayScale_ = scale;
  floorDb_ = nextFloor;
  ceilingDb_ = ceilingDb;
  sampleRate_ = nextSampleRate;
  requestedHopSamples_ = nextHopSamples;
  requestedMinimumFrequency_ = nextMinimumFrequency;
  requestedMaximumFrequency_ = nextMaximumFrequency;
  if (paused != paused_) {
    paused_ = paused;
    engine_.setDisplayConsumerActive(!paused_);
  }
  if (rasterChanged) {
    ++historyRevision_;
  }
  if (displayChanged) {
    SetDirty(false);
  }
}

void SpectrogramControl::Draw(IGraphics& graphics) {
  graphics.FillRoundRect(IColor(255, 7, 10, 17), mRECT, 10.0F);
  graphics.DrawRoundRect(IColor(210, 35, 45, 62), mRECT.GetPadded(-0.5F), 10.0F, nullptr, 1.0F);

  IRECT content = mRECT.GetPadded(-9.0F);
  const IRECT header = content.ReduceFromTop(34.0F);
  const IRECT statusBounds = content.ReduceFromBottom(17.0F);
  const IRECT rulerRow = content.ReduceFromBottom(25.0F);
  const IRECT colorBarBounds = content.ReduceFromRight(58.0F).GetPadded(-8.0F, 0.0F, 0.0F, 0.0F);
  const IRECT keyboardBounds = content.ReduceFromLeft(58.0F).GetPadded(0.0F, 0.0F, -7.0F, 0.0F);
  const IRECT plot = content;
  const IRECT timeRuler(plot.L, rulerRow.T, plot.R, rulerRow.B);

  graphics.FillRoundRect(IColor(255, 12, 17, 28), header, 7.0F);
  drawSpectrogram(graphics, plot);

  drawPianoKeyboard(graphics, keyboardBounds, plot);
  drawTimeGrid(graphics, plot, timeRuler);
  drawColorBar(graphics, colorBarBounds);
  drawHoverReadout(graphics, plot);

  const auto titleTransform = history_.empty() ? selectedTransform_ : transform_;
  graphics.DrawText(IText(13.0F, IColor(255, 230, 235, 244), kSineSodaFontId, EAlign::Near),
                    std::string(sinesoda::dsp::transformName(titleTransform)).c_str(),
                    header.GetPadded(-11.0F, 0.0F, -220.0F, 0.0F));

  char cadence[80]{};
  const double displayPixelSeconds = secondsPerDisplayPixel(plot);
  const auto axisScale = sinesoda::ui::TimeAxisScale{
      displayPixelSeconds, sinesoda::ui::chooseTimeTickSeconds(displayPixelSeconds * 90.0)};
  std::snprintf(cadence, sizeof(cadence), "%.2f ms / px   |   %s / div",
                displayPixelSeconds * 1000.0,
                formatTimeTick(axisScale.majorTickSeconds, false).c_str());
  const IRECT cadenceBounds(header.L + 190.0F, header.T, header.R - 78.0F, header.B);
  graphics.DrawText(IText(10.0F, IColor(255, 126, 142, 164), kSineSodaFontId, EAlign::Far), cadence,
                    cadenceBounds);

  const IRECT stateBounds = header.GetFromRight(66.0F).GetPadded(-7.0F, -7.0F, -2.0F, -7.0F);
  const IColor stateColor = paused_ ? IColor(255, 124, 92, 246) : IColor(255, 45, 201, 151);
  graphics.FillRoundRect(stateColor, stateBounds, stateBounds.H() * 0.5F);
  graphics.DrawText(IText(9.0F, IColor(255, 250, 252, 255), kSineSodaFontId, EAlign::Center),
                    paused_ ? "PAUSED" : "LIVE", stateBounds);

  graphics.DrawRect(IColor(220, 39, 51, 69), plot, nullptr, 1.0F);
  graphics.DrawLine(IColor(230, 74, 222, 202), plot.R, plot.T, plot.R, plot.B, nullptr, 1.0F);

  std::string status = engine_.statusMessage();
  const std::uint64_t droppedSamples = engine_.droppedSamples();
  const std::uint64_t droppedFrames = engine_.droppedFrames();
  if (droppedSamples != 0) {
    if (!status.empty()) {
      status += "  |  ";
    }
    status += "audio backlog: " + std::to_string(droppedSamples) + " samples dropped";
    if (droppedFrames != 0) {
      status += ", " + std::to_string(droppedFrames) + " display frames dropped";
    }
  } else if (droppedFrames != 0 && !paused_) {
    if (!status.empty()) {
      status += "  |  ";
    }
    status += "display backlog: " + std::to_string(droppedFrames) + " frames dropped";
  }
  const bool hasWarning = !status.empty();
  if (!hasWarning) {
    status = "REAL-TIME  |  new frames enter at the right edge and move left";
    if (displayScale_ == SineSodaDisplayScale::Logarithmic) {
      status += "  |  LOG: full piano range";
    } else if (frequencyViewport().scrollable) {
      status += "  |  scroll over the plot to move through notes";
    }
  }
  graphics.DrawText(IText(9.0F, hasWarning ? IColor(255, 244, 180, 85) : IColor(255, 91, 108, 132),
                          kSineSodaFontId, EAlign::Near),
                    status.c_str(), statusBounds.GetPadded(-2.0F, 0.0F, 0.0F, 0.0F));
}

void SpectrogramControl::drawSpectrogram(IGraphics& graphics, const IRECT& plot) {
  graphics.FillRect(IColor(255, 3, 6, 11), plot);
  if (history_.empty() || centerFrequencies_.empty()) {
    return;
  }

#ifdef IGRAPHICS_NANOVG
  if (updateSpectrogramRaster(graphics, plot)) {
    auto* context = static_cast<NVGcontext*>(graphics.GetDrawContext());
    const IRECT imageBounds = plot;
    nvgSave(context);
    nvgBeginPath(context);
    nvgRect(context, imageBounds.L, imageBounds.T, imageBounds.W(), imageBounds.H());
    nvgFillPaint(context, nvgImagePattern(context, imageBounds.L, imageBounds.T, imageBounds.W(),
                                          imageBounds.H(), 0.0F, rasterImageId_, 1.0F));
    nvgFill(context);
    nvgRestore(context);
    return;
  }
#endif

  drawSpectrogramFallback(graphics, plot);
}

bool SpectrogramControl::updateSpectrogramRaster(IGraphics& graphics, const IRECT& plot) {
#ifdef IGRAPHICS_NANOVG
  auto* context = static_cast<NVGcontext*>(graphics.GetDrawContext());
  if (context == nullptr || history_.empty() || centerFrequencies_.empty()) {
    return false;
  }

  const auto dimensions = sinesoda::ui::makeRasterDimensions(plot.W(), plot.H());
  const int width = dimensions.width;
  const int height = dimensions.height;
  const bool contextChanged = rasterContext_ != context;
  const bool dimensionsChanged = rasterWidth_ != width || rasterHeight_ != height;
  if (!contextChanged && !dimensionsChanged && uploadedRevision_ == historyRevision_ &&
      rasterImageId_ > 0) {
    return true;
  }

  rasterWidth_ = width;
  rasterHeight_ = height;
  rasterPixels_.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
  rasterBandInterpolations_.resize(static_cast<std::size_t>(height));
  rasterNormalizedBands_.resize(centerFrequencies_.size());

  for (std::size_t band = 0; band < centerFrequencies_.size(); ++band) {
    rasterNormalizedBands_[band] = frequencyToNormalized(centerFrequencies_[band], band);
  }
  sinesoda::ui::makeBandInterpolations(rasterNormalizedBands_, rasterBandInterpolations_);

  constexpr std::size_t paletteResolution = 1024;
  std::array<IColor, paletteResolution> paletteLookup{};
  for (std::size_t index = 0; index < paletteLookup.size(); ++index) {
    paletteLookup[index] =
        paletteColor(static_cast<float>(index) / static_cast<float>(paletteLookup.size() - 1));
  }

  const IColor plotBackground(255, 3, 6, 11);
  for (std::size_t pixel = 0; pixel < rasterPixels_.size(); pixel += 4) {
    rasterPixels_[pixel] = static_cast<std::uint8_t>(plotBackground.R);
    rasterPixels_[pixel + 1] = static_cast<std::uint8_t>(plotBackground.G);
    rasterPixels_[pixel + 2] = static_cast<std::uint8_t>(plotBackground.B);
    rasterPixels_[pixel + 3] = 255;
  }

  const std::size_t visibleColumns = std::min(history_.size(), static_cast<std::size_t>(width));
  const std::size_t firstColumn = history_.size() - visibleColumns;
  const int firstPixelColumn = width - static_cast<int>(visibleColumns);
  const float dbRange = std::max(1.0F, ceilingDb_ - floorDb_);
  for (std::size_t columnIndex = 0; columnIndex < visibleColumns; ++columnIndex) {
    const auto& values = history_[firstColumn + columnIndex].decibels;
    if (values.empty()) {
      continue;
    }
    const int x = firstPixelColumn + static_cast<int>(columnIndex);
    for (int row = 0; row < height; ++row) {
      const std::size_t rowIndex = static_cast<std::size_t>(row);
      const float db =
          sinesoda::ui::interpolateBandValue(values, rasterBandInterpolations_[rowIndex]);
      const float normalizedDb = std::clamp((db - floorDb_) / dbRange, 0.0F, 1.0F);
      const std::size_t paletteIndex = static_cast<std::size_t>(
          std::lround(normalizedDb * static_cast<float>(paletteLookup.size() - 1)));
      const IColor& color = paletteLookup[paletteIndex];
      const std::size_t pixel =
          (rowIndex * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4U;
      rasterPixels_[pixel] = static_cast<std::uint8_t>(color.R);
      rasterPixels_[pixel + 1] = static_cast<std::uint8_t>(color.G);
      rasterPixels_[pixel + 2] = static_cast<std::uint8_t>(color.B);
      rasterPixels_[pixel + 3] = 255;
    }
  }

  if (contextChanged || dimensionsChanged || rasterImageId_ <= 0) {
    if (!contextChanged && rasterImageId_ > 0) {
      nvgDeleteImage(context, rasterImageId_);
    }
    rasterContext_ = context;
    rasterImageId_ = nvgCreateImageRGBA(context, width, height, 0, rasterPixels_.data());
  } else {
    nvgUpdateImage(context, rasterImageId_, rasterPixels_.data());
  }
  uploadedRevision_ = historyRevision_;
  return rasterImageId_ > 0;
#else
  static_cast<void>(graphics);
  static_cast<void>(plot);
  return false;
#endif
}

void SpectrogramControl::drawSpectrogramFallback(IGraphics& graphics, const IRECT& plot) const {
  const std::size_t visibleColumns =
      std::min(history_.size(), static_cast<std::size_t>(std::max(1.0F, std::floor(plot.W()))));
  const std::size_t firstColumn = history_.size() - visibleColumns;
  for (std::size_t columnIndex = 0; columnIndex < visibleColumns; ++columnIndex) {
    const auto& values = history_[firstColumn + columnIndex].decibels;
    const float xRight = plot.R - static_cast<float>(visibleColumns - 1 - columnIndex);
    const float xLeft = xRight - 1.0F;
    const std::size_t bandCount = std::min(values.size(), centerFrequencies_.size());
    for (std::size_t band = 0; band < bandCount; ++band) {
      const float current = frequencyToNormalized(centerFrequencies_[band], band);
      const float below =
          band == 0
              ? current
              : 0.5F * (frequencyToNormalized(centerFrequencies_[band - 1], band - 1) + current);
      const float above =
          band + 1 == bandCount
              ? current
              : 0.5F * (current + frequencyToNormalized(centerFrequencies_[band + 1], band + 1));
      const float visibleBelow = std::clamp(below, 0.0F, 1.0F);
      const float visibleAbove = std::clamp(above, 0.0F, 1.0F);
      if (visibleAbove <= visibleBelow) {
        continue;
      }
      const float normalizedDb = (values[band] - floorDb_) / std::max(1.0F, ceilingDb_ - floorDb_);
      graphics.FillRect(paletteColor(normalizedDb),
                        IRECT(xLeft, normalizedToY(visibleAbove, plot), xRight,
                              normalizedToY(visibleBelow, plot)));
    }
  }
}

void SpectrogramControl::OnMouseWheel(float x, float y, const IMouseMod& modifier, float delta) {
  const IRECT plot = plotBounds();
  const IRECT frequencyArea(mRECT.L + 9.0F, plot.T, plot.R, plot.B);
  if (!frequencyArea.Contains(x, y) || delta == 0.0F ||
      displayScale_ == SineSodaDisplayScale::Logarithmic ||
      transform_ == sinesoda::dsp::TransformKind::Chroma) {
    return;
  }

  const auto current = frequencyViewport();
  if (!current.scrollable) {
    return;
  }
  const double notesPerStep = modifier.S ? 1.0 : 3.0;
  const double requestedCenter = current.centerMidiNote + static_cast<double>(delta) * notesPerStep;
  const auto [lowFrequency, highFrequency] = analysisFrequencyRange();
  const auto next = sinesoda::ui::makeFrequencyViewport(lowFrequency, highFrequency, plot.H(),
                                                        false, requestedCenter);
  if (next.centerMidiNote == current.centerMidiNote) {
    return;
  }
  viewportCenterMidiNote_ = next.centerMidiNote;
  ++historyRevision_;
  SetDirty(false);
}

void SpectrogramControl::OnMouseOver(float x, float y, const IMouseMod& modifier) {
  IControl::OnMouseOver(x, y, modifier);
  mouseX_ = x;
  mouseY_ = y;
  SetDirty(false);
}

void SpectrogramControl::OnMouseOut() {
  IControl::OnMouseOut();
  SetDirty(false);
}

void SpectrogramControl::OnResize() {
  ++historyRevision_;
  uploadedRevision_ = 0;
  SetDirty(false);
}

float SpectrogramControl::frequencyToNormalized(float frequency, std::size_t index) const noexcept {
  if (centerFrequencies_.size() < 2) {
    return 0.5F;
  }
  if (transform_ == sinesoda::dsp::TransformKind::Chroma) {
    return static_cast<float>(index) / static_cast<float>(centerFrequencies_.size() - 1);
  }

  const auto viewport = frequencyViewport();
  const float low = static_cast<float>(viewport.lowHertz);
  const float high = static_cast<float>(viewport.highHertz);
  if (displayScale_ == SineSodaDisplayScale::Logarithmic) {
    return std::log(std::max(frequency, std::numeric_limits<float>::min()) / low) /
           std::log(high / low);
  }
  if (displayScale_ == SineSodaDisplayScale::Native) {
    const double lowerIndex = fractionalBandIndex(low);
    const double upperIndex = fractionalBandIndex(high);
    return static_cast<float>((static_cast<double>(index) - lowerIndex) /
                              std::max(upperIndex - lowerIndex, 1.0e-12));
  }
  return (frequency - low) / (high - low);
}

float SpectrogramControl::frequencyToNormalizedForAxis(float frequency) const noexcept {
  const auto viewport = frequencyViewport();
  const float low = static_cast<float>(viewport.lowHertz);
  const float high = static_cast<float>(viewport.highHertz);
  if (displayScale_ == SineSodaDisplayScale::Logarithmic) {
    return std::log(std::max(frequency, std::numeric_limits<float>::min()) / low) /
           std::log(high / low);
  }
  if (displayScale_ == SineSodaDisplayScale::Linear) {
    return (frequency - low) / (high - low);
  }

  if (centerFrequencies_.size() < 2) {
    return std::log(std::max(frequency, std::numeric_limits<float>::min()) / low) /
           std::log(high / low);
  }

  const double lowerIndex = fractionalBandIndex(low);
  const double upperIndex = fractionalBandIndex(high);
  return static_cast<float>((fractionalBandIndex(frequency) - lowerIndex) /
                            std::max(upperIndex - lowerIndex, 1.0e-12));
}

double SpectrogramControl::fractionalBandIndex(float frequency) const noexcept {
  if (centerFrequencies_.size() < 2) {
    return 0.0;
  }
  const auto upper =
      std::lower_bound(centerFrequencies_.begin(), centerFrequencies_.end(), frequency);
  if (upper == centerFrequencies_.begin()) {
    return 0.0F;
  }
  if (upper == centerFrequencies_.end()) {
    return static_cast<double>(centerFrequencies_.size() - 1);
  }
  const std::size_t upperIndex = static_cast<std::size_t>(upper - centerFrequencies_.begin());
  const float lowerFrequency = centerFrequencies_[upperIndex - 1];
  const float upperFrequency = centerFrequencies_[upperIndex];
  const float fraction =
      (frequency - lowerFrequency) / std::max(upperFrequency - lowerFrequency, 1.0e-12F);
  return static_cast<double>(upperIndex - 1) + static_cast<double>(fraction);
}

std::pair<float, float> SpectrogramControl::analysisFrequencyRange() const noexcept {
  const float low = centerFrequencies_.empty() ? requestedMinimumFrequency_
                                               : std::max(centerFrequencies_.front(), 1.0F);
  const float high = centerFrequencies_.empty() ? std::max(requestedMaximumFrequency_, low + 1.0F)
                                                : std::max(centerFrequencies_.back(), low + 1.0F);
  return {low, high};
}

sinesoda::ui::FrequencyViewport SpectrogramControl::frequencyViewport() const noexcept {
  const auto [lowFrequency, highFrequency] = analysisFrequencyRange();
  const bool fitFullRange = displayScale_ == SineSodaDisplayScale::Logarithmic ||
                            transform_ == sinesoda::dsp::TransformKind::Chroma;
  return sinesoda::ui::makeFrequencyViewport(lowFrequency, highFrequency, plotBounds().H(),
                                             fitFullRange, viewportCenterMidiNote_);
}

IRECT SpectrogramControl::plotBounds() const noexcept {
  IRECT content = mRECT.GetPadded(-9.0F);
  content.ReduceFromTop(34.0F);
  content.ReduceFromBottom(17.0F);
  content.ReduceFromBottom(25.0F);
  content.ReduceFromRight(58.0F);
  content.ReduceFromLeft(58.0F);
  return content;
}

float SpectrogramControl::normalizedToY(float normalized, const IRECT& plot) const noexcept {
  return plot.B - std::clamp(normalized, 0.0F, 1.0F) * plot.H();
}

double SpectrogramControl::analysisSecondsPerColumn() const noexcept {
  if (history_.size() >= 2 && sampleRate_ > 0.0) {
    const std::uint64_t newest = history_.back().endSample;
    for (auto previous = history_.rbegin() + 1; previous != history_.rend(); ++previous) {
      if (previous->endSample < newest) {
        return static_cast<double>(newest - previous->endSample) / sampleRate_;
      }
    }
  }
  return sinesoda::ui::makeTimeAxisScale(sampleRate_, requestedHopSamples_).secondsPerColumn;
}

double SpectrogramControl::secondsPerDisplayPixel(const IRECT& plot) const noexcept {
  const auto dimensions = sinesoda::ui::makeRasterDimensions(plot.W(), plot.H());
  return sinesoda::ui::rasterSecondsPerDisplayPixel(analysisSecondsPerColumn(), plot.W(),
                                                    dimensions.width);
}

IColor SpectrogramControl::paletteColor(float normalized) const noexcept {
  static const std::array<PaletteStop, 5> viridis{{
      {0.0F, IColor(255, 68, 1, 84)},
      {0.25F, IColor(255, 59, 82, 139)},
      {0.5F, IColor(255, 33, 145, 140)},
      {0.75F, IColor(255, 94, 201, 98)},
      {1.0F, IColor(255, 253, 231, 37)},
  }};
  static const std::array<PaletteStop, 5> magma{{
      {0.0F, IColor(255, 0, 0, 4)},
      {0.25F, IColor(255, 81, 18, 124)},
      {0.5F, IColor(255, 183, 55, 121)},
      {0.75F, IColor(255, 252, 137, 97)},
      {1.0F, IColor(255, 252, 253, 191)},
  }};
  static const std::array<PaletteStop, 5> inferno{{
      {0.0F, IColor(255, 0, 0, 4)},
      {0.25F, IColor(255, 87, 16, 110)},
      {0.5F, IColor(255, 188, 55, 84)},
      {0.75F, IColor(255, 249, 142, 8)},
      {1.0F, IColor(255, 252, 255, 164)},
  }};
  static const std::array<PaletteStop, 6> turbo{{
      {0.0F, IColor(255, 48, 18, 59)},
      {0.2F, IColor(255, 50, 119, 227)},
      {0.4F, IColor(255, 41, 207, 131)},
      {0.6F, IColor(255, 199, 221, 47)},
      {0.8F, IColor(255, 245, 96, 24)},
      {1.0F, IColor(255, 122, 4, 3)},
  }};
  static const std::array<PaletteStop, 2> grayscale{{
      {0.0F, IColor(255, 3, 6, 12)},
      {1.0F, IColor(255, 250, 250, 250)},
  }};
  static const std::array<PaletteStop, 6> waveCandy{{
      {0.0F, IColor(255, 0, 0, 0)},
      {0.2F, IColor(255, 15, 19, 89)},
      {0.4F, IColor(255, 0, 171, 203)},
      {0.6F, IColor(255, 73, 246, 75)},
      {0.8F, IColor(255, 255, 220, 0)},
      {1.0F, IColor(255, 255, 48, 22)},
  }};

  switch (palette_) {
    case SineSodaPalette::Viridis:
      return interpolatePalette(viridis, normalized);
    case SineSodaPalette::Magma:
      return interpolatePalette(magma, normalized);
    case SineSodaPalette::Inferno:
      return interpolatePalette(inferno, normalized);
    case SineSodaPalette::Turbo:
      return interpolatePalette(turbo, normalized);
    case SineSodaPalette::Grayscale:
      return interpolatePalette(grayscale, normalized);
    case SineSodaPalette::WaveCandy:
      return interpolatePalette(waveCandy, normalized);
  }
  return COLOR_BLACK;
}

void SpectrogramControl::drawPianoKeyboard(IGraphics& graphics, const IRECT& keyboard,
                                           const IRECT& plot) const {
  graphics.FillRect(IColor(255, 13, 18, 29), keyboard);
  if (transform_ == sinesoda::dsp::TransformKind::Chroma) {
    for (int pitchClass = 0; pitchClass < 12; ++pitchClass) {
      const float lower = static_cast<float>(pitchClass) / 12.0F;
      const float upper = static_cast<float>(pitchClass + 1) / 12.0F;
      const float top = normalizedToY(upper, plot);
      const float bottom = normalizedToY(lower, plot);
      const bool black = isBlackKey(pitchClass);
      const IRECT row = black ? IRECT(keyboard.L, top, keyboard.L + keyboard.W() * 0.68F, bottom)
                              : IRECT(keyboard.L, top, keyboard.R, bottom);
      graphics.FillRect(black ? IColor(255, 25, 31, 43) : IColor(255, 211, 217, 226), row);
      graphics.DrawRect(IColor(180, 45, 54, 70), row, nullptr, 0.8F);
      graphics.DrawLine(IColor(42, 168, 187, 209), plot.L, top, plot.R, top, nullptr, 1.0F);
      graphics.DrawText(IText(9.0F, black ? IColor(255, 232, 237, 245) : IColor(255, 28, 35, 48),
                              kSineSodaFontId, EAlign::Far),
                        noteName(pitchClass, false).c_str(),
                        IRECT(keyboard.L, top, keyboard.R - 5.0F, bottom));
    }
  } else {
    const auto [lowFrequencyValue, highFrequencyValue] = analysisFrequencyRange();
    const double lowFrequency = lowFrequencyValue;
    const double highFrequency = highFrequencyValue;
    const int firstNote = std::max(
        -24, static_cast<int>(std::floor(sinesoda::dsp::noteNumberFromHertz(lowFrequency))) - 1);
    const int lastNote = std::min(
        160, static_cast<int>(std::ceil(sinesoda::dsp::noteNumberFromHertz(highFrequency))) + 1);

    for (int midiNote = firstNote; midiNote <= lastNote; ++midiNote) {
      const float lowerFrequency = static_cast<float>(
          sinesoda::dsp::hertzFromNoteNumber(static_cast<double>(midiNote) - 0.5));
      const float upperFrequency = static_cast<float>(
          sinesoda::dsp::hertzFromNoteNumber(static_cast<double>(midiNote) + 0.5));
      const float top = normalizedToY(frequencyToNormalizedForAxis(upperFrequency), plot);
      const float bottom = normalizedToY(frequencyToNormalizedForAxis(lowerFrequency), plot);
      if (bottom <= plot.T || top >= plot.B || bottom - top < 0.25F) {
        continue;
      }

      const float visibleTop = std::max(top, plot.T);
      const float visibleBottom = std::min(bottom, plot.B);

      const bool black = isBlackKey(midiNote);
      const IRECT row =
          black ? IRECT(keyboard.L, visibleTop, keyboard.L + keyboard.W() * 0.68F, visibleBottom)
                : IRECT(keyboard.L, visibleTop, keyboard.R, visibleBottom);
      graphics.FillRect(black ? IColor(255, 25, 31, 43) : IColor(255, 211, 217, 226), row);
      graphics.DrawRect(IColor(180, 45, 54, 70), row, nullptr, 0.8F);

      const bool octaveBoundary = positiveModulo(midiNote, 12) == 0;
      const float centerY = normalizedToY(frequencyToNormalizedForAxis(static_cast<float>(
                                              sinesoda::dsp::hertzFromNoteNumber(midiNote))),
                                          plot);
      graphics.DrawLine(octaveBoundary ? IColor(78, 126, 157, 190) : IColor(25, 151, 174, 199),
                        plot.L, centerY, plot.R, centerY, nullptr, 1.0F);

      if (octaveBoundary) {
        graphics.DrawText(IText(7.0F, IColor(255, 31, 39, 52), kSineSodaFontId, EAlign::Far),
                          noteName(midiNote).c_str(),
                          IRECT(keyboard.L, centerY - 5.0F, keyboard.R - 4.0F, centerY + 5.0F));
      }
    }

    const auto viewport = frequencyViewport();
    if (viewport.scrollable) {
      const double totalLowNote = sinesoda::ui::midiNoteFromHertz(lowFrequency);
      const double totalHighNote = sinesoda::ui::midiNoteFromHertz(highFrequency);
      const double viewportLowNote = sinesoda::ui::midiNoteFromHertz(viewport.lowHertz);
      const double viewportHighNote = sinesoda::ui::midiNoteFromHertz(viewport.highHertz);
      const double totalSpan = std::max(totalHighNote - totalLowNote, 1.0e-12);
      const float thumbTop =
          plot.B - static_cast<float>((viewportHighNote - totalLowNote) / totalSpan) * plot.H();
      const float thumbBottom =
          plot.B - static_cast<float>((viewportLowNote - totalLowNote) / totalSpan) * plot.H();
      graphics.FillRoundRect(IColor(220, 74, 222, 202),
                             IRECT(keyboard.L + 1.0F, thumbTop, keyboard.L + 4.0F, thumbBottom),
                             1.5F);
    }
  }

  graphics.DrawRect(IColor(220, 42, 52, 69), keyboard, nullptr, 1.0F);
  graphics.DrawLine(IColor(220, 51, 65, 85), keyboard.R, keyboard.T, keyboard.R, keyboard.B,
                    nullptr, 1.0F);
}

void SpectrogramControl::drawTimeGrid(IGraphics& graphics, const IRECT& plot,
                                      const IRECT& ruler) const {
  graphics.FillRect(IColor(255, 10, 14, 23), ruler);
  const double columnSeconds = std::max(secondsPerDisplayPixel(plot), 1.0e-9);
  const double majorSeconds = sinesoda::ui::chooseTimeTickSeconds(columnSeconds * 90.0);
  const double minorSeconds = majorSeconds * 0.5;
  const double visibleSeconds = static_cast<double>(plot.W()) * columnSeconds;

  for (double age = minorSeconds; age <= visibleSeconds + 1.0e-9; age += minorSeconds) {
    const float x = plot.R - static_cast<float>(age / columnSeconds);
    graphics.DrawLine(IColor(18, 160, 182, 207), x, plot.T, x, plot.B, nullptr, 1.0F);
  }

  const IText labelText(9.0F, IColor(255, 112, 128, 151), kSineSodaFontId, EAlign::Center);
  for (double age = 0.0; age <= visibleSeconds + 1.0e-9; age += majorSeconds) {
    const float x = plot.R - static_cast<float>(age / columnSeconds);
    graphics.DrawLine(IColor(58, 169, 191, 214), x, plot.T, x, plot.B, nullptr, 1.0F);
    graphics.DrawLine(IColor(210, 92, 113, 140), x, ruler.T, x, ruler.T + 5.0F, nullptr, 1.0F);
    const std::string label = age == 0.0 ? "NOW" : formatTimeTick(age);
    const float labelCenter = std::clamp(x, plot.L + 34.0F, plot.R - 25.0F);
    graphics.DrawText(labelText, label.c_str(),
                      IRECT(labelCenter - 34.0F, ruler.T + 4.0F, labelCenter + 34.0F, ruler.B));
  }

  graphics.DrawLine(IColor(220, 42, 52, 69), ruler.L, ruler.T, ruler.R, ruler.T, nullptr, 1.0F);
}

void SpectrogramControl::drawColorBar(IGraphics& graphics, const IRECT& bounds) const {
  graphics.DrawText(IText(8.5F, IColor(255, 132, 148, 173), kSineSodaFontId, EAlign::Near),
                    "LEVEL / dBFS", bounds.GetFromTop(18.0F));
  const IRECT gradient(bounds.L, bounds.T + 23.0F, bounds.L + 13.0F, bounds.B - 7.0F);
  constexpr int segments = 96;
  for (int segment = 0; segment < segments; ++segment) {
    const float low = static_cast<float>(segment) / static_cast<float>(segments);
    const float high = static_cast<float>(segment + 1) / static_cast<float>(segments);
    graphics.FillRect(paletteColor(1.0F - low),
                      IRECT(gradient.L, gradient.T + low * gradient.H(), gradient.R,
                            gradient.T + high * gradient.H()));
  }
  graphics.DrawRect(IColor(210, 87, 102, 128), gradient);
  const IText labelText(9.5F, IColor(255, 174, 188, 210), kSineSodaFontId, EAlign::Near);
  for (int index = 0; index <= 4; ++index) {
    const float fraction = static_cast<float>(index) / 4.0F;
    const float db = ceilingDb_ - fraction * (ceilingDb_ - floorDb_);
    const float y = gradient.T + fraction * gradient.H();
    graphics.DrawLine(IColor(190, 108, 124, 150), gradient.R, y, gradient.R + 4.0F, y, nullptr,
                      1.0F);
    graphics.DrawText(labelText, formatDbTick(db).c_str(),
                      IRECT(gradient.R + 6.0F, y - 8.0F, bounds.R, y + 8.0F));
  }
  if (floorDb_ < 0.0F && ceilingDb_ > 0.0F) {
    const float zeroFraction = ceilingDb_ / (ceilingDb_ - floorDb_);
    const float zeroY = gradient.T + zeroFraction * gradient.H();
    graphics.DrawLine(IColor(235, 241, 245, 252), gradient.L - 2.0F, zeroY, gradient.R + 4.0F,
                      zeroY, nullptr, 1.0F);
  }
}

void SpectrogramControl::drawHoverReadout(IGraphics& graphics, const IRECT& plot) const {
  if (!mMouseIsOver || centerFrequencies_.empty() || history_.empty() ||
      !plot.Contains(mouseX_, mouseY_)) {
    return;
  }

  const float normalizedY = std::clamp((plot.B - mouseY_) / plot.H(), 0.0F, 1.0F);
  std::size_t nearestBand = 0;
  float nearestDistance = std::numeric_limits<float>::max();
  for (std::size_t band = 0; band < centerFrequencies_.size(); ++band) {
    const float distance =
        std::abs(frequencyToNormalized(centerFrequencies_[band], band) - normalizedY);
    if (distance < nearestDistance) {
      nearestDistance = distance;
      nearestBand = band;
    }
  }

  const std::size_t visibleColumns =
      std::min(history_.size(), static_cast<std::size_t>(std::max(1.0F, std::floor(plot.W()))));
  const auto dimensions = sinesoda::ui::makeRasterDimensions(plot.W(), plot.H());
  const float fromRight =
      sinesoda::ui::rasterColumnOffsetFromRight(mouseX_, plot.L, plot.R, dimensions.width);
  const std::size_t offset = static_cast<std::size_t>(std::floor(fromRight));
  if (offset >= visibleColumns) {
    return;
  }
  const auto& column = history_[history_.size() - 1 - offset].decibels;
  if (nearestBand >= column.size()) {
    return;
  }

  graphics.DrawLine(IColor(175, 77, 224, 203), mouseX_, plot.T, mouseX_, plot.B);
  graphics.DrawLine(IColor(175, 77, 224, 203), plot.L, mouseY_, plot.R, mouseY_);
  char readout[128]{};
  const std::string pitch = transform_ == sinesoda::dsp::TransformKind::Chroma
                                ? noteName(static_cast<int>(nearestBand), false)
                                : noteNameFromFrequency(centerFrequencies_[nearestBand]);
  const std::string frequency = formatFrequency(centerFrequencies_[nearestBand]);
  const double ageSeconds = static_cast<double>(offset) * analysisSecondsPerColumn();
  std::snprintf(readout, sizeof(readout), "%s  ·  %s  |  %.1f dBFS  |  %s", pitch.c_str(),
                frequency.c_str(), column[nearestBand],
                ageSeconds < 0.0005 ? "NOW" : formatTimeTick(ageSeconds).c_str());
  constexpr float labelWidth = 288.0F;
  const float left = std::clamp(mouseX_ + 12.0F, plot.L + 4.0F, plot.R - labelWidth - 4.0F);
  const float top = std::max(plot.T + 5.0F, mouseY_ - 30.0F);
  const IRECT labelBounds(left, top, left + labelWidth, top + 24.0F);
  graphics.FillRoundRect(IColor(242, 14, 20, 31), labelBounds, 6.0F);
  graphics.DrawRoundRect(IColor(190, 57, 78, 99), labelBounds, 6.0F, nullptr, 1.0F);
  graphics.DrawText(IText(10.0F, IColor(255, 235, 239, 247), kSineSodaFontId, EAlign::Center),
                    readout, labelBounds);
}
