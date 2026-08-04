#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

#include "sinesoda/dsp/AnalysisSettings.h"
#include "sinesoda/dsp/SpectralFrame.h"
#include "sinesoda/dsp/Transforms.h"

namespace sinesoda::dsp {

class StreamingAnalyzer {
 public:
  using FrameCallback = std::function<void(const SpectralFrame&)>;

  explicit StreamingAnalyzer(const AnalysisSettings& settings);

  void configure(const AnalysisSettings& settings);
  void reset();
  void push(std::span<const float> samples, const FrameCallback& callback);

  [[nodiscard]] const AnalysisSettings& settings() const noexcept { return settings_; }
  [[nodiscard]] std::uint64_t samplesProcessed() const noexcept { return samplesProcessed_; }

 private:
  void emitFrame(const FrameCallback& callback);

  AnalysisSettings settings_;
  std::unique_ptr<IFrameTransform> transform_;
  std::vector<float> history_;
  std::vector<float> contiguousFrame_;
  std::size_t writeIndex_ = 0;
  std::size_t samplesSinceFrame_ = 0;
  std::uint64_t samplesProcessed_ = 0;
  SpectralFrame outputFrame_;
};

}  // namespace sinesoda::dsp
