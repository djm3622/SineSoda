#include "sinesoda/dsp/Transforms.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>
#include <vector>

#include "sinesoda/dsp/Fft.h"
#include "sinesoda/dsp/FrequencyScale.h"

namespace sinesoda::dsp {
namespace {

constexpr double kTwoPi = 2.0 * std::numbers::pi_v<double>;

std::vector<float> makeWindow(std::size_t size, WindowKind kind) {
  std::vector<float> window(size, 1.0F);
  if (size == 1 || kind == WindowKind::Rectangular) {
    return window;
  }

  const double denominator = static_cast<double>(size - 1);
  for (std::size_t index = 0; index < size; ++index) {
    const double phase = kTwoPi * static_cast<double>(index) / denominator;
    double value = 1.0;
    switch (kind) {
      case WindowKind::Hann:
        value = 0.5 - 0.5 * std::cos(phase);
        break;
      case WindowKind::BlackmanHarris:
        value = 0.35875 - 0.48829 * std::cos(phase) + 0.14128 * std::cos(2.0 * phase) -
                0.01168 * std::cos(3.0 * phase);
        break;
      case WindowKind::Hamming:
        value = 0.54 - 0.46 * std::cos(phase);
        break;
      case WindowKind::FlatTop:
        value = 0.21557895 - 0.41663158 * std::cos(phase) + 0.277263158 * std::cos(2.0 * phase) -
                0.083578947 * std::cos(3.0 * phase) + 0.006947368 * std::cos(4.0 * phase);
        break;
      case WindowKind::Rectangular:
        value = 1.0;
        break;
    }
    window[index] = static_cast<float>(value);
  }
  return window;
}

class StftKernel {
 public:
  explicit StftKernel(const AnalysisSettings& settings)
      : settings_(settings),
        fft_(settings.fftSize),
        window_(makeWindow(settings.fftSize, settings.window)),
        windowed_(settings.fftSize),
        spectrum_(settings.fftSize),
        magnitudes_(settings.fftSize / 2 + 1) {
    double sum = 0.0;
    for (const float value : window_) {
      sum += static_cast<double>(value);
    }
    coherentGainSum_ = static_cast<float>(sum);
  }

  std::span<const float> analyze(std::span<const float> input) {
    if (input.size() < settings_.fftSize) {
      throw std::invalid_argument("STFT input is shorter than the FFT size");
    }
    const auto frame = input.last(settings_.fftSize);
    for (std::size_t index = 0; index < settings_.fftSize; ++index) {
      windowed_[index] = frame[index] * window_[index];
    }
    fft_.forward(windowed_, spectrum_);

    const std::size_t nyquistBin = settings_.fftSize / 2;
    for (std::size_t bin = 0; bin <= nyquistBin; ++bin) {
      const float oneSidedScale = (bin == 0 || bin == nyquistBin) ? 1.0F : 2.0F;
      magnitudes_[bin] = oneSidedScale * std::abs(spectrum_[bin]) / coherentGainSum_;
    }
    return magnitudes_;
  }

 private:
  AnalysisSettings settings_;
  Fft fft_;
  std::vector<float> window_;
  std::vector<float> windowed_;
  std::vector<std::complex<float>> spectrum_;
  std::vector<float> magnitudes_;
  float coherentGainSum_ = 1.0F;
};

class StftTransform final : public IFrameTransform {
 public:
  explicit StftTransform(const AnalysisSettings& settings)
      : settings_(settings), kernel_(settings) {
    const double binWidth = settings_.sampleRate / static_cast<double>(settings_.fftSize);
    firstBin_ = static_cast<std::size_t>(std::ceil(settings_.minFrequency / binWidth));
    lastBin_ = std::min(settings_.fftSize / 2,
                        static_cast<std::size_t>(std::floor(settings_.maxFrequency / binWidth)));
    if (firstBin_ > lastBin_) {
      throw std::invalid_argument("STFT frequency range contains no FFT bins");
    }
    centers_.reserve(lastBin_ - firstBin_ + 1);
    bandwidths_.reserve(lastBin_ - firstBin_ + 1);
    for (std::size_t bin = firstBin_; bin <= lastBin_; ++bin) {
      centers_.push_back(static_cast<float>(static_cast<double>(bin) * binWidth));
      bandwidths_.push_back(static_cast<float>(binWidth));
    }
  }

  [[nodiscard]] std::size_t requiredSamples() const noexcept override { return settings_.fftSize; }

  void analyze(std::span<const float> input, SpectralFrame& output) override {
    const auto allMagnitudes = kernel_.analyze(input);
    output.transform = TransformKind::Stft;
    output.timeOffsetsSeconds.clear();
    output.centerFrequencies = centers_;
    output.bandwidths = bandwidths_;
    output.magnitudes.assign(allMagnitudes.begin() + static_cast<std::ptrdiff_t>(firstBin_),
                             allMagnitudes.begin() + static_cast<std::ptrdiff_t>(lastBin_ + 1));
  }

 private:
  AnalysisSettings settings_;
  StftKernel kernel_;
  std::size_t firstBin_ = 0;
  std::size_t lastBin_ = 0;
  std::vector<float> centers_;
  std::vector<float> bandwidths_;
};

FrequencyScale frequencyScaleFor(TransformKind kind) {
  switch (kind) {
    case TransformKind::LogFilterbank:
      return FrequencyScale::Logarithmic;
    case TransformKind::Mel:
      return FrequencyScale::Mel;
    case TransformKind::Bark:
      return FrequencyScale::Bark;
    case TransformKind::Erb:
      return FrequencyScale::Erb;
    default:
      throw std::invalid_argument("transform is not an STFT filterbank");
  }
}

struct SparseBand {
  std::size_t firstBin = 0;
  std::vector<float> weights;
  float weightSum = 0.0F;
};

class FilterbankTransform final : public IFrameTransform {
 public:
  explicit FilterbankTransform(const AnalysisSettings& settings)
      : settings_(settings), kernel_(settings), scale_(frequencyScaleFor(settings.transform)) {
    const double low = scaleFromHertz(settings_.minFrequency, scale_);
    const double high = scaleFromHertz(settings_.maxFrequency, scale_);
    const std::size_t pointCount = settings_.bandCount + 2;
    std::vector<double> points(pointCount);
    for (std::size_t index = 0; index < pointCount; ++index) {
      const double fraction = static_cast<double>(index) / static_cast<double>(pointCount - 1);
      points[index] = hertzFromScale(low + fraction * (high - low), scale_);
    }

    const double binWidth = settings_.sampleRate / static_cast<double>(settings_.fftSize);
    centers_.reserve(settings_.bandCount);
    bandwidths_.reserve(settings_.bandCount);
    bands_.reserve(settings_.bandCount);

    for (std::size_t bandIndex = 0; bandIndex < settings_.bandCount; ++bandIndex) {
      const double left = points[bandIndex];
      const double center = points[bandIndex + 1];
      const double right = points[bandIndex + 2];
      const std::size_t first = static_cast<std::size_t>(std::ceil(left / binWidth));
      const std::size_t last =
          std::min(settings_.fftSize / 2, static_cast<std::size_t>(std::floor(right / binWidth)));

      SparseBand band;
      band.firstBin = first;
      if (last >= first) {
        band.weights.reserve(last - first + 1);
        for (std::size_t bin = first; bin <= last; ++bin) {
          const double frequency = static_cast<double>(bin) * binWidth;
          double weight = 0.0;
          if (frequency <= center) {
            weight = (frequency - left) / (center - left);
          } else {
            weight = (right - frequency) / (right - center);
          }
          const float clamped = static_cast<float>(std::clamp(weight, 0.0, 1.0));
          band.weights.push_back(clamped);
          band.weightSum += clamped;
        }
      }
      if (band.weightSum <= std::numeric_limits<float>::epsilon()) {
        const auto nearest = std::min(settings_.fftSize / 2,
                                      static_cast<std::size_t>(std::llround(center / binWidth)));
        band.firstBin = nearest;
        band.weights.assign(1, 1.0F);
        band.weightSum = 1.0F;
      }
      centers_.push_back(static_cast<float>(center));
      bandwidths_.push_back(static_cast<float>(right - left));
      bands_.push_back(std::move(band));
    }
  }

  [[nodiscard]] std::size_t requiredSamples() const noexcept override { return settings_.fftSize; }

  void analyze(std::span<const float> input, SpectralFrame& output) override {
    const auto fftMagnitudes = kernel_.analyze(input);
    output.transform = settings_.transform;
    output.timeOffsetsSeconds.clear();
    output.centerFrequencies = centers_;
    output.bandwidths = bandwidths_;
    output.magnitudes.resize(bands_.size());
    for (std::size_t bandIndex = 0; bandIndex < bands_.size(); ++bandIndex) {
      const auto& band = bands_[bandIndex];
      double energy = 0.0;
      for (std::size_t index = 0; index < band.weights.size(); ++index) {
        const float magnitude = fftMagnitudes[band.firstBin + index];
        energy += static_cast<double>(band.weights[index]) * magnitude * magnitude;
      }
      output.magnitudes[bandIndex] =
          static_cast<float>(std::sqrt(energy / static_cast<double>(band.weightSum)));
    }
  }

 private:
  AnalysisSettings settings_;
  StftKernel kernel_;
  FrequencyScale scale_;
  std::vector<SparseBand> bands_;
  std::vector<float> centers_;
  std::vector<float> bandwidths_;
};

struct ComplexKernel {
  std::vector<std::complex<float>> coefficients;
};

class ConstantQTransform final : public IFrameTransform {
 public:
  explicit ConstantQTransform(const AnalysisSettings& settings) : settings_(settings) {
    const bool variableQ = settings_.transform == TransformKind::Vqt;
    const double gamma = variableQ ? settings_.gamma : 0.0;
    const double bins = static_cast<double>(settings_.binsPerOctave);
    const double q = settings_.qScale / (std::exp2(1.0 / bins) - 1.0);
    const double tuningRatio = std::exp2(settings_.tuningCents / 1200.0);
    const auto maximumWindow =
        static_cast<std::size_t>(std::ceil(settings_.maximumWindowSeconds * settings_.sampleRate));

    for (std::size_t index = 0; index < settings_.bandCount; ++index) {
      const double frequency =
          settings_.minFrequency * tuningRatio * std::exp2(static_cast<double>(index) / bins);
      if (frequency > settings_.maxFrequency || frequency >= settings_.sampleRate * 0.5) {
        break;
      }
      const double bandwidth = frequency / q + gamma;
      std::size_t windowLength =
          static_cast<std::size_t>(std::ceil(settings_.sampleRate / bandwidth));
      windowLength = std::clamp<std::size_t>(windowLength, 8, maximumWindow);

      const auto window = makeWindow(windowLength, settings_.window);
      double windowSum = 0.0;
      for (const float value : window) {
        windowSum += static_cast<double>(value);
      }
      const double normalization = 2.0 / windowSum;
      ComplexKernel kernel;
      kernel.coefficients.resize(windowLength);
      for (std::size_t sample = 0; sample < windowLength; ++sample) {
        const double phase =
            -kTwoPi * frequency * static_cast<double>(sample) / settings_.sampleRate;
        kernel.coefficients[sample] =
            static_cast<float>(normalization * static_cast<double>(window[sample])) *
            std::complex<float>(static_cast<float>(std::cos(phase)),
                                static_cast<float>(std::sin(phase)));
      }
      requiredSamples_ = std::max(requiredSamples_, windowLength);
      centers_.push_back(static_cast<float>(frequency));
      bandwidths_.push_back(static_cast<float>(bandwidth));
      kernels_.push_back(std::move(kernel));
    }
    if (kernels_.empty()) {
      throw std::invalid_argument("CQT frequency range contains no bands");
    }
  }

  [[nodiscard]] std::size_t requiredSamples() const noexcept override { return requiredSamples_; }

  void analyze(std::span<const float> input, SpectralFrame& output) override {
    if (input.size() < requiredSamples_) {
      throw std::invalid_argument("CQT input is shorter than the longest kernel");
    }
    output.transform = settings_.transform;
    output.timeOffsetsSeconds.clear();
    output.centerFrequencies = centers_;
    output.bandwidths = bandwidths_;
    output.magnitudes.resize(kernels_.size());

    for (std::size_t band = 0; band < kernels_.size(); ++band) {
      const auto& coefficients = kernels_[band].coefficients;
      const auto signal = input.last(coefficients.size());
      std::complex<double> sum(0.0, 0.0);
      for (std::size_t sample = 0; sample < coefficients.size(); ++sample) {
        const auto coefficient = coefficients[sample];
        sum += static_cast<double>(signal[sample]) *
               std::complex<double>(static_cast<double>(coefficient.real()),
                                    static_cast<double>(coefficient.imag()));
      }
      output.magnitudes[band] = static_cast<float>(std::abs(sum));
    }
  }

 private:
  AnalysisSettings settings_;
  std::size_t requiredSamples_ = 0;
  std::vector<ComplexKernel> kernels_;
  std::vector<float> centers_;
  std::vector<float> bandwidths_;
};

std::vector<float> makeGeometricCenters(const AnalysisSettings& settings) {
  std::vector<float> centers;
  centers.reserve(settings.bandCount);
  const double tuningRatio = std::exp2(settings.tuningCents / 1200.0);
  for (std::size_t index = 0; index < settings.bandCount; ++index) {
    const double frequency =
        settings.minFrequency * tuningRatio *
        std::exp2(static_cast<double>(index) / static_cast<double>(settings.binsPerOctave));
    if (frequency > settings.maxFrequency || frequency >= settings.sampleRate * 0.5) {
      break;
    }
    centers.push_back(static_cast<float>(frequency));
  }
  if (centers.empty()) {
    throw std::invalid_argument("transform frequency range contains no bands");
  }
  return centers;
}

enum class AnalyticBankKind {
  Morlet,
  Morse,
  Stockwell,
};

float analyticResponse(AnalyticBankKind kind, double frequencyRatio, double qScale) {
  if (frequencyRatio <= 0.0) {
    return 0.0F;
  }
  double exponent = 0.0;
  switch (kind) {
    case AnalyticBankKind::Morlet: {
      const double omega0 = 6.0 * qScale;
      const double distance = omega0 * (frequencyRatio - 1.0);
      exponent = -0.5 * distance * distance;
      break;
    }
    case AnalyticBankKind::Morse: {
      // Generalized Morse spectrum with gamma=3 and beta controlling
      // time-frequency concentration. The expression is peak-normalized.
      constexpr double gamma = 3.0;
      const double beta = 20.0 * qScale;
      exponent = beta * std::log(frequencyRatio) -
                 (beta / gamma) * (std::pow(frequencyRatio, gamma) - 1.0);
      break;
    }
    case AnalyticBankKind::Stockwell: {
      // The canonical S-transform Gaussian has exp(-2*pi^2*df^2/f^2).
      const double distance = kTwoPi * qScale * (frequencyRatio - 1.0);
      exponent = -0.5 * distance * distance;
      break;
    }
  }
  if (exponent < -80.0) {
    return 0.0F;
  }
  return static_cast<float>(std::exp(exponent));
}

float responseBandwidth(AnalyticBankKind kind, float center, double qScale) {
  constexpr float halfAmplitude = 0.70710678118F;
  double lowRatio = 1.0;
  double highRatio = 1.0;
  constexpr int steps = 4096;
  for (int index = 1; index <= steps; ++index) {
    const double ratio = 1.0 - 0.999 * static_cast<double>(index) / static_cast<double>(steps);
    if (analyticResponse(kind, ratio, qScale) < halfAmplitude) {
      lowRatio = ratio;
      break;
    }
  }
  for (int index = 1; index <= steps; ++index) {
    const double ratio = 1.0 + 3.0 * static_cast<double>(index) / static_cast<double>(steps);
    if (analyticResponse(kind, ratio, qScale) < halfAmplitude) {
      highRatio = ratio;
      break;
    }
  }
  return static_cast<float>(static_cast<double>(center) * (highRatio - lowRatio));
}

class AnalyticWaveletBankTransform final : public IFrameTransform {
 public:
  AnalyticWaveletBankTransform(const AnalysisSettings& settings, AnalyticBankKind bankKind,
                               bool synchrosqueezed)
      : settings_(settings),
        bankKind_(bankKind),
        synchrosqueezed_(synchrosqueezed),
        fft_(settings.fftSize),
        spectrum_(settings.fftSize),
        phasors_(settings.fftSize / 2 + 1),
        angularFrequencies_(settings.fftSize / 2 + 1),
        centers_(makeGeometricCenters(settings)) {
    const double binWidth = settings_.sampleRate / static_cast<double>(settings_.fftSize);
    const double endpoint = static_cast<double>(settings_.fftSize - 1);
    for (std::size_t bin = 0; bin < phasors_.size(); ++bin) {
      const double phase =
          kTwoPi * static_cast<double>(bin) * endpoint / static_cast<double>(settings_.fftSize);
      phasors_[bin] = {static_cast<float>(std::cos(phase)), static_cast<float>(std::sin(phase))};
      angularFrequencies_[bin] = kTwoPi * static_cast<double>(bin) * binWidth;
    }

    responses_.reserve(centers_.size());
    bandwidths_.reserve(centers_.size());
    for (const float center : centers_) {
      std::vector<float> response(phasors_.size(), 0.0F);
      for (std::size_t bin = 1; bin < response.size(); ++bin) {
        const double frequency = static_cast<double>(bin) * binWidth;
        response[bin] = analyticResponse(bankKind_, frequency / center, settings_.qScale);
      }
      responses_.push_back(std::move(response));
      bandwidths_.push_back(responseBandwidth(bankKind_, center, settings_.qScale));
    }
  }

  [[nodiscard]] std::size_t requiredSamples() const noexcept override { return settings_.fftSize; }

  void analyze(std::span<const float> input, SpectralFrame& output) override {
    if (input.size() < settings_.fftSize) {
      throw std::invalid_argument("wavelet input is shorter than the FFT size");
    }
    fft_.forward(input.last(settings_.fftSize), spectrum_);
    output.transform = transformKind();
    output.centerFrequencies = centers_;
    output.bandwidths = bandwidths_;
    output.timeOffsetsSeconds.clear();
    output.magnitudes.assign(centers_.size(), 0.0F);

    const double reconstructionScale = 2.0 / static_cast<double>(settings_.fftSize);
    for (std::size_t band = 0; band < responses_.size(); ++band) {
      std::complex<double> coefficient(0.0, 0.0);
      std::complex<double> derivative(0.0, 0.0);
      for (std::size_t bin = 1; bin < phasors_.size(); ++bin) {
        const double response = responses_[band][bin];
        if (response == 0.0) {
          continue;
        }
        const std::complex<double> spectralValue(static_cast<double>(spectrum_[bin].real()),
                                                 static_cast<double>(spectrum_[bin].imag()));
        const std::complex<double> phasor(static_cast<double>(phasors_[bin].real()),
                                          static_cast<double>(phasors_[bin].imag()));
        const std::complex<double> contribution = spectralValue * phasor * response;
        coefficient += contribution;
        derivative += std::complex<double>(0.0, angularFrequencies_[bin]) * contribution;
      }
      coefficient *= reconstructionScale;
      derivative *= reconstructionScale;

      const float magnitude = static_cast<float>(std::abs(coefficient));
      if (!synchrosqueezed_) {
        output.magnitudes[band] = magnitude;
        continue;
      }
      if (std::norm(coefficient) <= 1.0e-16) {
        continue;
      }
      const double reassignedFrequency = std::imag(derivative / coefficient) / kTwoPi;
      if (reassignedFrequency < centers_.front() || reassignedFrequency > centers_.back()) {
        continue;
      }
      const auto upper = std::lower_bound(centers_.begin(), centers_.end(),
                                          static_cast<float>(reassignedFrequency));
      std::size_t destination = static_cast<std::size_t>(upper - centers_.begin());
      if (destination > 0 && destination < centers_.size()) {
        const double upperDistance = centers_[destination] - reassignedFrequency;
        const double lowerDistance = reassignedFrequency - centers_[destination - 1];
        if (lowerDistance < upperDistance) {
          --destination;
        }
      } else if (destination == centers_.size()) {
        destination = centers_.size() - 1;
      }
      // Peak aggregation preserves amplitude while concentrating duplicated
      // scale responses onto the instantaneous-frequency ridge.
      output.magnitudes[destination] = std::max(output.magnitudes[destination], magnitude);
    }
  }

 private:
  [[nodiscard]] TransformKind transformKind() const noexcept {
    if (synchrosqueezed_) {
      return TransformKind::SynchrosqueezedCwt;
    }
    switch (bankKind_) {
      case AnalyticBankKind::Morlet:
        return TransformKind::MorletCwt;
      case AnalyticBankKind::Morse:
        return TransformKind::MorseCwt;
      case AnalyticBankKind::Stockwell:
        return TransformKind::Stockwell;
    }
    return TransformKind::MorletCwt;
  }

  AnalysisSettings settings_;
  AnalyticBankKind bankKind_;
  bool synchrosqueezed_;
  Fft fft_;
  std::vector<std::complex<float>> spectrum_;
  std::vector<std::complex<float>> phasors_;
  std::vector<double> angularFrequencies_;
  std::vector<float> centers_;
  std::vector<float> bandwidths_;
  std::vector<std::vector<float>> responses_;
};

class ReassignedStftTransform final : public IFrameTransform {
 public:
  explicit ReassignedStftTransform(const AnalysisSettings& settings)
      : settings_(settings),
        fft_(settings.fftSize),
        window_(makeWindow(settings.fftSize, settings.window)),
        windowed_(settings.fftSize),
        derivativeWindowed_(settings.fftSize),
        timeWindowed_(settings.fftSize),
        spectrum_(settings.fftSize),
        derivativeSpectrum_(settings.fftSize),
        timeSpectrum_(settings.fftSize) {
    const double binWidth = settings_.sampleRate / static_cast<double>(settings_.fftSize);
    firstBin_ = static_cast<std::size_t>(std::ceil(settings_.minFrequency / binWidth));
    lastBin_ = std::min(settings_.fftSize / 2,
                        static_cast<std::size_t>(std::floor(settings_.maxFrequency / binWidth)));
    if (firstBin_ > lastBin_) {
      throw std::invalid_argument("reassigned STFT range contains no FFT bins");
    }
    double windowSum = 0.0;
    for (const float value : window_) {
      windowSum += value;
    }
    coherentGainSum_ = windowSum;
    const double center = 0.5 * static_cast<double>(settings_.fftSize - 1);
    for (std::size_t index = 0; index < settings_.fftSize; ++index) {
      const double phase =
          kTwoPi * static_cast<double>(index) / static_cast<double>(settings_.fftSize - 1);
      derivativeWindow_.push_back(static_cast<float>(std::numbers::pi_v<double> * std::sin(phase) /
                                                     static_cast<double>(settings_.fftSize - 1)));
      timeWindow_.push_back(static_cast<float>((static_cast<double>(index) - center) *
                                               static_cast<double>(window_[index])));
    }
    const float bandwidth = static_cast<float>(binWidth);
    for (std::size_t bin = firstBin_; bin <= lastBin_; ++bin) {
      centers_.push_back(static_cast<float>(static_cast<double>(bin) * binWidth));
      bandwidths_.push_back(bandwidth);
    }
  }

  [[nodiscard]] std::size_t requiredSamples() const noexcept override { return settings_.fftSize; }

  void analyze(std::span<const float> input, SpectralFrame& output) override {
    if (input.size() < settings_.fftSize) {
      throw std::invalid_argument("reassigned STFT input is shorter than FFT size");
    }
    const auto frame = input.last(settings_.fftSize);
    for (std::size_t index = 0; index < settings_.fftSize; ++index) {
      windowed_[index] = frame[index] * window_[index];
      derivativeWindowed_[index] = frame[index] * derivativeWindow_[index];
      timeWindowed_[index] = frame[index] * timeWindow_[index];
    }
    fft_.forward(windowed_, spectrum_);
    fft_.forward(derivativeWindowed_, derivativeSpectrum_);
    fft_.forward(timeWindowed_, timeSpectrum_);

    output.transform = TransformKind::ReassignedStft;
    output.centerFrequencies = centers_;
    output.bandwidths = bandwidths_;
    output.magnitudes.assign(centers_.size(), 0.0F);
    output.timeOffsetsSeconds.assign(centers_.size(), 0.0F);
    std::vector<double> timeWeights(centers_.size(), 0.0);
    const double binWidth = settings_.sampleRate / static_cast<double>(settings_.fftSize);
    const double centerToEnd =
        0.5 * static_cast<double>(settings_.fftSize - 1) / settings_.sampleRate;

    for (std::size_t bin = firstBin_; bin <= lastBin_; ++bin) {
      const std::complex<double> coefficient(spectrum_[bin].real(), spectrum_[bin].imag());
      const double energy = std::norm(coefficient);
      if (energy <= 1.0e-18) {
        continue;
      }
      const std::complex<double> derivative(derivativeSpectrum_[bin].real(),
                                            derivativeSpectrum_[bin].imag());
      const std::complex<double> timeWeighted(timeSpectrum_[bin].real(), timeSpectrum_[bin].imag());
      const double reassignedFrequency =
          static_cast<double>(bin) * binWidth -
          settings_.sampleRate * std::imag(derivative / coefficient) / kTwoPi;
      if (reassignedFrequency < settings_.minFrequency ||
          reassignedFrequency > settings_.maxFrequency) {
        continue;
      }
      const auto destinationBin =
          static_cast<std::size_t>(std::llround(reassignedFrequency / binWidth));
      if (destinationBin < firstBin_ || destinationBin > lastBin_) {
        continue;
      }
      const std::size_t destination = destinationBin - firstBin_;
      const double oneSidedScale = (bin == 0 || bin == settings_.fftSize / 2) ? 1.0 : 2.0;
      const double amplitude = oneSidedScale * std::abs(coefficient) / coherentGainSum_;
      output.magnitudes[destination] =
          static_cast<float>(std::hypot(output.magnitudes[destination], amplitude));
      const double relativeTime =
          std::real(timeWeighted / coefficient) / settings_.sampleRate - centerToEnd;
      output.timeOffsetsSeconds[destination] += static_cast<float>(relativeTime * amplitude);
      timeWeights[destination] += amplitude;
    }
    for (std::size_t index = 0; index < timeWeights.size(); ++index) {
      if (timeWeights[index] > 0.0) {
        output.timeOffsetsSeconds[index] =
            static_cast<float>(output.timeOffsetsSeconds[index] / timeWeights[index]);
      }
    }
  }

 private:
  AnalysisSettings settings_;
  Fft fft_;
  std::vector<float> window_;
  std::vector<float> derivativeWindow_;
  std::vector<float> timeWindow_;
  std::vector<float> windowed_;
  std::vector<float> derivativeWindowed_;
  std::vector<float> timeWindowed_;
  std::vector<std::complex<float>> spectrum_;
  std::vector<std::complex<float>> derivativeSpectrum_;
  std::vector<std::complex<float>> timeSpectrum_;
  std::size_t firstBin_ = 0;
  std::size_t lastBin_ = 0;
  double coherentGainSum_ = 1.0;
  std::vector<float> centers_;
  std::vector<float> bandwidths_;
};

class ChromaTransform final : public IFrameTransform {
 public:
  explicit ChromaTransform(const AnalysisSettings& settings)
      : settings_(settings), kernel_(settings) {
    constexpr double c4 = 261.6255653005986;
    for (int pitchClass = 0; pitchClass < 12; ++pitchClass) {
      centers_.push_back(static_cast<float>(c4 * std::exp2(pitchClass / 12.0)));
      bandwidths_.push_back(0.0F);
    }
  }

  [[nodiscard]] std::size_t requiredSamples() const noexcept override { return settings_.fftSize; }

  void analyze(std::span<const float> input, SpectralFrame& output) override {
    const auto magnitudes = kernel_.analyze(input);
    std::array<double, 12> powers{};
    const double binWidth = settings_.sampleRate / static_cast<double>(settings_.fftSize);
    for (std::size_t bin = 1; bin < magnitudes.size(); ++bin) {
      const double frequency = static_cast<double>(bin) * binWidth;
      if (frequency < settings_.minFrequency || frequency > settings_.maxFrequency) {
        continue;
      }
      const double midi =
          69.0 + 12.0 * std::log2(frequency / 440.0) - settings_.tuningCents / 100.0;
      const double lowerNote = std::floor(midi);
      const double fraction = midi - lowerNote;
      const int lowerClass = (static_cast<int>(lowerNote) % 12 + 12) % 12;
      const int upperClass = (lowerClass + 1) % 12;
      const double power = static_cast<double>(magnitudes[bin]) * magnitudes[bin];
      powers[static_cast<std::size_t>(lowerClass)] += (1.0 - fraction) * power;
      powers[static_cast<std::size_t>(upperClass)] += fraction * power;
    }
    output.transform = TransformKind::Chroma;
    output.centerFrequencies = centers_;
    output.bandwidths = bandwidths_;
    output.timeOffsetsSeconds.clear();
    output.magnitudes.resize(12);
    for (std::size_t pitchClass = 0; pitchClass < powers.size(); ++pitchClass) {
      output.magnitudes[pitchClass] = static_cast<float>(std::sqrt(powers[pitchClass]));
    }
  }

 private:
  AnalysisSettings settings_;
  StftKernel kernel_;
  std::vector<float> centers_;
  std::vector<float> bandwidths_;
};

class HaarWaveletPacketTransform final : public IFrameTransform {
 public:
  explicit HaarWaveletPacketTransform(const AnalysisSettings& settings) : settings_(settings) {
    leafCount_ = 1;
    while (leafCount_ * 2 <= settings_.bandCount && leafCount_ * 2 <= settings_.fftSize) {
      leafCount_ *= 2;
    }
    const double bandWidth = settings_.sampleRate * 0.5 / static_cast<double>(leafCount_);
    for (std::size_t band = 0; band < leafCount_; ++band) {
      centers_.push_back(static_cast<float>((static_cast<double>(band) + 0.5) * bandWidth));
      bandwidths_.push_back(static_cast<float>(bandWidth));
    }
  }

  [[nodiscard]] std::size_t requiredSamples() const noexcept override { return settings_.fftSize; }

  void analyze(std::span<const float> input, SpectralFrame& output) override {
    if (input.size() < settings_.fftSize) {
      throw std::invalid_argument("wavelet packet input is shorter than FFT size");
    }
    std::vector<std::vector<float>> nodes;
    nodes.emplace_back(input.end() - static_cast<std::ptrdiff_t>(settings_.fftSize), input.end());
    constexpr float inverseSqrtTwo = 0.7071067811865475F;
    while (nodes.size() < leafCount_) {
      std::vector<std::vector<float>> next;
      next.reserve(nodes.size() * 2);
      for (const auto& node : nodes) {
        std::vector<float> low(node.size() / 2);
        std::vector<float> high(node.size() / 2);
        for (std::size_t index = 0; index < low.size(); ++index) {
          const float first = node[2 * index];
          const float second = node[2 * index + 1];
          low[index] = (first + second) * inverseSqrtTwo;
          high[index] = (first - second) * inverseSqrtTwo;
        }
        next.push_back(std::move(low));
        next.push_back(std::move(high));
      }
      nodes = std::move(next);
    }

    output.transform = TransformKind::HaarWaveletPacket;
    output.centerFrequencies = centers_;
    output.bandwidths = bandwidths_;
    output.timeOffsetsSeconds.clear();
    output.magnitudes.resize(leafCount_);
    for (std::size_t band = 0; band < leafCount_; ++band) {
      const std::size_t grayOrderedNode = band ^ (band >> 1U);
      const auto& coefficients = nodes[grayOrderedNode];
      double energy = 0.0;
      for (const float coefficient : coefficients) {
        energy += static_cast<double>(coefficient) * coefficient;
      }
      output.magnitudes[band] =
          static_cast<float>(std::sqrt(energy / static_cast<double>(coefficients.size())));
    }
  }

 private:
  AnalysisSettings settings_;
  std::size_t leafCount_ = 1;
  std::vector<float> centers_;
  std::vector<float> bandwidths_;
};

class SmoothedPseudoWignerVilleTransform final : public IFrameTransform {
 public:
  explicit SmoothedPseudoWignerVilleTransform(const AnalysisSettings& settings)
      : settings_(settings) {
    const double width = (settings_.maxFrequency - settings_.minFrequency) /
                         static_cast<double>(settings_.bandCount);
    for (std::size_t band = 0; band < settings_.bandCount; ++band) {
      centers_.push_back(
          static_cast<float>(settings_.minFrequency + (static_cast<double>(band) + 0.5) * width));
      bandwidths_.push_back(static_cast<float>(width));
    }
  }

  [[nodiscard]] std::size_t requiredSamples() const noexcept override { return settings_.fftSize; }

  void analyze(std::span<const float> input, SpectralFrame& output) override {
    if (input.size() < settings_.fftSize) {
      throw std::invalid_argument("SPWVD input is shorter than FFT size");
    }
    const auto frame = input.last(settings_.fftSize);
    const std::size_t center = settings_.fftSize / 2;
    constexpr std::size_t timeHalfWidth = 2;
    const std::size_t lagMaximum = std::min(settings_.fftSize / 4, center - timeHalfWidth - 1);
    std::vector<double> autocorrelation(lagMaximum + 1, 0.0);
    for (std::size_t lag = 0; lag <= lagMaximum; ++lag) {
      double sum = 0.0;
      for (int timeOffset = -static_cast<int>(timeHalfWidth);
           timeOffset <= static_cast<int>(timeHalfWidth); ++timeOffset) {
        const std::size_t time =
            static_cast<std::size_t>(static_cast<std::ptrdiff_t>(center) + timeOffset);
        sum += static_cast<double>(frame[time + lag]) * frame[time - lag];
      }
      const double timeAverage = sum / static_cast<double>(2 * timeHalfWidth + 1);
      const double lagWindow =
          0.5 + 0.5 * std::cos(std::numbers::pi_v<double> * static_cast<double>(lag) /
                               static_cast<double>(lagMaximum + 1));
      autocorrelation[lag] = timeAverage * lagWindow;
    }

    output.transform = TransformKind::SmoothedPseudoWignerVille;
    output.centerFrequencies = centers_;
    output.bandwidths = bandwidths_;
    output.timeOffsetsSeconds.clear();
    output.magnitudes.resize(centers_.size());
    const double normalization = static_cast<double>(2 * lagMaximum + 1);
    for (std::size_t band = 0; band < centers_.size(); ++band) {
      double value = autocorrelation[0];
      for (std::size_t lag = 1; lag <= lagMaximum; ++lag) {
        const double phase = 4.0 * std::numbers::pi_v<double> * centers_[band] *
                             static_cast<double>(lag) / settings_.sampleRate;
        value += 2.0 * autocorrelation[lag] * std::cos(phase);
      }
      output.magnitudes[band] = static_cast<float>(std::sqrt(std::max(value / normalization, 0.0)));
    }
  }

 private:
  AnalysisSettings settings_;
  std::vector<float> centers_;
  std::vector<float> bandwidths_;
};

}  // namespace

std::unique_ptr<IFrameTransform> makeTransform(const AnalysisSettings& settings) {
  settings.validate();
  switch (settings.transform) {
    case TransformKind::Stft:
      return std::make_unique<StftTransform>(settings);
    case TransformKind::LogFilterbank:
    case TransformKind::Mel:
    case TransformKind::Bark:
    case TransformKind::Erb:
      return std::make_unique<FilterbankTransform>(settings);
    case TransformKind::Cqt:
    case TransformKind::Vqt:
      return std::make_unique<ConstantQTransform>(settings);
    case TransformKind::MorletCwt:
      return std::make_unique<AnalyticWaveletBankTransform>(settings, AnalyticBankKind::Morlet,
                                                            false);
    case TransformKind::MorseCwt:
      return std::make_unique<AnalyticWaveletBankTransform>(settings, AnalyticBankKind::Morse,
                                                            false);
    case TransformKind::Stockwell:
      return std::make_unique<AnalyticWaveletBankTransform>(settings, AnalyticBankKind::Stockwell,
                                                            false);
    case TransformKind::ReassignedStft:
      return std::make_unique<ReassignedStftTransform>(settings);
    case TransformKind::SynchrosqueezedCwt:
      return std::make_unique<AnalyticWaveletBankTransform>(settings, AnalyticBankKind::Morlet,
                                                            true);
    case TransformKind::Chroma:
      return std::make_unique<ChromaTransform>(settings);
    case TransformKind::HaarWaveletPacket:
      return std::make_unique<HaarWaveletPacketTransform>(settings);
    case TransformKind::SmoothedPseudoWignerVille:
      return std::make_unique<SmoothedPseudoWignerVilleTransform>(settings);
  }
  throw std::invalid_argument("unsupported transform kind");
}

}  // namespace sinesoda::dsp
