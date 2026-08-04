#include <complex>
#include <vector>

#include "TestSupport.h"
#include "sinesoda/dsp/Fft.h"

using sinesoda::dsp::Fft;

SINESODA_TEST(FftImpulseHasFlatSpectrum) {
  constexpr std::size_t size = 32;
  Fft fft(size);
  std::vector<float> input(size, 0.0F);
  std::vector<std::complex<float>> output(size);
  input[0] = 1.0F;
  fft.forward(input, output);
  for (const auto value : output) {
    SINESODA_EXPECT_NEAR(value.real(), 1.0, 1.0e-6);
    SINESODA_EXPECT_NEAR(value.imag(), 0.0, 1.0e-6);
  }
}

SINESODA_TEST(FftRoundTripRecoversComplexInput) {
  constexpr std::size_t size = 64;
  Fft fft(size);
  std::vector<std::complex<float>> values(size);
  for (std::size_t index = 0; index < size; ++index) {
    values[index] = {static_cast<float>(index) / 64.0F, static_cast<float>(size - index) / 128.0F};
  }
  const auto original = values;
  fft.transform(values, false);
  fft.transform(values, true);
  for (std::size_t index = 0; index < size; ++index) {
    SINESODA_EXPECT_NEAR(values[index].real(), original[index].real(), 2.0e-5);
    SINESODA_EXPECT_NEAR(values[index].imag(), original[index].imag(), 2.0e-5);
  }
}

SINESODA_TEST(PowerOfTwoUtilitiesAreBounded) {
  SINESODA_EXPECT_TRUE(sinesoda::dsp::isPowerOfTwo(4096));
  SINESODA_EXPECT_TRUE(!sinesoda::dsp::isPowerOfTwo(4095));
  SINESODA_EXPECT_EQ(sinesoda::dsp::nextPowerOfTwo(4095), 4096U);
  SINESODA_EXPECT_EQ(sinesoda::dsp::nextPowerOfTwo(4096), 4096U);
  SINESODA_EXPECT_THROW(Fft(48), std::invalid_argument);
}
