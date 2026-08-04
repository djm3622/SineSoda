#include "sinesoda/dsp/StreamingAnalyzer.h"

#include <algorithm>
#include <stdexcept>

namespace sinesoda::dsp {

StreamingAnalyzer::StreamingAnalyzer(const AnalysisSettings& settings) { configure(settings); }

void StreamingAnalyzer::configure(const AnalysisSettings& settings) {
  settings.validate();
  settings_ = settings;
  transform_ = makeTransform(settings_);
  const auto required = transform_->requiredSamples();
  if (required == 0) {
    throw std::logic_error("transform requested an empty analysis window");
  }
  history_.assign(required, 0.0F);
  contiguousFrame_.assign(required, 0.0F);
  reset();
}

void StreamingAnalyzer::reset() {
  std::fill(history_.begin(), history_.end(), 0.0F);
  std::fill(contiguousFrame_.begin(), contiguousFrame_.end(), 0.0F);
  writeIndex_ = 0;
  samplesSinceFrame_ = 0;
  samplesProcessed_ = 0;
  outputFrame_ = {};
}

void StreamingAnalyzer::push(std::span<const float> samples, const FrameCallback& callback) {
  if (!callback) {
    throw std::invalid_argument("frame callback must be valid");
  }
  for (const float sample : samples) {
    history_[writeIndex_] = sample;
    writeIndex_ = (writeIndex_ + 1) % history_.size();
    ++samplesSinceFrame_;
    ++samplesProcessed_;
    if (samplesSinceFrame_ >= settings_.hopSize) {
      samplesSinceFrame_ -= settings_.hopSize;
      emitFrame(callback);
    }
  }
}

void StreamingAnalyzer::emitFrame(const FrameCallback& callback) {
  const std::size_t tail = history_.size() - writeIndex_;
  std::copy(history_.begin() + static_cast<std::ptrdiff_t>(writeIndex_), history_.end(),
            contiguousFrame_.begin());
  std::copy(history_.begin(), history_.begin() + static_cast<std::ptrdiff_t>(writeIndex_),
            contiguousFrame_.begin() + static_cast<std::ptrdiff_t>(tail));

  transform_->analyze(contiguousFrame_, outputFrame_);
  outputFrame_.endSample = samplesProcessed_;
  outputFrame_.analysisWindowSamples = transform_->requiredSamples();
  callback(outputFrame_);
}

}  // namespace sinesoda::dsp
