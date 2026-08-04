#include "src/plugin/SineSoda.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "IControls.h"
#include "IPlug_include_in_plug_src.h"
#include "src/plugin/SpectrogramControl.h"

using namespace iplug;
using namespace igraphics;

namespace {

constexpr int kFftSizes[]{1024, 2048, 4096, 8192};
constexpr int kHopDivisors[]{2, 4, 8, 16};
constexpr int kBinsPerOctaveValues[]{12, 24, 36, 48};

IRECT controlBounds(int tag, const IRECT& bounds) {
  IRECT content = bounds.GetPadded(-10.0F);
  const IRECT top = content.ReduceFromTop(168.0F);
  const IRECT menus(top.L, top.T, top.R, top.T + 52.0F);
  const IRECT primary(top.L, top.T + 56.0F, top.R, top.T + 108.0F);
  const IRECT secondary(top.L, top.T + 112.0F, top.R, top.B);

  switch (tag) {
    case kTransformTag:
      return menus.GetGridCell(0, 0, 1, 6).GetPadded(-3.0F);
    case kFftSizeTag:
      return menus.GetGridCell(0, 1, 1, 6).GetPadded(-3.0F);
    case kWindowTag:
      return menus.GetGridCell(0, 2, 1, 6).GetPadded(-3.0F);
    case kBinsPerOctaveTag:
      return menus.GetGridCell(0, 3, 1, 6).GetPadded(-3.0F);
    case kPaletteTag:
      return menus.GetGridCell(0, 4, 1, 6).GetPadded(-3.0F);
    case kDisplayScaleTag:
      return menus.GetGridCell(0, 5, 1, 6).GetPadded(-3.0F);
    case kHopRatioTag:
      return primary.GetGridCell(0, 0, 1, 5).GetPadded(-3.0F);
    case kBandCountTag:
      return primary.GetGridCell(0, 1, 1, 5).GetPadded(-3.0F);
    case kMinFrequencyTag:
      return primary.GetGridCell(0, 2, 1, 5).GetPadded(-3.0F);
    case kMaxFrequencyTag:
      return primary.GetGridCell(0, 3, 1, 5).GetPadded(-3.0F);
    case kQScaleTag:
      return primary.GetGridCell(0, 4, 1, 5).GetPadded(-3.0F);
    case kGammaTag:
      return secondary.GetGridCell(0, 0, 1, 4).GetPadded(-3.0F);
    case kDbFloorTag:
      return secondary.GetGridCell(0, 1, 1, 4).GetPadded(-3.0F);
    case kDbCeilingTag:
      return secondary.GetGridCell(0, 2, 1, 4).GetPadded(-3.0F);
    case kPausedTag:
      return secondary.GetGridCell(0, 3, 1, 4).GetPadded(-3.0F);
    case kSpectrogramTag:
      return content.GetPadded(0.0F, -2.0F, 0.0F, 0.0F);
    default:
      return bounds;
  }
}

IVStyle makeBaseStyle() {
  return DEFAULT_STYLE.WithColor(kBG, IColor(255, 10, 14, 23))
      .WithColor(kFG, IColor(255, 22, 29, 44))
      .WithColor(kPR, IColor(255, 116, 99, 241))
      .WithColor(kFR, IColor(255, 41, 52, 72))
      .WithColor(kHL, IColor(150, 128, 111, 247))
      .WithColor(kSH, IColor(255, 35, 44, 62))
      .WithColor(kX1, IColor(255, 116, 99, 241))
      .WithLabelText(IText(10.5F, IColor(255, 151, 166, 190), kSineSodaFontId))
      .WithValueText(IText(12.0F, IColor(255, 239, 243, 250), kSineSodaFontId))
      .WithRoundness(0.18F)
      .WithDrawShadows(false)
      .WithEmboss(false)
      .WithFrameThickness(1.0F)
      .WithWidgetFrac(0.82F);
}

IVStyle makeMenuStyle() {
  return makeBaseStyle()
      .WithColor(kFG, IColor(255, 18, 24, 37))
      .WithColor(kHL, IColor(180, 39, 48, 68))
      .WithWidgetFrac(0.92F);
}

IVStyle makeSliderStyle() {
  return makeBaseStyle()
      .WithColor(kFG, IColor(255, 231, 236, 247))
      .WithColor(kHL, IColor(255, 176, 166, 255))
      .WithColor(kSH, IColor(255, 34, 43, 60))
      .WithValueText(
          IText(11.5F, IColor(255, 239, 243, 250), kSineSodaFontId).WithVAlign(EVAlign::Bottom))
      .WithWidgetFrac(0.88F);
}

IVStyle makeToggleStyle() {
  return makeBaseStyle()
      .WithColor(kFG, IColor(255, 24, 32, 47))
      .WithColor(kPR, IColor(255, 53, 198, 154))
      .WithColor(kHL, IColor(160, 65, 214, 172))
      .WithWidgetFrac(0.84F);
}

}  // namespace

SineSoda::SineSoda(const InstanceInfo& info) : Plugin(info, MakeConfig(kNumParams, kNumPresets)) {
  analysisEngine_.setDisplayConsumerActive(false);
  GetParam(kTransform)
      ->InitEnum("Transform", 0,
                 {"STFT", "Log filterbank", "Mel", "Bark", "ERB", "CQT", "VQT", "Morlet CWT",
                  "Morse CWT", "Stockwell", "Reassigned STFT", "Synchrosqueezed CWT", "Chroma",
                  "Haar wavelet packet", "SPWVD"});
  GetParam(kFftSize)->InitEnum("FFT size", 1, {"1024", "2048", "4096", "8192"});
  GetParam(kWindow)->InitEnum("Window", 0,
                              {"Hann", "Blackman-Harris", "Hamming", "Flat top", "Rectangular"});
  GetParam(kHopRatio)->InitEnum("Speed", 2, {"1/2 FFT", "1/4 FFT", "1/8 FFT", "1/16 FFT"});
  GetParam(kBandCount)->InitInt("Bands", 192, 24, 512);
  GetParam(kBinsPerOctave)->InitEnum("Bins/octave", 1, {"12", "24", "36", "48"});
  GetParam(kMinFrequency)
      ->InitDouble("Minimum frequency", 20.0, 10.0, 2000.0, 1.0, "Hz", IParam::kFlagsNone,
                   "Analysis", IParam::ShapePowCurve(3.0));
  GetParam(kMaxFrequency)
      ->InitDouble("Maximum frequency", 20000.0, 1000.0, 24000.0, 10.0, "Hz", IParam::kFlagsNone,
                   "Analysis", IParam::ShapePowCurve(0.35));
  GetParam(kQScale)->InitDouble("Q scale", 1.0, 0.25, 4.0, 0.01, "x");
  GetParam(kGamma)->InitDouble("VQT gamma", 0.0, 0.0, 200.0, 0.1, "Hz", IParam::kFlagsNone,
                               "Analysis", IParam::ShapePowCurve(2.0));
  GetParam(kDbFloor)->InitDouble("Color floor", -96.0, -160.0, -24.0, 1.0, "dBFS");
  GetParam(kDbCeiling)->InitDouble("Color ceiling", 6.0, -24.0, 12.0, 0.5, "dBFS");
  GetParam(kPalette)->InitEnum("Palette", 0,
                               {"Viridis", "Magma", "Inferno", "Turbo", "Grayscale", "Wave Candy"});
  GetParam(kDisplayScale)->InitEnum("Display scale", 1, {"Native", "Log", "Linear"});
  GetParam(kPaused)->InitBool("Display", false, "", IParam::kFlagsNone, "Display", "Live",
                              "Paused");

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                        GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* graphics) {
    const IRECT bounds = graphics->GetBounds();
    constexpr int tags[]{kSpectrogramTag,  kTransformTag, kFftSizeTag,       kWindowTag,
                         kHopRatioTag,     kBandCountTag, kBinsPerOctaveTag, kMinFrequencyTag,
                         kMaxFrequencyTag, kQScaleTag,    kGammaTag,         kDbFloorTag,
                         kDbCeilingTag,    kPaletteTag,   kDisplayScaleTag,  kPausedTag};
    if (graphics->NControls() != 0) {
      graphics->GetBackgroundControl()->SetTargetAndDrawRECTs(bounds);
      for (const int tag : tags) {
        if (auto* control = graphics->GetControlWithTag(tag)) {
          control->SetTargetAndDrawRECTs(controlBounds(tag, bounds));
        }
      }
      return;
    }

    graphics->LoadFont(kSineSodaFontId, ROBOTO_FN);
    graphics->SetLayoutOnResize(true);
    graphics->EnableMouseOver(true);
    graphics->AttachCornerResizer(EUIResizerMode::Size, true, IColor(0, 0, 0, 0),
                                  IColor(180, 126, 104, 255), IColor(255, 126, 104, 255));
    graphics->AttachPanelBackground(IColor(255, 6, 9, 15));
    const IVStyle menuStyle = makeMenuStyle();
    const IVStyle sliderStyle = makeSliderStyle();
    const IVStyle toggleStyle = makeToggleStyle();

    graphics->AttachControl(
        new SpectrogramControl(controlBounds(kSpectrogramTag, bounds), analysisEngine_),
        kSpectrogramTag);
    graphics->AttachControl(new IVMenuButtonControl(controlBounds(kTransformTag, bounds),
                                                    kTransform, "TRANSFORM", menuStyle),
                            kTransformTag);
    graphics->AttachControl(new IVMenuButtonControl(controlBounds(kFftSizeTag, bounds), kFftSize,
                                                    "FFT SIZE", menuStyle),
                            kFftSizeTag);
    graphics->AttachControl(
        new IVMenuButtonControl(controlBounds(kWindowTag, bounds), kWindow, "WINDOW", menuStyle),
        kWindowTag);
    graphics->AttachControl(new IVMenuButtonControl(controlBounds(kBinsPerOctaveTag, bounds),
                                                    kBinsPerOctave, "BINS / OCTAVE", menuStyle),
                            kBinsPerOctaveTag);
    graphics->AttachControl(
        new IVMenuButtonControl(controlBounds(kPaletteTag, bounds), kPalette, "PALETTE", menuStyle),
        kPaletteTag);
    graphics->AttachControl(new IVMenuButtonControl(controlBounds(kDisplayScaleTag, bounds),
                                                    kDisplayScale, "DISPLAY SCALE", menuStyle),
                            kDisplayScaleTag);
    graphics->AttachControl(
        new IVMenuButtonControl(controlBounds(kHopRatioTag, bounds), kHopRatio, "SPEED", menuStyle),
        kHopRatioTag);
    const auto attachSlider = [&](int tag, int parameter, const char* label) {
      graphics->AttachControl(
          new IVSliderControl(controlBounds(tag, bounds), parameter, label, sliderStyle, true,
                              EDirection::Horizontal, DEFAULT_GEARING, 5.5F, 4.0F, true),
          tag);
    };
    attachSlider(kBandCountTag, kBandCount, "BANDS");
    attachSlider(kMinFrequencyTag, kMinFrequency, "LOW CUTOFF");
    attachSlider(kMaxFrequencyTag, kMaxFrequency, "HIGH CUTOFF");
    attachSlider(kQScaleTag, kQScale, "Q SCALE");
    attachSlider(kGammaTag, kGamma, "VQT GAMMA");
    attachSlider(kDbFloorTag, kDbFloor, "COLOR FLOOR");
    attachSlider(kDbCeilingTag, kDbCeiling, "COLOR CEILING");
    graphics->AttachControl(
        new IVSlideSwitchControl(controlBounds(kPausedTag, bounds), kPaused, "FREEZE DISPLAY",
                                 toggleStyle, false, EDirection::Horizontal),
        kPausedTag);
  };
#endif
}

#if IPLUG_DSP
void SineSoda::ProcessBlock(sample** inputs, sample** outputs, int nFrames) {
  const int inputChannels = NInChansConnected();
  const int outputChannels = NOutChansConnected();
  if (nFrames <= 0) {
    return;
  }

  for (int channel = 0; channel < outputChannels; ++channel) {
    if (inputChannels == 0) {
      std::fill_n(outputs[channel], nFrames, sample{0});
      continue;
    }
    const sample* source = inputs[std::min(channel, inputChannels - 1)];
    if (outputs[channel] != source) {
      std::memcpy(outputs[channel], source, static_cast<std::size_t>(nFrames) * sizeof(sample));
    }
  }

  if (inputChannels >= 2) {
    analysisEngine_.pushStereo(inputs[0], inputs[1], static_cast<std::size_t>(nFrames));
  } else if (inputChannels == 1) {
    analysisEngine_.pushMono(inputs[0], static_cast<std::size_t>(nFrames));
  }
}

void SineSoda::OnReset() { publishAnalysisSettings(); }

void SineSoda::OnParamChange(int parameterIndex) {
  if (parameterIndex >= kTransform && parameterIndex <= kGamma) {
    publishAnalysisSettings();
  }
}

void SineSoda::OnIdle() {
  if (auto* graphics = GetUI()) {
    if (auto* control = graphics->GetControlWithTag(kSpectrogramTag)) {
      auto* spectrogram = dynamic_cast<SpectrogramControl*>(control);
      if (spectrogram != nullptr) {
        const auto settings = currentAnalysisSettings();
        spectrogram->SetDisplaySettings(
            static_cast<SineSodaPalette>(GetParam(kPalette)->Int()),
            static_cast<SineSodaDisplayScale>(GetParam(kDisplayScale)->Int()),
            static_cast<float>(GetParam(kDbFloor)->Value()),
            static_cast<float>(GetParam(kDbCeiling)->Value()), GetParam(kPaused)->Bool(),
            static_cast<sinesoda::dsp::TransformKind>(GetParam(kTransform)->Int()),
            settings.sampleRate, settings.hopSize, settings.minFrequency, settings.maxFrequency);
        spectrogram->Tick();
      }
    }
  }
}
#endif

#if IPLUG_EDITOR
void SineSoda::OnUIOpen() {
  analysisEngine_.setDisplayConsumerActive(true);
  Plugin::OnUIOpen();
}

void SineSoda::OnUIClose() {
  analysisEngine_.setDisplayConsumerActive(false);
  Plugin::OnUIClose();
}

void SineSoda::OnParentWindowResize(int width, int height) {
  int constrainedWidth = width;
  int constrainedHeight = height;
  const bool withinLimits = ConstrainEditorResize(constrainedWidth, constrainedHeight);

  if (GetUI() != nullptr) {
    GetUI()->Resize(constrainedWidth, constrainedHeight, 1.0F, !withinLimits);
  }
}

bool SineSoda::OnHostRequestingSupportedViewConfiguration(int width, int height) {
  return ConstrainEditorResize(width, height);
}

void SineSoda::OnHostSelectedViewConfiguration(int width, int height) {
  if (GetUI() != nullptr) {
    GetUI()->Resize(width, height, 1.0F, true);
  }
}
#endif

sinesoda::dsp::AnalysisSettings SineSoda::currentAnalysisSettings() const {
  sinesoda::dsp::AnalysisSettings settings;
  settings.transform = static_cast<sinesoda::dsp::TransformKind>(GetParam(kTransform)->Int());
  settings.window = static_cast<sinesoda::dsp::WindowKind>(GetParam(kWindow)->Int());
  settings.sampleRate = GetSampleRate() > 0.0 ? GetSampleRate() : 48000.0;
  settings.fftSize = static_cast<std::size_t>(kFftSizes[GetParam(kFftSize)->Int()]);
  settings.hopSize =
      settings.fftSize / static_cast<std::size_t>(kHopDivisors[GetParam(kHopRatio)->Int()]);
  settings.bandCount = static_cast<std::size_t>(GetParam(kBandCount)->Int());
  settings.binsPerOctave = kBinsPerOctaveValues[GetParam(kBinsPerOctave)->Int()];
  const double nyquist = settings.sampleRate * 0.5;
  settings.maxFrequency = std::min(GetParam(kMaxFrequency)->Value(), nyquist);
  settings.minFrequency = std::min(GetParam(kMinFrequency)->Value(), settings.maxFrequency * 0.5);
  settings.qScale = GetParam(kQScale)->Value();
  settings.gamma = GetParam(kGamma)->Value();
  return settings;
}

void SineSoda::publishAnalysisSettings() {
  try {
    analysisEngine_.requestSettings(currentAnalysisSettings());
  } catch (...) {
    // Host-provided parameter batches can be transiently inconsistent. The
    // last validated analyzer remains active until the next notification.
  }
}
