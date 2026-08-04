#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sinesoda/dsp/AnalysisSettings.h"
#include "sinesoda/dsp/FrequencyScale.h"
#include "sinesoda/dsp/SpectralFrame.h"
#include "sinesoda/dsp/StreamingAnalyzer.h"
#include "sinesoda/dsp/Transforms.h"

namespace {

using sinesoda::dsp::AnalysisSettings;
using sinesoda::dsp::FrequencyScale;
using sinesoda::dsp::SpectralFrame;
using sinesoda::dsp::StreamingAnalyzer;
using sinesoda::dsp::TransformKind;

constexpr double kSampleRate = 48000.0;
constexpr std::size_t kFftSize = 8192;
constexpr std::size_t kHopSize = 1024;
constexpr std::size_t kHistoryFrames = 96;
constexpr std::size_t kAnimationFrames = 96;
// Matches the native spectrogram viewport in the 1100 x 700 plugin layout.
// Exporting at the destination aspect ratio avoids transform-specific stretching
// when frames are composited into the captured FL Studio-hosted interface.
constexpr int kCanvasWidth = 946;
constexpr int kCanvasHeight = 420;
constexpr double kMinFrequency = 20.0;
constexpr double kMaxFrequency = 20000.0;

constexpr std::array kTransforms{
    TransformKind::Stft,
    TransformKind::LogFilterbank,
    TransformKind::Mel,
    TransformKind::Bark,
    TransformKind::Erb,
    TransformKind::Cqt,
    TransformKind::Vqt,
    TransformKind::MorletCwt,
    TransformKind::MorseCwt,
    TransformKind::Stockwell,
    TransformKind::ReassignedStft,
    TransformKind::SynchrosqueezedCwt,
    TransformKind::Chroma,
    TransformKind::HaarWaveletPacket,
    TransformKind::SmoothedPseudoWignerVille,
};

constexpr std::array<std::string_view, kTransforms.size()> kSlugs{
    "stft",
    "log-filterbank",
    "mel",
    "bark",
    "erb",
    "cqt",
    "vqt",
    "morlet-cwt",
    "morse-cwt",
    "stockwell",
    "reassigned-stft",
    "synchrosqueezed-cwt",
    "chroma",
    "haar-wavelet-packet",
    "smoothed-pseudo-wigner-ville",
};

enum class SignalKind { HarmonicResolution, PitchStructure, TransientMotion };

struct NoteEvent {
  double start = 0.0;
  double duration = 0.0;
  int midiNote = 60;
  double velocity = 1.0;
};

struct Rgb {
  unsigned char red = 0;
  unsigned char green = 0;
  unsigned char blue = 0;
};

struct Image {
  int width = 1;
  int height = 1;
  std::vector<Rgb> pixels;

  Image(int imageWidth, int imageHeight, Rgb background)
      : width(imageWidth),
        height(imageHeight),
        pixels(static_cast<std::size_t>(imageWidth * imageHeight), background) {}

  void set(int x, int y, Rgb color) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
      pixels[static_cast<std::size_t>(y * width + x)] = color;
    }
  }
};

double midiToHertz(int note) {
  return 440.0 * std::pow(2.0, (static_cast<double>(note) - 69.0) / 12.0);
}

std::vector<NoteEvent> makeNoteEvents(SignalKind kind, double duration) {
  std::vector<NoteEvent> events;
  const double usableDuration = std::max(duration, 1.0);
  if (kind == SignalKind::HarmonicResolution) {
    constexpr std::array<std::array<int, 6>, 4> chords{{
        {{36, 43, 48, 52, 59, 62}},
        {{38, 45, 48, 53, 57, 64}},
        {{29, 36, 41, 45, 52, 55}},
        {{31, 38, 41, 47, 52, 57}},
    }};
    const double chordDuration = usableDuration / static_cast<double>(chords.size());
    for (std::size_t chord = 0; chord < chords.size(); ++chord) {
      for (std::size_t voice = 0; voice < chords[chord].size(); ++voice) {
        events.push_back({chordDuration * static_cast<double>(chord), chordDuration * 0.91,
                          chords[chord][voice], 0.58 + 0.07 * static_cast<double>(voice)});
      }
    }
  } else if (kind == SignalKind::PitchStructure) {
    constexpr std::array<std::array<int, 5>, 4> chords{{
        {{48, 52, 55, 59, 62}},
        {{45, 48, 52, 55, 59}},
        {{41, 45, 48, 52, 55}},
        {{43, 47, 50, 53, 57}},
    }};
    const double step = usableDuration / 20.0;
    for (std::size_t chord = 0; chord < chords.size(); ++chord) {
      for (std::size_t voice = 0; voice < chords[chord].size(); ++voice) {
        const double start = step * static_cast<double>(chord * 5 + voice);
        events.push_back({start, step * 1.42, chords[chord][voice],
                          0.66 + 0.06 * static_cast<double>((voice + chord) % 4)});
      }
    }
  } else {
    const double step = usableDuration / 24.0;
    for (int index = 0; index < 24; ++index) {
      const int ascending = 36 + index;
      const int note = index % 6 == 5 ? ascending + 12 : ascending;
      const double noteDuration = index % 4 == 0 ? step * 0.82 : step * 0.34;
      events.push_back(
          {step * static_cast<double>(index), noteDuration, note, index % 3 == 0 ? 1.0 : 0.72});
      if (index % 6 == 0) {
        events.push_back({step * static_cast<double>(index), step * 0.18, note + 19, 0.82});
      }
    }
  }
  return events;
}

std::vector<float> synthesizeEvents(std::size_t size, const std::vector<NoteEvent>& events) {
  std::vector<float> signal(size, 0.0F);
  constexpr double twoPi = 2.0 * std::numbers::pi_v<double>;
  for (const auto& event : events) {
    const std::size_t first =
        std::min(size, static_cast<std::size_t>(std::max(event.start, 0.0) * kSampleRate));
    const std::size_t last =
        std::min(size, static_cast<std::size_t>((event.start + event.duration) * kSampleRate));
    const double fundamental = midiToHertz(event.midiNote);
    for (std::size_t sample = first; sample < last; ++sample) {
      const double localTime = static_cast<double>(sample - first) / kSampleRate;
      const double remaining = static_cast<double>(last - sample) / kSampleRate;
      const double attack = std::min(localTime / 0.012, 1.0);
      const double release = std::min(remaining / std::min(0.11, event.duration * 0.35), 1.0);
      const double envelope = attack * release;
      double value = 0.0;
      for (int harmonic = 1; harmonic <= 7; ++harmonic) {
        const double frequency = fundamental * static_cast<double>(harmonic);
        if (frequency >= kSampleRate * 0.49) {
          break;
        }
        const double amplitude = 1.0 / std::pow(static_cast<double>(harmonic), 1.28);
        value += amplitude * std::sin(twoPi * frequency * localTime + 0.11 * harmonic);
      }
      signal[sample] += static_cast<float>(0.075 * event.velocity * envelope * value);
    }
  }
  float peak = 0.0F;
  for (const float sample : signal) {
    peak = std::max(peak, std::abs(sample));
  }
  if (peak > 0.92F) {
    const float scale = 0.92F / peak;
    for (float& sample : signal) {
      sample *= scale;
    }
  }
  return signal;
}

std::vector<float> synthesizeLoopingSignal(std::size_t size, SignalKind kind,
                                           std::size_t loopStartSample, std::size_t loopSize) {
  const double loopStart = static_cast<double>(loopStartSample) / kSampleRate;
  const double loopDuration = static_cast<double>(loopSize) / kSampleRate;
  const auto loopEvents = makeNoteEvents(kind, loopDuration);
  std::vector<NoteEvent> events;
  events.reserve(loopEvents.size() * 2);
  for (int cycle = -1; cycle <= 0; ++cycle) {
    for (const auto& event : loopEvents) {
      auto shifted = event;
      shifted.start += loopStart + static_cast<double>(cycle) * loopDuration;
      events.push_back(shifted);
    }
  }
  return synthesizeEvents(size, events);
}

std::vector<SpectralFrame> analyze(const AnalysisSettings& settings,
                                   std::span<const float> signal) {
  StreamingAnalyzer analyzer(settings);
  std::vector<SpectralFrame> frames;
  std::size_t offset = 0;
  while (offset < signal.size()) {
    const std::size_t count = std::min<std::size_t>(512, signal.size() - offset);
    analyzer.push(signal.subspan(offset, count),
                  [&](const SpectralFrame& frame) { frames.push_back(frame); });
    offset += count;
  }
  if (frames.empty()) {
    throw std::runtime_error("transform did not emit visual frames");
  }
  return frames;
}

AnalysisSettings makeSettings(TransformKind transform) {
  AnalysisSettings settings;
  settings.transform = transform;
  settings.sampleRate = kSampleRate;
  settings.fftSize = kFftSize;
  settings.hopSize = kHopSize;
  settings.bandCount = 384;
  settings.minFrequency = kMinFrequency;
  settings.maxFrequency = kMaxFrequency;
  settings.binsPerOctave = 48;
  settings.gamma = transform == TransformKind::Vqt ? 20.0 : 0.0;
  return settings;
}

std::pair<AnalysisSettings, AnalysisSettings> makeParameterSweepSettings(TransformKind transform) {
  AnalysisSettings low = makeSettings(transform);
  AnalysisSettings high = low;
  switch (transform) {
    case TransformKind::Stft:
    case TransformKind::ReassignedStft:
    case TransformKind::Chroma:
    case TransformKind::SmoothedPseudoWignerVille:
      low.fftSize = 1024;
      high.fftSize = 8192;
      break;
    case TransformKind::LogFilterbank:
    case TransformKind::Mel:
    case TransformKind::Bark:
    case TransformKind::Erb:
    case TransformKind::HaarWaveletPacket:
      low.bandCount = 24;
      high.bandCount = 512;
      break;
    case TransformKind::Cqt:
      low.binsPerOctave = 12;
      high.binsPerOctave = 48;
      break;
    case TransformKind::Vqt:
      low.gamma = 0.0;
      high.gamma = 200.0;
      break;
    case TransformKind::MorletCwt:
    case TransformKind::MorseCwt:
    case TransformKind::Stockwell:
    case TransformKind::SynchrosqueezedCwt:
      low.qScale = 0.25;
      high.qScale = 4.0;
      break;
  }
  return {low, high};
}

SignalKind signalForTransform(TransformKind transform) {
  switch (transform) {
    case TransformKind::Cqt:
      return SignalKind::HarmonicResolution;
    case TransformKind::Vqt:
    case TransformKind::Chroma:
      return SignalKind::PitchStructure;
    case TransformKind::MorletCwt:
    case TransformKind::MorseCwt:
    case TransformKind::Stockwell:
    case TransformKind::ReassignedStft:
    case TransformKind::SynchrosqueezedCwt:
    case TransformKind::HaarWaveletPacket:
    case TransformKind::SmoothedPseudoWignerVille:
      return SignalKind::TransientMotion;
    default:
      return SignalKind::HarmonicResolution;
  }
}

Rgb viridis(float normalized) {
  constexpr std::array<Rgb, 5> colors{{
      {68, 1, 84},
      {59, 82, 139},
      {33, 145, 140},
      {94, 201, 98},
      {253, 231, 37},
  }};
  const float scaled = std::clamp(normalized, 0.0F, 1.0F) * 4.0F;
  const int lower = std::min(static_cast<int>(scaled), 3);
  const float fraction = scaled - static_cast<float>(lower);
  const auto mix = [fraction](unsigned char left, unsigned char right) {
    return static_cast<unsigned char>(
        std::lround(static_cast<float>(left) + fraction * static_cast<float>(right - left)));
  };
  return {mix(colors[static_cast<std::size_t>(lower)].red,
              colors[static_cast<std::size_t>(lower + 1)].red),
          mix(colors[static_cast<std::size_t>(lower)].green,
              colors[static_cast<std::size_t>(lower + 1)].green),
          mix(colors[static_cast<std::size_t>(lower)].blue,
              colors[static_cast<std::size_t>(lower + 1)].blue)};
}

float interpolateMagnitude(const SpectralFrame& frame, double frequency) {
  if (frame.magnitudes.empty()) {
    return 0.0F;
  }
  const auto upper = std::lower_bound(frame.centerFrequencies.begin(),
                                      frame.centerFrequencies.end(), static_cast<float>(frequency));
  if (upper == frame.centerFrequencies.begin()) {
    return frame.magnitudes.front();
  }
  if (upper == frame.centerFrequencies.end()) {
    return frame.magnitudes.back();
  }
  const std::size_t upperIndex = static_cast<std::size_t>(upper - frame.centerFrequencies.begin());
  const std::size_t lowerIndex = upperIndex - 1;
  const float lowerFrequency = frame.centerFrequencies[lowerIndex];
  const float upperFrequency = frame.centerFrequencies[upperIndex];
  const float fraction = static_cast<float>((frequency - lowerFrequency) /
                                            std::max(upperFrequency - lowerFrequency, 1.0F));
  return std::lerp(frame.magnitudes[lowerIndex], frame.magnitudes[upperIndex],
                   std::clamp(fraction, 0.0F, 1.0F));
}

double displayFrequency(double normalized, FrequencyScale scale) {
  const double lower = sinesoda::dsp::scaleFromHertz(kMinFrequency, scale);
  const double upper = sinesoda::dsp::scaleFromHertz(kMaxFrequency, scale);
  return sinesoda::dsp::hertzFromScale(std::lerp(lower, upper, normalized), scale);
}

Image renderSpectrogram(const std::vector<SpectralFrame>& transformFrames, TransformKind transform,
                        std::size_t transformEnd,
                        FrequencyScale scale = FrequencyScale::Logarithmic) {
  Image image(kCanvasWidth, kCanvasHeight, {5, 8, 15});

  const std::size_t history = std::min(kHistoryFrames, transformEnd + 1);
  const std::size_t firstFrame = transformEnd + 1 - history;
  for (int x = 0; x < kCanvasWidth; ++x) {
    const std::size_t column =
        std::min(history - 1, static_cast<std::size_t>(static_cast<double>(x) /
                                                       static_cast<double>(kCanvasWidth) *
                                                       static_cast<double>(history)));
    const auto& frame = transformFrames[firstFrame + column];
    for (int y = 0; y < kCanvasHeight; ++y) {
      float magnitude = 0.0F;
      if (transform == TransformKind::Chroma) {
        const std::size_t band = std::min<std::size_t>(
            frame.magnitudes.size() - 1,
            static_cast<std::size_t>((kCanvasHeight - 1 - y) *
                                     static_cast<int>(frame.magnitudes.size()) / kCanvasHeight));
        magnitude = frame.magnitudes[band];
      } else {
        const double normalized =
            1.0 - (static_cast<double>(y) + 0.5) / static_cast<double>(kCanvasHeight);
        magnitude = interpolateMagnitude(frame, displayFrequency(normalized, scale));
      }
      const float decibels = sinesoda::dsp::amplitudeToDb(magnitude, -96.0F);
      image.set(x, y, viridis((decibels + 96.0F) / 96.0F));
    }
  }

  return image;
}

Image renderLoopSnapshot(const AnalysisSettings& settings, TransformKind transform,
                         SignalKind signalKind) {
  const std::size_t requiredSamples = sinesoda::dsp::makeTransform(settings)->requiredSamples();
  const std::size_t signalSize =
      requiredSamples + (kHistoryFrames + kAnimationFrames + 18) * settings.hopSize;
  const std::size_t loopSize = kAnimationFrames * settings.hopSize;
  const auto signal =
      synthesizeLoopingSignal(signalSize, signalKind, signalSize - loopSize, loopSize);
  const auto transformFrames = analyze(settings, signal);
  return renderSpectrogram(transformFrames, transform, transformFrames.size() - 1);
}

void writePpm(const std::filesystem::path& path, const Image& image) {
  std::ofstream output(path, std::ios::binary);
  if (!output) {
    throw std::runtime_error("could not open visual output path");
  }
  output << "P6\n" << image.width << ' ' << image.height << "\n255\n";
  for (const Rgb pixel : image.pixels) {
    output.write(reinterpret_cast<const char*>(&pixel.red), 1);
    output.write(reinterpret_cast<const char*>(&pixel.green), 1);
    output.write(reinterpret_cast<const char*>(&pixel.blue), 1);
  }
}

std::filesystem::path parseOutputRoot(int argc, char** argv) {
  std::filesystem::path output = "artifacts/qa/site-visual-export";
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument == "--output" && index + 1 < argc) {
      output = argv[++index];
    } else {
      throw std::invalid_argument("usage: SineSodaSiteVisuals [--output DIRECTORY]");
    }
  }
  return output;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const auto outputRoot = parseOutputRoot(argc, argv);
    const auto snapshotRoot = outputRoot / "snapshots";
    const auto animationRoot = outputRoot / "animations";
    const auto parameterSweepRoot = outputRoot / "parameter-sweeps";
    const auto scaleRoot = outputRoot / "scales";
    std::filesystem::create_directories(snapshotRoot);
    std::filesystem::create_directories(animationRoot);
    std::filesystem::create_directories(parameterSweepRoot);
    std::filesystem::create_directories(scaleRoot);

    for (std::size_t index = 0; index < kTransforms.size(); ++index) {
      const TransformKind transform = kTransforms[index];
      const AnalysisSettings settings = makeSettings(transform);
      const std::size_t requiredSamples = sinesoda::dsp::makeTransform(settings)->requiredSamples();
      // Two loop lengths provide a complete pre-roll before the captured cycle.
      // Every snapshot therefore represents the same phrase from start to end,
      // which also allows the packager to scroll it against a normalized
      // piano-roll playhead without exposing a blank leading edge.
      const std::size_t signalSize =
          requiredSamples + (kHistoryFrames + kAnimationFrames + 18) * kHopSize;
      const std::size_t loopSize = kAnimationFrames * kHopSize;
      const std::size_t loopStart = signalSize - loopSize;
      const auto snapshotSignal =
          synthesizeLoopingSignal(signalSize, SignalKind::HarmonicResolution, loopStart, loopSize);
      const auto transformFrames = analyze(settings, snapshotSignal);
      const auto slug = std::string(kSlugs[index]);
      writePpm(snapshotRoot / (slug + ".ppm"),
               renderSpectrogram(transformFrames, transform, transformFrames.size() - 1));

      const auto [lowSettings, highSettings] = makeParameterSweepSettings(transform);
      writePpm(parameterSweepRoot / (slug + "-low.ppm"),
               renderLoopSnapshot(lowSettings, transform, SignalKind::HarmonicResolution));
      writePpm(parameterSweepRoot / (slug + "-high.ppm"),
               renderLoopSnapshot(highSettings, transform, SignalKind::HarmonicResolution));

      const bool animate = transform == TransformKind::Cqt ||
                           transform == TransformKind::ReassignedStft ||
                           transform == TransformKind::Chroma;
      if (animate) {
        // Feature loops deliberately use different musical material so the
        // selected transform's useful structure is visible in both the piano
        // roll and the incoming analysis.
        const auto featureSignal =
            synthesizeLoopingSignal(signalSize, signalForTransform(transform), loopStart, loopSize);
        const auto featureFrames = analyze(settings, featureSignal);
        const auto frameRoot = animationRoot / slug;
        std::filesystem::create_directories(frameRoot);
        const std::size_t available = std::min(kAnimationFrames, featureFrames.size());
        const std::size_t first = featureFrames.size() - available;
        for (std::size_t frame = 0; frame < available; ++frame) {
          std::string number = std::to_string(frame);
          number.insert(number.begin(), 3 - std::min<std::size_t>(3, number.size()), '0');
          writePpm(frameRoot / ("frame-" + number + ".ppm"),
                   renderSpectrogram(featureFrames, transform, first + frame));
        }
      }
      std::cout << "[visual] " << sinesoda::dsp::transformName(transform) << " @ 8192\n";
    }

    constexpr std::array kScales{FrequencyScale::Linear, FrequencyScale::Logarithmic,
                                 FrequencyScale::Mel, FrequencyScale::Bark, FrequencyScale::Erb};
    constexpr std::array<std::string_view, kScales.size()> kScaleSlugs{"linear", "log", "mel",
                                                                       "bark", "erb"};
    const AnalysisSettings scaleSettings = makeSettings(TransformKind::Stft);
    const std::size_t scaleRequired =
        sinesoda::dsp::makeTransform(scaleSettings)->requiredSamples();
    const std::size_t scaleSignalSize =
        scaleRequired + (kHistoryFrames + kAnimationFrames + 18) * kHopSize;
    const std::size_t scaleLoopSize = kAnimationFrames * kHopSize;
    const auto scaleSignal =
        synthesizeLoopingSignal(scaleSignalSize, SignalKind::HarmonicResolution,
                                scaleSignalSize - scaleLoopSize, scaleLoopSize);
    const auto scaleFrames = analyze(scaleSettings, scaleSignal);
    for (std::size_t index = 0; index < kScales.size(); ++index) {
      writePpm(scaleRoot / (std::string(kScaleSlugs[index]) + ".ppm"),
               renderSpectrogram(scaleFrames, TransformKind::Stft, scaleFrames.size() - 1,
                                 kScales[index]));
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "SineSodaSiteVisuals: " << error.what() << '\n';
    return 1;
  }
}
