#pragma once

#include <complex>
#include <cstddef>
#include <span>
#include <vector>

namespace sinesoda::dsp {

class Fft {
 public:
  explicit Fft(std::size_t size);

  [[nodiscard]] std::size_t size() const noexcept { return size_; }

  void forward(std::span<const float> input, std::span<std::complex<float>> output) const;
  void transform(std::span<std::complex<float>> values, bool inverse) const;

 private:
  std::size_t size_;
  std::vector<std::size_t> bitReversal_;
  std::vector<std::complex<float>> twiddles_;
};

[[nodiscard]] bool isPowerOfTwo(std::size_t value) noexcept;
[[nodiscard]] std::size_t nextPowerOfTwo(std::size_t value);

}  // namespace sinesoda::dsp
