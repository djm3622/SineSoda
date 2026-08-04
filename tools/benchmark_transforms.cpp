#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <span>
#include <string_view>
#include <vector>

#include "sinesoda/dsp/AnalysisSettings.h"
#include "sinesoda/dsp/SpectralFrame.h"
#include "sinesoda/dsp/Transforms.h"

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::size_t kTransformCount = 15;

}  // namespace

int main() {
  sinesoda::dsp::AnalysisSettings settings;
  settings.sampleRate = 48000.0;
  settings.fftSize = 2048;
  settings.hopSize = 512;
  settings.bandCount = 192;
  settings.minFrequency = 20.0;
  settings.maxFrequency = 20000.0;
  settings.binsPerOctave = 24;

  std::cout << "SineSoda transform benchmark (48 kHz, FFT 2048, 192 requested bands)\n";
  std::cout << std::left << std::setw(34) << "Transform" << std::right << std::setw(12) << "Window"
            << std::setw(14) << "Median ms" << std::setw(14) << "Max fps" << '\n';

  for (std::size_t index = 0; index < kTransformCount; ++index) {
    settings.transform = static_cast<sinesoda::dsp::TransformKind>(index);
    auto transform = sinesoda::dsp::makeTransform(settings);
    const std::size_t required = transform->requiredSamples();
    std::vector<float> signal(required);
    for (std::size_t sample = 0; sample < signal.size(); ++sample) {
      const double time = static_cast<double>(sample) / settings.sampleRate;
      signal[sample] =
          static_cast<float>(0.5 * std::sin(2.0 * std::numbers::pi_v<double> * 220.0 * time) +
                             0.25 * std::sin(2.0 * std::numbers::pi_v<double> * 1760.0 * time));
    }

    sinesoda::dsp::SpectralFrame frame;
    transform->analyze(signal, frame);
    std::vector<double> durations;
    constexpr int repetitions = 5;
    durations.reserve(repetitions);
    for (int repetition = 0; repetition < repetitions; ++repetition) {
      const auto start = Clock::now();
      transform->analyze(signal, frame);
      const auto stop = Clock::now();
      durations.push_back(std::chrono::duration<double, std::milli>(stop - start).count());
    }
    std::sort(durations.begin(), durations.end());
    const double median = durations[durations.size() / 2];
    const double framesPerSecond = median > 0.0 ? 1000.0 / median : 0.0;
    std::cout << std::left << std::setw(34) << sinesoda::dsp::transformName(settings.transform)
              << std::right << std::setw(12) << required << std::setw(14) << std::fixed
              << std::setprecision(3) << median << std::setw(14) << std::setprecision(1)
              << framesPerSecond << '\n';
  }
}
