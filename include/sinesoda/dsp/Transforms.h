#pragma once

#include <memory>
#include <span>

#include "sinesoda/dsp/AnalysisSettings.h"
#include "sinesoda/dsp/SpectralFrame.h"

namespace sinesoda::dsp {

class IFrameTransform {
 public:
  virtual ~IFrameTransform() = default;

  [[nodiscard]] virtual std::size_t requiredSamples() const noexcept = 0;
  virtual void analyze(std::span<const float> input, SpectralFrame& output) = 0;
};

[[nodiscard]] std::unique_ptr<IFrameTransform> makeTransform(const AnalysisSettings& settings);

}  // namespace sinesoda::dsp
