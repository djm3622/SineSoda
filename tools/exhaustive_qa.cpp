#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numbers>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "sinesoda/dsp/AnalysisSettings.h"
#include "sinesoda/dsp/SpectralFrame.h"
#include "sinesoda/dsp/StreamingAnalyzer.h"
#include "sinesoda/dsp/Transforms.h"

namespace {

using sinesoda::dsp::AnalysisSettings;
using sinesoda::dsp::SpectralFrame;
using sinesoda::dsp::StreamingAnalyzer;
using sinesoda::dsp::TransformKind;

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
constexpr std::array<double, 9> kSampleRates{22050.0, 32000.0,  44100.0,  48000.0, 88200.0,
                                             96000.0, 176400.0, 192000.0, 384000.0};
constexpr std::array<std::size_t, 5> kFixedBlockSizes{1, 32, 64, 512, 2048};
constexpr std::array<std::size_t, 7> kVariableBlockPattern{1, 17, 64, 511, 3, 1024, 29};

struct MatrixResult {
  TransformKind transform{};
  double sampleRate = 0.0;
  std::size_t analysisWindow = 0;
  std::size_t hopSize = 0;
  std::size_t bandCount = 0;
  float peakFrequency = 0.0F;
  float peakMagnitude = 0.0F;
  double elapsedMilliseconds = 0.0;
  double maximumBlockDifference = 0.0;
  bool passed = false;
  std::string note;
};

struct Rgb {
  int red = 0;
  int green = 0;
  int blue = 0;
};

struct ColorStop {
  float position = 0.0F;
  Rgb color;
};

std::vector<float> makeQaSignal(std::size_t size, double sampleRate) {
  std::vector<float> signal(size);
  constexpr double twoPi = 2.0 * std::numbers::pi_v<double>;
  for (std::size_t index = 0; index < size; ++index) {
    const double time = static_cast<double>(index) / sampleRate;
    const double sweepFrequency = 180.0 + 2200.0 * static_cast<double>(index) /
                                              static_cast<double>(std::max<std::size_t>(size, 1));
    const double sweepPhase = twoPi * (180.0 * time + 0.5 * (sweepFrequency - 180.0) * time);
    signal[index] =
        static_cast<float>(0.68 * std::sin(twoPi * 440.0 * time) +
                           0.18 * std::sin(twoPi * 997.0 * time) + 0.08 * std::sin(sweepPhase));
  }
  if (!signal.empty()) {
    signal[signal.size() / 2] = std::clamp(signal[signal.size() / 2] + 0.2F, -1.0F, 1.0F);
  }
  return signal;
}

std::vector<float> makePreviewSignal(std::size_t size, double sampleRate) {
  std::vector<float> signal(size);
  if (size == 0) {
    return signal;
  }

  constexpr double twoPi = 2.0 * std::numbers::pi_v<double>;
  const double duration = static_cast<double>(size) / sampleRate;
  const double sweepRate = (7600.0 - 140.0) / std::max(duration, 1.0 / sampleRate);
  for (std::size_t index = 0; index < size; ++index) {
    const double time = static_cast<double>(index) / sampleRate;
    const double position = static_cast<double>(index) / static_cast<double>(size);
    const double sweepPhase = twoPi * (140.0 * time + 0.5 * sweepRate * time * time);
    const double toneEnvelope = position > 0.18 && position < 0.72 ? 1.0 : 0.16;
    const double upperEnvelope = position > 0.48 && position < 0.9 ? 1.0 : 0.0;
    signal[index] = static_cast<float>(0.34 * std::sin(sweepPhase) +
                                       0.48 * toneEnvelope * std::sin(twoPi * 440.0 * time) +
                                       0.22 * upperEnvelope * std::sin(twoPi * 1760.0 * time));
  }
  for (const double position : {0.28, 0.58, 0.84}) {
    const auto index = static_cast<std::size_t>(position * static_cast<double>(size - 1));
    signal[index] = std::clamp(signal[index] + 0.8F, -1.0F, 1.0F);
  }
  return signal;
}

SpectralFrame analyzeInBlocks(const AnalysisSettings& settings, std::span<const float> signal,
                              std::span<const std::size_t> pattern) {
  if (pattern.empty() || std::ranges::any_of(pattern, [](std::size_t size) { return size == 0; })) {
    throw std::invalid_argument("block pattern must contain only positive sizes");
  }
  StreamingAnalyzer analyzer(settings);
  SpectralFrame lastFrame;
  std::size_t frameCount = 0;
  std::size_t offset = 0;
  std::size_t patternIndex = 0;
  while (offset < signal.size()) {
    const std::size_t count =
        std::min(pattern[patternIndex % pattern.size()], signal.size() - offset);
    analyzer.push(signal.subspan(offset, count), [&](const SpectralFrame& frame) {
      lastFrame = frame;
      ++frameCount;
    });
    offset += count;
    ++patternIndex;
  }
  if (frameCount == 0) {
    throw std::runtime_error("streaming analyzer did not emit a frame");
  }
  return lastFrame;
}

std::vector<SpectralFrame> analyzeFrames(const AnalysisSettings& settings,
                                         std::span<const float> signal) {
  StreamingAnalyzer analyzer(settings);
  std::vector<SpectralFrame> frames;
  std::size_t offset = 0;
  std::size_t patternIndex = 0;
  while (offset < signal.size()) {
    const std::size_t count = std::min(
        kVariableBlockPattern[patternIndex % kVariableBlockPattern.size()], signal.size() - offset);
    analyzer.push(signal.subspan(offset, count),
                  [&](const SpectralFrame& frame) { frames.push_back(frame); });
    offset += count;
    ++patternIndex;
  }
  return frames;
}

void validateFrame(const SpectralFrame& frame, const AnalysisSettings& settings,
                   bool requireMeasurableResponse = true) {
  if (frame.transform != settings.transform || frame.magnitudes.empty() ||
      frame.centerFrequencies.size() != frame.magnitudes.size() ||
      frame.bandwidths.size() != frame.magnitudes.size()) {
    throw std::runtime_error("frame metadata or dimensions are inconsistent");
  }
  if (!frame.timeOffsetsSeconds.empty() &&
      frame.timeOffsetsSeconds.size() != frame.magnitudes.size()) {
    throw std::runtime_error("time-coordinate dimensions are inconsistent");
  }
  for (std::size_t index = 0; index < frame.magnitudes.size(); ++index) {
    const float magnitude = frame.magnitudes[index];
    const float center = frame.centerFrequencies[index];
    const float bandwidth = frame.bandwidths[index];
    if (!std::isfinite(magnitude) || magnitude < 0.0F || !std::isfinite(center) || center < 0.0F ||
        !std::isfinite(bandwidth) || bandwidth < 0.0F ||
        center > static_cast<float>(settings.sampleRate * 0.5 + 1.0)) {
      throw std::runtime_error("frame contains an invalid magnitude or frequency coordinate");
    }
  }
  for (const float offset : frame.timeOffsetsSeconds) {
    if (!std::isfinite(offset)) {
      throw std::runtime_error("frame contains a non-finite reassigned time coordinate");
    }
  }
  if (requireMeasurableResponse &&
      *std::max_element(frame.magnitudes.begin(), frame.magnitudes.end()) <= 1.0e-7F) {
    throw std::runtime_error("transform produced no measurable response to the QA signal");
  }
}

double maximumDifference(const SpectralFrame& left, const SpectralFrame& right) {
  if (left.magnitudes.size() != right.magnitudes.size()) {
    return std::numeric_limits<double>::infinity();
  }
  double result = 0.0;
  for (std::size_t index = 0; index < left.magnitudes.size(); ++index) {
    result = std::max(result, std::abs(static_cast<double>(left.magnitudes[index]) -
                                       static_cast<double>(right.magnitudes[index])));
  }
  return result;
}

Rgb interpolateViridis(float normalized) {
  constexpr std::array<ColorStop, 5> stops{{
      {0.0F, {68, 1, 84}},
      {0.25F, {59, 82, 139}},
      {0.5F, {33, 145, 140}},
      {0.75F, {94, 201, 98}},
      {1.0F, {253, 231, 37}},
  }};
  const float value = std::clamp(normalized, 0.0F, 1.0F);
  for (std::size_t index = 1; index < stops.size(); ++index) {
    if (value <= stops[index].position) {
      const float span = stops[index].position - stops[index - 1].position;
      const float fraction = span > 0.0F ? (value - stops[index - 1].position) / span : 0.0F;
      const auto blend = [fraction](int left, int right) {
        return static_cast<int>(
            std::lround(static_cast<float>(left) + fraction * static_cast<float>(right - left)));
      };
      return {blend(stops[index - 1].color.red, stops[index].color.red),
              blend(stops[index - 1].color.green, stops[index].color.green),
              blend(stops[index - 1].color.blue, stops[index].color.blue)};
    }
  }
  return stops.back().color;
}

std::string rgbHex(const Rgb& color) {
  std::ostringstream output;
  output << '#' << std::hex << std::setfill('0') << std::setw(2) << color.red << std::setw(2)
         << color.green << std::setw(2) << color.blue;
  return output.str();
}

void writePreviewSvg(const std::filesystem::path& path) {
  constexpr std::size_t previewColumns = 32;
  constexpr std::size_t previewRows = 64;
  constexpr int panelColumns = 3;
  constexpr int panelWidth = 360;
  constexpr int panelHeight = 188;
  constexpr int margin = 16;
  constexpr int headerHeight = 42;
  constexpr int footerHeight = 36;
  constexpr int panelRows =
      static_cast<int>((kTransforms.size() + panelColumns - 1) / panelColumns);
  constexpr int canvasWidth = panelColumns * panelWidth + (panelColumns + 1) * margin;
  constexpr int canvasHeight = panelRows * panelHeight + (panelRows + 1) * margin + footerHeight;

  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("could not open SVG preview path");
  }
  output << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << canvasWidth << "\" height=\""
         << canvasHeight << "\" viewBox=\"0 0 " << canvasWidth << ' ' << canvasHeight
         << "\" role=\"img\" aria-labelledby=\"title description\">\n"
         << "<title id=\"title\">SineSoda transform preview matrix</title>\n"
         << "<desc id=\"description\">Actual analyzer output for all fifteen transforms at 48 "
            "kilohertz using a deterministic tones, chirp, and transients signal.</desc>\n"
         << "<rect width=\"100%\" height=\"100%\" fill=\"#080b12\"/>\n";

  for (std::size_t transformIndex = 0; transformIndex < kTransforms.size(); ++transformIndex) {
    const TransformKind transform = kTransforms[transformIndex];
    AnalysisSettings settings;
    settings.transform = transform;
    settings.sampleRate = 48000.0;
    settings.fftSize = 2048;
    settings.hopSize = 512;
    settings.bandCount = 192;
    settings.minFrequency = 55.0;
    settings.maxFrequency = 20000.0;
    settings.binsPerOctave = 24;
    settings.gamma = transform == TransformKind::Vqt ? 20.0 : 0.0;

    const std::size_t requiredSamples = sinesoda::dsp::makeTransform(settings)->requiredSamples();
    const std::size_t signalSize = requiredSamples + (previewColumns + 3) * settings.hopSize;
    const auto signal = makePreviewSignal(signalSize, settings.sampleRate);
    const auto frames = analyzeFrames(settings, signal);
    if (frames.size() < previewColumns) {
      throw std::runtime_error("insufficient frames for transform preview");
    }
    for (const auto& frame : frames) {
      validateFrame(frame, settings, false);
    }

    const int panelColumn = static_cast<int>(transformIndex) % panelColumns;
    const int panelRow = static_cast<int>(transformIndex) / panelColumns;
    const int x = margin + panelColumn * (panelWidth + margin);
    const int y = margin + panelRow * (panelHeight + margin);
    const int plotX = x + 10;
    const int plotY = y + headerHeight;
    const int plotWidth = panelWidth - 20;
    const int plotHeight = panelHeight - headerHeight - 10;
    const double cellWidth = static_cast<double>(plotWidth) / static_cast<double>(previewColumns);
    const double cellHeight = static_cast<double>(plotHeight) / static_cast<double>(previewRows);

    output << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << panelWidth << "\" height=\""
           << panelHeight << "\" rx=\"7\" fill=\"#0d1220\" stroke=\"#2c3548\"/>\n"
           << "<text x=\"" << x + 12 << "\" y=\"" << y + 18
           << "\" fill=\"#d7e0f0\" font-family=\"-apple-system, sans-serif\" font-size=\"13\">"
           << sinesoda::dsp::transformName(transform) << "</text>\n"
           << "<text x=\"" << x + 12 << "\" y=\"" << y + 34
           << "\" fill=\"#91a0b8\" font-family=\"-apple-system, sans-serif\" font-size=\"10\">48 "
              "kHz · "
           << requiredSamples << " samples · " << frames.back().magnitudes.size()
           << " bands</text>\n"
           << "<rect x=\"" << plotX << "\" y=\"" << plotY << "\" width=\"" << plotWidth
           << "\" height=\"" << plotHeight << "\" fill=\"#04060c\"/>\n";

    const std::size_t firstFrame = frames.size() - previewColumns;
    output << std::fixed << std::setprecision(2);
    for (std::size_t column = 0; column < previewColumns; ++column) {
      const auto& magnitudes = frames[firstFrame + column].magnitudes;
      for (std::size_t row = 0; row < previewRows; ++row) {
        const std::size_t invertedRow = previewRows - 1 - row;
        const std::size_t firstBand = invertedRow * magnitudes.size() / previewRows;
        const std::size_t lastBand =
            std::max(firstBand + 1, (invertedRow + 1) * magnitudes.size() / previewRows);
        float magnitude = 0.0F;
        for (std::size_t band = firstBand; band < std::min(lastBand, magnitudes.size()); ++band) {
          magnitude = std::max(magnitude, magnitudes[band]);
        }
        const float decibels = sinesoda::dsp::amplitudeToDb(magnitude, -96.0F);
        const Rgb color = interpolateViridis((decibels + 96.0F) / 96.0F);
        output << "<rect x=\"" << static_cast<double>(plotX) + cellWidth * column << "\" y=\""
               << static_cast<double>(plotY) + cellHeight * row << "\" width=\"" << cellWidth + 0.15
               << "\" height=\"" << cellHeight + 0.15 << "\" fill=\"" << rgbHex(color) << "\"/>\n";
      }
    }
  }

  output << "<text x=\"" << margin << "\" y=\"" << canvasHeight - 18
         << "\" fill=\"#91a0b8\" font-family=\"-apple-system, sans-serif\" font-size=\"11\">"
            "Offline QA preview from real transform frames · deterministic 140–7600 Hz chirp, "
            "440/1760 Hz tones, and three transients · viridis, −96 to 0 dBFS</text>\n"
         << "</svg>\n";
}

std::string escapeJson(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  for (const char character : value) {
    if (character == '\\' || character == '"') {
      result.push_back('\\');
    }
    result.push_back(character);
  }
  return result;
}

MatrixResult runCase(TransformKind transform, double sampleRate) {
  MatrixResult result;
  result.transform = transform;
  result.sampleRate = sampleRate;
  try {
    AnalysisSettings settings;
    settings.transform = transform;
    settings.sampleRate = sampleRate;
    settings.fftSize = 2048;
    settings.bandCount = 192;
    settings.minFrequency = 55.0;
    settings.maxFrequency = std::min(20000.0, sampleRate * 0.475);
    settings.binsPerOctave = 24;
    settings.gamma = transform == TransformKind::Vqt ? 20.0 : 0.0;

    const auto initialTransform = sinesoda::dsp::makeTransform(settings);
    result.analysisWindow = initialTransform->requiredSamples();
    settings.hopSize = std::min(result.analysisWindow, settings.fftSize * 16);
    const std::size_t emittedFrames =
        (result.analysisWindow + settings.hopSize - 1) / settings.hopSize;
    const std::size_t signalSize = emittedFrames * settings.hopSize;
    const auto signal = makeQaSignal(signalSize, sampleRate);

    const auto started = std::chrono::steady_clock::now();
    const std::array referencePattern{signal.size()};
    const SpectralFrame reference = analyzeInBlocks(settings, signal, referencePattern);
    const auto stopped = std::chrono::steady_clock::now();
    validateFrame(reference, settings);

    result.elapsedMilliseconds =
        std::chrono::duration<double, std::milli>(stopped - started).count();
    result.hopSize = settings.hopSize;
    result.bandCount = reference.magnitudes.size();
    const auto peak = std::max_element(reference.magnitudes.begin(), reference.magnitudes.end());
    const std::size_t peakIndex =
        static_cast<std::size_t>(std::distance(reference.magnitudes.begin(), peak));
    result.peakFrequency = reference.centerFrequencies[peakIndex];
    result.peakMagnitude = *peak;

    for (const std::size_t blockSize : kFixedBlockSizes) {
      const std::array pattern{blockSize};
      const SpectralFrame candidate = analyzeInBlocks(settings, signal, pattern);
      validateFrame(candidate, settings);
      result.maximumBlockDifference =
          std::max(result.maximumBlockDifference, maximumDifference(reference, candidate));
    }
    const SpectralFrame variable = analyzeInBlocks(settings, signal, kVariableBlockPattern);
    validateFrame(variable, settings);
    result.maximumBlockDifference =
        std::max(result.maximumBlockDifference, maximumDifference(reference, variable));
    if (result.maximumBlockDifference > 1.0e-7) {
      throw std::runtime_error("output changes with host block partitioning");
    }

    result.passed = true;
    result.note = transform == TransformKind::Chroma
                      ? "Peak frequency is the representative frequency of the winning pitch class."
                      : "Finite, nonnegative response; invariant across all block patterns.";
  } catch (const std::exception& error) {
    result.note = error.what();
  }
  return result;
}

void writeJson(const std::filesystem::path& path, const std::vector<MatrixResult>& results) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("could not open JSON report path");
  }
  const auto passed = std::count_if(results.begin(), results.end(),
                                    [](const MatrixResult& result) { return result.passed; });
  output << "{\n  \"schema_version\": 1,\n  \"platform\": \"macOS\",\n"
         << "  \"sample_rates_hz\": [";
  for (std::size_t index = 0; index < kSampleRates.size(); ++index) {
    output << std::fixed << std::setprecision(0) << kSampleRates[index]
           << (index + 1 == kSampleRates.size() ? "" : ", ");
  }
  output << "],\n"
         << "  \"fixed_block_sizes\": [1, 32, 64, 512, 2048],\n"
         << "  \"variable_block_pattern\": [1, 17, 64, 511, 3, 1024, 29],\n"
         << "  \"passed_cases\": " << passed << ",\n"
         << "  \"total_cases\": " << results.size() << ",\n  \"cases\": [\n";
  for (std::size_t index = 0; index < results.size(); ++index) {
    const auto& result = results[index];
    output << "    {\"transform\": \"" << escapeJson(sinesoda::dsp::transformName(result.transform))
           << "\", "
           << "\"sample_rate_hz\": " << std::fixed << std::setprecision(0) << result.sampleRate
           << ", \"analysis_window_samples\": " << result.analysisWindow
           << ", \"hop_samples\": " << result.hopSize << ", \"bands\": " << result.bandCount
           << ", \"peak_frequency_hz\": " << std::setprecision(4) << result.peakFrequency
           << ", \"peak_magnitude\": " << result.peakMagnitude
           << ", \"elapsed_ms\": " << result.elapsedMilliseconds
           << ", \"maximum_block_difference\": " << result.maximumBlockDifference
           << ", \"passed\": " << (result.passed ? "true" : "false") << ", \"note\": \""
           << escapeJson(result.note) << "\"}" << (index + 1 == results.size() ? "\n" : ",\n");
  }
  output << "  ]\n}\n";
}

void writeMarkdown(const std::filesystem::path& path, const std::vector<MatrixResult>& results) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("could not open Markdown report path");
  }
  const auto passed = std::count_if(results.begin(), results.end(),
                                    [](const MatrixResult& result) { return result.passed; });
  output << "# SineSoda macOS exhaustive transform QA\n\n"
         << "Passed **" << passed << '/' << results.size()
         << "** transform/sample-rate cases over nine standard host rates from 22.05 to 384 kHz. "
            "Each case was compared across fixed host blocks "
            "of 1, 32, 64, 512, and 2,048 samples plus the variable pattern "
            "`1, 17, 64, 511, 3, 1024, 29`.\n\n"
         << "| Transform | Rate | Window | Bands | Peak | Max block delta | Result |\n"
         << "|---|---:|---:|---:|---:|---:|:---:|\n";
  for (const auto& result : results) {
    output << "| " << sinesoda::dsp::transformName(result.transform) << " | " << std::fixed
           << std::setprecision(1) << result.sampleRate / 1000.0 << " kHz | "
           << result.analysisWindow << " | " << result.bandCount << " | " << std::setprecision(1)
           << result.peakFrequency << " Hz | " << std::scientific << std::setprecision(1)
           << result.maximumBlockDifference << " | " << (result.passed ? "PASS" : "FAIL") << " |\n";
  }
  output << "\nNotes:\n\n"
         << "- The deterministic signal contains dominant 440 Hz energy, a quieter 997 Hz tone, "
            "a low-level chirp, and a transient.\n"
         << "- CQT/VQT window lengths grow with sample rate because their time support is "
            "defined in seconds/cycles, not a fixed FFT count.\n"
         << "- Chroma reports a representative pitch-class frequency rather than a literal "
            "spectral-bin peak.\n"
         << "- A zero maximum block delta proves that host buffer partitioning did not change "
            "the final frame for this deterministic input.\n";
}

std::filesystem::path parseOutputRoot(int argc, char** argv) {
  std::filesystem::path result = "artifacts/qa/macos-exhaustive";
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument == "--output" && index + 1 < argc) {
      result = argv[++index];
    } else {
      throw std::invalid_argument("usage: SineSodaExhaustiveQA [--output DIRECTORY]");
    }
  }
  return result;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const std::filesystem::path outputRoot = parseOutputRoot(argc, argv);
    std::filesystem::create_directories(outputRoot);
    std::vector<MatrixResult> results;
    results.reserve(kTransforms.size() * kSampleRates.size());
    for (const TransformKind transform : kTransforms) {
      for (const double sampleRate : kSampleRates) {
        MatrixResult result = runCase(transform, sampleRate);
        std::cout << (result.passed ? "[PASS] " : "[FAIL] ")
                  << sinesoda::dsp::transformName(transform) << " @ " << sampleRate << " Hz"
                  << " (max block delta " << result.maximumBlockDifference << ")\n";
        results.push_back(std::move(result));
      }
    }

    writeJson(outputRoot / "transform-matrix.json", results);
    writeMarkdown(outputRoot / "transform-matrix.md", results);
    writePreviewSvg(outputRoot / "transform-previews.svg");
    const auto passed = std::count_if(results.begin(), results.end(),
                                      [](const MatrixResult& result) { return result.passed; });
    std::cout << passed << '/' << results.size() << " exhaustive cases passed\n"
              << "Reports written to " << outputRoot << '\n';
    return passed == static_cast<std::ptrdiff_t>(results.size()) ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "SineSodaExhaustiveQA: " << error.what() << '\n';
    return 2;
  }
}
