#include "sinesoda/runtime/AnalysisEngine.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <exception>
#include <span>
#include <utility>

#include "sinesoda/dsp/StreamingAnalyzer.h"

namespace sinesoda::runtime {
namespace {

static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
              "real-time settings publication requires lock-free 64-bit atomics");

std::uint64_t encodeDouble(double value) noexcept { return std::bit_cast<std::uint64_t>(value); }

double decodeDouble(std::uint64_t value) noexcept { return std::bit_cast<double>(value); }

struct RealtimeConfiguration {
  dsp::AnalysisSettings settings;
  std::string status;
};

RealtimeConfiguration adaptForRealtime(const dsp::AnalysisSettings& requested) {
  RealtimeConfiguration result{requested, {}};
  if (requested.transform != dsp::TransformKind::Cqt &&
      requested.transform != dsp::TransformKind::Vqt) {
    return result;
  }

  constexpr double tapBudgetPerSecond = 150'000'000.0;
  const bool variableQ = requested.transform == dsp::TransformKind::Vqt;
  const double gamma = variableQ ? requested.gamma : 0.0;
  const double bins = static_cast<double>(requested.binsPerOctave);
  const double q = requested.qScale / (std::exp2(1.0 / bins) - 1.0);
  const double tuningRatio = std::exp2(requested.tuningCents / 1200.0);
  const auto maximumWindow =
      static_cast<std::size_t>(std::ceil(requested.maximumWindowSeconds * requested.sampleRate));

  std::size_t tapsPerFrame = 0;
  for (std::size_t index = 0; index < requested.bandCount; ++index) {
    const double frequency =
        requested.minFrequency * tuningRatio * std::exp2(static_cast<double>(index) / bins);
    if (frequency > requested.maxFrequency || frequency >= requested.sampleRate * 0.5) {
      break;
    }
    const double bandwidth = frequency / q + gamma;
    const auto windowLength = std::clamp<std::size_t>(
        static_cast<std::size_t>(std::ceil(requested.sampleRate / bandwidth)), 8, maximumWindow);
    tapsPerFrame += windowLength;
  }

  const auto minimumHop = static_cast<std::size_t>(
      std::ceil(static_cast<double>(tapsPerFrame) * requested.sampleRate / tapBudgetPerSecond));
  const std::size_t maximumHop = requested.fftSize * 16;
  const std::size_t boundedHop = std::min(minimumHop, maximumHop);
  if (boundedHop > result.settings.hopSize) {
    result.settings.hopSize = boundedHop;
    result.status = "CQT/VQT update rate limited to preserve real-time operation (effective " +
                    std::to_string(result.settings.hopSize) + "-sample hop)";
  }
  if (minimumHop > maximumHop) {
    result.status += "; requested transform exceeds the conservative worker budget";
  }
  result.settings.validate();
  return result;
}

}  // namespace

AnalysisEngine::AnalysisEngine(const dsp::AnalysisSettings& settings) {
  settings.validate();
  publishSettings(settings);
  worker_ = std::thread([this] { run(); });
}

AnalysisEngine::~AnalysisEngine() {
  stopRequested_.store(true, std::memory_order_release);
  if (worker_.joinable()) {
    worker_.join();
  }
}

void AnalysisEngine::pushMono(const float* samples, std::size_t count) noexcept {
  if (samples == nullptr) {
    return;
  }
  std::uint64_t dropped = 0;
  for (std::size_t index = 0; index < count; ++index) {
    if (!audioQueue_.tryPush(samples[index])) {
      ++dropped;
    }
  }
  if (dropped != 0) {
    droppedSamples_.fetch_add(dropped, std::memory_order_relaxed);
  }
}

void AnalysisEngine::pushMono(const double* samples, std::size_t count) noexcept {
  if (samples == nullptr) {
    return;
  }
  std::uint64_t dropped = 0;
  for (std::size_t index = 0; index < count; ++index) {
    if (!audioQueue_.tryPush(static_cast<float>(samples[index]))) {
      ++dropped;
    }
  }
  if (dropped != 0) {
    droppedSamples_.fetch_add(dropped, std::memory_order_relaxed);
  }
}

void AnalysisEngine::pushStereo(const float* left, const float* right, std::size_t count) noexcept {
  if (left == nullptr || right == nullptr) {
    return;
  }
  std::uint64_t dropped = 0;
  for (std::size_t index = 0; index < count; ++index) {
    const float mono = 0.5F * (left[index] + right[index]);
    if (!audioQueue_.tryPush(mono)) {
      ++dropped;
    }
  }
  if (dropped != 0) {
    droppedSamples_.fetch_add(dropped, std::memory_order_relaxed);
  }
}

void AnalysisEngine::pushStereo(const double* left, const double* right,
                                std::size_t count) noexcept {
  if (left == nullptr || right == nullptr) {
    return;
  }
  std::uint64_t dropped = 0;
  for (std::size_t index = 0; index < count; ++index) {
    const float mono = static_cast<float>(0.5 * (left[index] + right[index]));
    if (!audioQueue_.tryPush(mono)) {
      ++dropped;
    }
  }
  if (dropped != 0) {
    droppedSamples_.fetch_add(dropped, std::memory_order_relaxed);
  }
}

void AnalysisEngine::requestSettings(const dsp::AnalysisSettings& settings) {
  settings.validate();
  std::scoped_lock lock(settingsWriterMutex_);
  publishSettings(settings);
}

bool AnalysisEngine::copyLatestFrame(dsp::SpectralFrame& destination,
                                     std::uint64_t& lastSequence) const {
  std::scoped_lock lock(frameMutex_);
  if (latestSequence_ == 0 || latestSequence_ == lastSequence) {
    return false;
  }
  destination = latestFrame_;
  lastSequence = latestSequence_;
  return true;
}

bool AnalysisEngine::tryPopFrame(dsp::SpectralFrame& destination) {
  return frameQueue_.tryPop(destination);
}

void AnalysisEngine::setDisplayConsumerActive(bool active) noexcept {
  displayConsumerActive_.store(active, std::memory_order_release);
  frameQueue_.discard(frameQueue_.size());
  droppedFrames_.store(0, std::memory_order_relaxed);
}

std::size_t AnalysisEngine::queuedAudioSamples() const noexcept { return audioQueue_.size(); }

std::size_t AnalysisEngine::queuedFrames() const noexcept { return frameQueue_.size(); }

std::uint64_t AnalysisEngine::droppedSamples() const noexcept {
  return droppedSamples_.load(std::memory_order_relaxed);
}

std::uint64_t AnalysisEngine::droppedFrames() const noexcept {
  return droppedFrames_.load(std::memory_order_relaxed);
}

std::string AnalysisEngine::statusMessage() const {
  std::scoped_lock lock(statusMutex_);
  return statusMessage_;
}

void AnalysisEngine::publishSettings(const dsp::AnalysisSettings& settings) noexcept {
  settingsGeneration_.fetch_add(1, std::memory_order_acq_rel);
  const auto put = [this](SettingSlot slot, std::uint64_t value) {
    settings_[static_cast<std::size_t>(slot)].store(value, std::memory_order_relaxed);
  };
  put(SettingSlot::Transform, static_cast<std::uint64_t>(settings.transform));
  put(SettingSlot::Window, static_cast<std::uint64_t>(settings.window));
  put(SettingSlot::SampleRate, encodeDouble(settings.sampleRate));
  put(SettingSlot::FftSize, static_cast<std::uint64_t>(settings.fftSize));
  put(SettingSlot::HopSize, static_cast<std::uint64_t>(settings.hopSize));
  put(SettingSlot::BandCount, static_cast<std::uint64_t>(settings.bandCount));
  put(SettingSlot::MinFrequency, encodeDouble(settings.minFrequency));
  put(SettingSlot::MaxFrequency, encodeDouble(settings.maxFrequency));
  put(SettingSlot::BinsPerOctave, static_cast<std::uint64_t>(settings.binsPerOctave));
  put(SettingSlot::TuningCents, encodeDouble(settings.tuningCents));
  put(SettingSlot::QScale, encodeDouble(settings.qScale));
  put(SettingSlot::Gamma, encodeDouble(settings.gamma));
  put(SettingSlot::MaximumWindowSeconds, encodeDouble(settings.maximumWindowSeconds));
  settingsGeneration_.fetch_add(1, std::memory_order_release);
}

dsp::AnalysisSettings AnalysisEngine::readSettings() const noexcept {
  while (true) {
    const std::uint64_t before = settingsGeneration_.load(std::memory_order_acquire);
    if ((before & 1U) != 0U) {
      std::this_thread::yield();
      continue;
    }
    const auto get = [this](SettingSlot slot) {
      return settings_[static_cast<std::size_t>(slot)].load(std::memory_order_relaxed);
    };
    dsp::AnalysisSettings result;
    result.transform = static_cast<dsp::TransformKind>(get(SettingSlot::Transform));
    result.window = static_cast<dsp::WindowKind>(get(SettingSlot::Window));
    result.sampleRate = decodeDouble(get(SettingSlot::SampleRate));
    result.fftSize = static_cast<std::size_t>(get(SettingSlot::FftSize));
    result.hopSize = static_cast<std::size_t>(get(SettingSlot::HopSize));
    result.bandCount = static_cast<std::size_t>(get(SettingSlot::BandCount));
    result.minFrequency = decodeDouble(get(SettingSlot::MinFrequency));
    result.maxFrequency = decodeDouble(get(SettingSlot::MaxFrequency));
    result.binsPerOctave = static_cast<int>(get(SettingSlot::BinsPerOctave));
    result.tuningCents = decodeDouble(get(SettingSlot::TuningCents));
    result.qScale = decodeDouble(get(SettingSlot::QScale));
    result.gamma = decodeDouble(get(SettingSlot::Gamma));
    result.maximumWindowSeconds = decodeDouble(get(SettingSlot::MaximumWindowSeconds));
    const std::uint64_t after = settingsGeneration_.load(std::memory_order_acquire);
    if (before == after) {
      return result;
    }
  }
}

void AnalysisEngine::run() {
  using namespace std::chrono_literals;
  std::uint64_t appliedGeneration = settingsGeneration_.load(std::memory_order_acquire);
  std::uint64_t observedGeneration = appliedGeneration;
  auto settingsStableSince = std::chrono::steady_clock::now();
  RealtimeConfiguration active = adaptForRealtime(readSettings());
  dsp::StreamingAnalyzer analyzer(active.settings);
  setStatus(active.status);
  std::array<float, 4096> block{};
  bool wasDisplayConsumerActive = true;

  while (!stopRequested_.load(std::memory_order_acquire)) {
    const std::uint64_t requestedGeneration = settingsGeneration_.load(std::memory_order_acquire);
    if (requestedGeneration != observedGeneration) {
      observedGeneration = requestedGeneration;
      settingsStableSince = std::chrono::steady_clock::now();
    }

    const bool displayConsumerActive = displayConsumerActive_.load(std::memory_order_acquire);
    if (!displayConsumerActive) {
      audioQueue_.discard(audioQueue_.size());
      if (wasDisplayConsumerActive) {
        analyzer.reset();
        wasDisplayConsumerActive = false;
      }
      std::this_thread::sleep_for(2ms);
      continue;
    }
    if (!wasDisplayConsumerActive) {
      analyzer.reset();
      wasDisplayConsumerActive = true;
    }

    if (observedGeneration != appliedGeneration &&
        std::chrono::steady_clock::now() - settingsStableSince >= 30ms) {
      try {
        active = adaptForRealtime(readSettings());
        analyzer.configure(active.settings);
        appliedGeneration = observedGeneration;
        setStatus(active.status);
      } catch (const std::exception& error) {
        // Retain the last valid analyzer. A subsequent parameter update retries.
        appliedGeneration = observedGeneration;
        setStatus(error.what());
      }
    }

    const std::size_t backlog = audioQueue_.size();
    const auto latencyLimit =
        static_cast<std::size_t>(std::max(4096.0, std::round(active.settings.sampleRate * 0.5)));
    if (backlog > latencyLimit) {
      const std::size_t discarded = audioQueue_.discard(backlog - latencyLimit / 2);
      droppedSamples_.fetch_add(discarded, std::memory_order_relaxed);
      analyzer.reset();
    }

    std::size_t count = 0;
    while (count < block.size() && audioQueue_.tryPop(block[count])) {
      ++count;
    }
    if (count == 0) {
      std::this_thread::sleep_for(1ms);
      continue;
    }

    try {
      analyzer.push(std::span<const float>(block).first(count),
                    [this](const dsp::SpectralFrame& frame) {
                      if (!frameQueue_.tryPush(frame)) {
                        droppedFrames_.fetch_add(1, std::memory_order_relaxed);
                      }
                      std::scoped_lock lock(frameMutex_);
                      latestFrame_ = frame;
                      ++latestSequence_;
                    });
    } catch (const std::exception& error) {
      setStatus(error.what());
    }
  }
}

void AnalysisEngine::setStatus(std::string message) {
  std::scoped_lock lock(statusMutex_);
  statusMessage_ = std::move(message);
}

}  // namespace sinesoda::runtime
