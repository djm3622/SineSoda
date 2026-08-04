#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "sinesoda/dsp/AnalysisSettings.h"
#include "sinesoda/dsp/SpectralFrame.h"
#include "sinesoda/runtime/SpscRing.h"

namespace sinesoda::runtime {

class AnalysisEngine {
 public:
  static constexpr std::size_t kAudioQueueCapacity = 1U << 19U;
  static constexpr std::size_t kFrameQueueCapacity = 32;

  explicit AnalysisEngine(const dsp::AnalysisSettings& settings = {});
  ~AnalysisEngine();

  AnalysisEngine(const AnalysisEngine&) = delete;
  AnalysisEngine& operator=(const AnalysisEngine&) = delete;

  // Called by the audio thread. This path never locks or allocates.
  void pushMono(const float* samples, std::size_t count) noexcept;
  void pushMono(const double* samples, std::size_t count) noexcept;
  void pushStereo(const float* left, const float* right, std::size_t count) noexcept;
  void pushStereo(const double* left, const double* right, std::size_t count) noexcept;

  // Requests are published atomically and applied by the analysis worker.
  void requestSettings(const dsp::AnalysisSettings& settings);

  // Returns true only when a frame newer than lastSequence was copied.
  [[nodiscard]] bool copyLatestFrame(dsp::SpectralFrame& destination,
                                     std::uint64_t& lastSequence) const;
  [[nodiscard]] bool tryPopFrame(dsp::SpectralFrame& destination);
  void setDisplayConsumerActive(bool active) noexcept;
  [[nodiscard]] std::size_t queuedAudioSamples() const noexcept;
  [[nodiscard]] std::size_t queuedFrames() const noexcept;
  [[nodiscard]] std::uint64_t droppedSamples() const noexcept;
  [[nodiscard]] std::uint64_t droppedFrames() const noexcept;
  [[nodiscard]] std::string statusMessage() const;

 private:
  enum class SettingSlot : std::size_t {
    Transform,
    Window,
    SampleRate,
    FftSize,
    HopSize,
    BandCount,
    MinFrequency,
    MaxFrequency,
    BinsPerOctave,
    TuningCents,
    QScale,
    Gamma,
    MaximumWindowSeconds,
    Count,
  };

  void publishSettings(const dsp::AnalysisSettings& settings) noexcept;
  [[nodiscard]] dsp::AnalysisSettings readSettings() const noexcept;
  void run();
  void setStatus(std::string message);

  SpscRing<float> audioQueue_{kAudioQueueCapacity};
  SpscRing<dsp::SpectralFrame> frameQueue_{kFrameQueueCapacity};
  std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(SettingSlot::Count)> settings_{};
  std::atomic<std::uint64_t> settingsGeneration_{0};
  std::atomic<std::uint64_t> droppedSamples_{0};
  std::atomic<std::uint64_t> droppedFrames_{0};
  std::atomic<bool> displayConsumerActive_{true};

  mutable std::mutex settingsWriterMutex_;

  mutable std::mutex frameMutex_;
  dsp::SpectralFrame latestFrame_;
  std::uint64_t latestSequence_ = 0;

  mutable std::mutex statusMutex_;
  std::string statusMessage_;

  std::atomic<bool> stopRequested_{false};
  std::thread worker_;
};

}  // namespace sinesoda::runtime
