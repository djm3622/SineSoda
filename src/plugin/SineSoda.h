#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "sinesoda/dsp/AnalysisSettings.h"
#include "sinesoda/runtime/AnalysisEngine.h"

constexpr int kNumPresets = 1;

enum ESineSodaParams {
  kTransform = 0,
  kFftSize,
  kWindow,
  kHopRatio,
  kBandCount,
  kBinsPerOctave,
  kMinFrequency,
  kMaxFrequency,
  kQScale,
  kGamma,
  kDbFloor,
  kDbCeiling,
  kPalette,
  kDisplayScale,
  kPaused,
  kNumParams,
};

enum ESineSodaControlTags {
  kSpectrogramTag = 100,
  kTransformTag,
  kFftSizeTag,
  kWindowTag,
  kHopRatioTag,
  kBandCountTag,
  kBinsPerOctaveTag,
  kMinFrequencyTag,
  kMaxFrequencyTag,
  kQScaleTag,
  kGammaTag,
  kDbFloorTag,
  kDbCeilingTag,
  kPaletteTag,
  kDisplayScaleTag,
  kPausedTag,
};

class SineSoda final : public iplug::Plugin {
 public:
  explicit SineSoda(const iplug::InstanceInfo& info);

#if IPLUG_DSP
  void ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames) override;
  void OnReset() override;
  void OnParamChange(int parameterIndex) override;
  void OnIdle() override;
#endif

#if IPLUG_EDITOR
  void OnUIOpen() override;
  void OnUIClose() override;
  void OnParentWindowResize(int width, int height) override;
  bool OnHostRequestingSupportedViewConfiguration(int width, int height) override;
  void OnHostSelectedViewConfiguration(int width, int height) override;
#endif

 private:
  [[nodiscard]] sinesoda::dsp::AnalysisSettings currentAnalysisSettings() const;
  void publishAnalysisSettings();

  sinesoda::runtime::AnalysisEngine analysisEngine_;
};
