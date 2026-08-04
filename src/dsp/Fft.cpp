#include "sinesoda/dsp/Fft.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace sinesoda::dsp {

bool isPowerOfTwo(std::size_t value) noexcept { return value != 0 && (value & (value - 1)) == 0; }

std::size_t nextPowerOfTwo(std::size_t value) {
  if (value <= 1) {
    return 1;
  }
  --value;
  for (std::size_t shift = 1; shift < sizeof(std::size_t) * 8; shift <<= 1U) {
    value |= value >> shift;
  }
  if (value == static_cast<std::size_t>(-1)) {
    throw std::overflow_error("nextPowerOfTwo overflow");
  }
  return value + 1;
}

Fft::Fft(std::size_t size) : size_(size), bitReversal_(size), twiddles_(size / 2) {
  if (!isPowerOfTwo(size_) || size_ < 2) {
    throw std::invalid_argument("FFT size must be a power of two of at least two");
  }

  std::size_t bits = 0;
  for (std::size_t value = size_; value > 1; value >>= 1U) {
    ++bits;
  }
  for (std::size_t index = 0; index < size_; ++index) {
    std::size_t value = index;
    std::size_t reversed = 0;
    for (std::size_t bit = 0; bit < bits; ++bit) {
      reversed = (reversed << 1U) | (value & 1U);
      value >>= 1U;
    }
    bitReversal_[index] = reversed;
  }

  constexpr double twoPi = 2.0 * std::numbers::pi_v<double>;
  for (std::size_t index = 0; index < twiddles_.size(); ++index) {
    const double phase = -twoPi * static_cast<double>(index) / static_cast<double>(size_);
    twiddles_[index] = std::complex<float>(static_cast<float>(std::cos(phase)),
                                           static_cast<float>(std::sin(phase)));
  }
}

void Fft::forward(std::span<const float> input, std::span<std::complex<float>> output) const {
  if (input.size() != size_ || output.size() != size_) {
    throw std::invalid_argument("FFT input and output must match the configured size");
  }
  for (std::size_t index = 0; index < size_; ++index) {
    output[index] = std::complex<float>(input[index], 0.0F);
  }
  transform(output, false);
}

void Fft::transform(std::span<std::complex<float>> values, bool inverse) const {
  if (values.size() != size_) {
    throw std::invalid_argument("FFT buffer must match the configured size");
  }

  for (std::size_t index = 0; index < size_; ++index) {
    const std::size_t reversed = bitReversal_[index];
    if (index < reversed) {
      std::swap(values[index], values[reversed]);
    }
  }

  for (std::size_t length = 2; length <= size_; length <<= 1U) {
    const std::size_t half = length / 2;
    const std::size_t stride = size_ / length;
    for (std::size_t offset = 0; offset < size_; offset += length) {
      for (std::size_t index = 0; index < half; ++index) {
        const auto twiddle =
            inverse ? std::conj(twiddles_[index * stride]) : twiddles_[index * stride];
        const auto even = values[offset + index];
        const auto odd = values[offset + index + half] * twiddle;
        values[offset + index] = even + odd;
        values[offset + index + half] = even - odd;
      }
    }
  }

  if (inverse) {
    const float normalization = 1.0F / static_cast<float>(size_);
    for (auto& value : values) {
      value *= normalization;
    }
  }
}

}  // namespace sinesoda::dsp
