#include <chrono>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <thread>
#include <vector>

#include "TestSupport.h"
#include "sinesoda/dsp/AnalysisSettings.h"
#include "sinesoda/dsp/SpectralFrame.h"
#include "sinesoda/runtime/AnalysisEngine.h"
#include "sinesoda/runtime/SpscRing.h"

SINESODA_TEST(SpscRingPreservesOrderAcrossWraparound) {
  sinesoda::runtime::SpscRing<int> ring(4);
  SINESODA_EXPECT_TRUE(ring.tryPush(10));
  SINESODA_EXPECT_TRUE(ring.tryPush(20));
  SINESODA_EXPECT_TRUE(ring.tryPush(30));
  SINESODA_EXPECT_TRUE(ring.tryPush(40));
  SINESODA_EXPECT_TRUE(!ring.tryPush(50));

  int value = 0;
  SINESODA_EXPECT_TRUE(ring.tryPop(value));
  SINESODA_EXPECT_EQ(value, 10);
  SINESODA_EXPECT_TRUE(ring.tryPop(value));
  SINESODA_EXPECT_EQ(value, 20);
  SINESODA_EXPECT_TRUE(ring.tryPush(50));
  SINESODA_EXPECT_TRUE(ring.tryPush(60));

  for (const int expected : {30, 40, 50, 60}) {
    SINESODA_EXPECT_TRUE(ring.tryPop(value));
    SINESODA_EXPECT_EQ(value, expected);
  }
  SINESODA_EXPECT_TRUE(!ring.tryPop(value));
}

SINESODA_TEST(SpscRingCanDiscardOldestValues) {
  sinesoda::runtime::SpscRing<int> ring(8);
  for (int value = 0; value < 6; ++value) {
    SINESODA_EXPECT_TRUE(ring.tryPush(value));
  }
  SINESODA_EXPECT_EQ(ring.discard(4), 4U);
  SINESODA_EXPECT_EQ(ring.size(), 2U);
  int value = 0;
  SINESODA_EXPECT_TRUE(ring.tryPop(value));
  SINESODA_EXPECT_EQ(value, 4);
  SINESODA_EXPECT_EQ(ring.discard(10), 1U);
  SINESODA_EXPECT_EQ(ring.size(), 0U);
}

SINESODA_TEST(SpscRingRejectsInvalidCapacity) {
  SINESODA_EXPECT_THROW(sinesoda::runtime::SpscRing<float>(3), std::invalid_argument);
}

SINESODA_TEST(AnalysisEngineProducesFramesOffTheCallingThread) {
  using namespace std::chrono_literals;
  sinesoda::dsp::AnalysisSettings settings;
  settings.sampleRate = 48000.0;
  settings.fftSize = 512;
  settings.hopSize = 128;
  settings.minFrequency = 50.0;
  settings.maxFrequency = 8000.0;
  sinesoda::runtime::AnalysisEngine engine(settings);

  std::vector<float> signal(4096);
  for (std::size_t index = 0; index < signal.size(); ++index) {
    signal[index] =
        static_cast<float>(0.7 * std::sin(2.0 * std::numbers::pi_v<double> * 1000.0 *
                                          static_cast<double>(index) / settings.sampleRate));
  }
  engine.pushMono(signal.data(), signal.size());

  sinesoda::dsp::SpectralFrame frame;
  std::uint64_t sequence = 0;
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (!engine.copyLatestFrame(frame, sequence) && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(2ms);
  }

  SINESODA_EXPECT_TRUE(sequence != 0);
  SINESODA_EXPECT_TRUE(!frame.magnitudes.empty());
  SINESODA_EXPECT_EQ(engine.droppedSamples(), 0U);
  SINESODA_EXPECT_TRUE(engine.statusMessage().empty());
}

SINESODA_TEST(AnalysisEngineQueuesEveryHopWithoutDrops) {
  using namespace std::chrono_literals;
  sinesoda::dsp::AnalysisSettings settings;
  settings.sampleRate = 48000.0;
  settings.fftSize = 256;
  settings.hopSize = 128;
  settings.minFrequency = 100.0;
  settings.maxFrequency = 8000.0;
  sinesoda::runtime::AnalysisEngine engine(settings);

  constexpr std::size_t sampleCount = 4096;
  std::vector<float> signal(sampleCount, 0.25F);
  engine.pushMono(signal.data(), signal.size());

  sinesoda::dsp::SpectralFrame latest;
  std::uint64_t sequence = 0;
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (sequence < sampleCount / settings.hopSize && std::chrono::steady_clock::now() < deadline) {
    static_cast<void>(engine.copyLatestFrame(latest, sequence));
    std::this_thread::sleep_for(2ms);
  }

  std::size_t queuedFrames = 0;
  while (engine.tryPopFrame(latest)) {
    ++queuedFrames;
  }
  SINESODA_EXPECT_EQ(sequence, sampleCount / settings.hopSize);
  SINESODA_EXPECT_EQ(queuedFrames, sampleCount / settings.hopSize);
  SINESODA_EXPECT_EQ(engine.droppedSamples(), 0U);
  SINESODA_EXPECT_EQ(engine.droppedFrames(), 0U);
}

SINESODA_TEST(AnalysisEngineAcceptsRuntimeReconfiguration) {
  using namespace std::chrono_literals;
  sinesoda::dsp::AnalysisSettings settings;
  settings.fftSize = 256;
  settings.hopSize = 64;
  settings.minFrequency = 100.0;
  settings.maxFrequency = 10000.0;
  sinesoda::runtime::AnalysisEngine engine(settings);

  settings.transform = sinesoda::dsp::TransformKind::Mel;
  settings.bandCount = 32;
  engine.requestSettings(settings);
  std::vector<float> signal(2048, 0.25F);
  engine.pushMono(signal.data(), signal.size());

  sinesoda::dsp::SpectralFrame frame;
  std::uint64_t sequence = 0;
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while ((!engine.copyLatestFrame(frame, sequence) ||
          frame.transform != sinesoda::dsp::TransformKind::Mel) &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(2ms);
  }
  SINESODA_EXPECT_EQ(frame.transform, sinesoda::dsp::TransformKind::Mel);
  SINESODA_EXPECT_EQ(frame.magnitudes.size(), 32U);
}

SINESODA_TEST(AnalysisEngineDoesNotRetainFramesWhileDisplayIsClosed) {
  using namespace std::chrono_literals;
  sinesoda::dsp::AnalysisSettings settings;
  settings.fftSize = 8192;
  settings.hopSize = 128;
  settings.minFrequency = 20.0;
  settings.maxFrequency = 20000.0;
  sinesoda::runtime::AnalysisEngine engine(settings);
  engine.setDisplayConsumerActive(false);

  constexpr std::size_t frameCount = sinesoda::runtime::AnalysisEngine::kFrameQueueCapacity * 3;
  std::vector<float> signal(frameCount * settings.hopSize, 0.25F);
  engine.pushMono(signal.data(), signal.size());

  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (engine.queuedAudioSamples() != 0 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(2ms);
  }

  SINESODA_EXPECT_EQ(engine.queuedAudioSamples(), 0U);
  SINESODA_EXPECT_EQ(engine.queuedFrames(), 0U);
  SINESODA_EXPECT_EQ(engine.droppedFrames(), 0U);
}

SINESODA_TEST(AnalysisEngineBoundsHighResolutionDisplayBacklog) {
  using namespace std::chrono_literals;
  sinesoda::dsp::AnalysisSettings settings;
  settings.fftSize = 8192;
  settings.hopSize = 128;
  settings.minFrequency = 20.0;
  settings.maxFrequency = 20000.0;
  sinesoda::runtime::AnalysisEngine engine(settings);

  constexpr std::size_t frameCount = sinesoda::runtime::AnalysisEngine::kFrameQueueCapacity * 3;
  std::vector<float> signal(frameCount * settings.hopSize, 0.25F);
  engine.pushMono(signal.data(), signal.size());

  sinesoda::dsp::SpectralFrame latest;
  std::uint64_t sequence = 0;
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (sequence < frameCount && std::chrono::steady_clock::now() < deadline) {
    static_cast<void>(engine.copyLatestFrame(latest, sequence));
    std::this_thread::sleep_for(2ms);
  }

  SINESODA_EXPECT_EQ(sequence, frameCount);
  SINESODA_EXPECT_EQ(engine.queuedFrames(), sinesoda::runtime::AnalysisEngine::kFrameQueueCapacity);
  SINESODA_EXPECT_EQ(engine.droppedFrames(),
                     frameCount - sinesoda::runtime::AnalysisEngine::kFrameQueueCapacity);
}
