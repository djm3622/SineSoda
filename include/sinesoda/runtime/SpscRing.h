#pragma once

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace sinesoda::runtime {

// A bounded single-producer/single-consumer queue. The storage is allocated at
// construction; push and pop are wait-free and allocation-free.
template <typename Value>
class SpscRing {
 public:
  explicit SpscRing(std::size_t capacity) : storage_(capacity), mask_(capacity - 1) {
    if (capacity < 2 || !std::has_single_bit(capacity)) {
      throw std::invalid_argument("SPSC capacity must be a power of two >= 2");
    }
  }

  [[nodiscard]] bool tryPush(const Value& value) noexcept(
      std::is_nothrow_copy_assignable_v<Value>) {
    const std::size_t write = writePosition_.load(std::memory_order_relaxed);
    const std::size_t read = readPosition_.load(std::memory_order_acquire);
    if (write - read == storage_.size()) {
      return false;
    }
    storage_[write & mask_] = value;
    writePosition_.store(write + 1, std::memory_order_release);
    return true;
  }

  [[nodiscard]] bool tryPush(Value&& value) noexcept(std::is_nothrow_move_assignable_v<Value>) {
    const std::size_t write = writePosition_.load(std::memory_order_relaxed);
    const std::size_t read = readPosition_.load(std::memory_order_acquire);
    if (write - read == storage_.size()) {
      return false;
    }
    storage_[write & mask_] = std::move(value);
    writePosition_.store(write + 1, std::memory_order_release);
    return true;
  }

  [[nodiscard]] bool tryPop(Value& value) noexcept(std::is_nothrow_copy_assignable_v<Value>) {
    const std::size_t read = readPosition_.load(std::memory_order_relaxed);
    const std::size_t write = writePosition_.load(std::memory_order_acquire);
    if (read == write) {
      return false;
    }
    value = storage_[read & mask_];
    readPosition_.store(read + 1, std::memory_order_release);
    return true;
  }

  [[nodiscard]] std::size_t capacity() const noexcept { return storage_.size(); }

  [[nodiscard]] std::size_t size() const noexcept {
    const std::size_t write = writePosition_.load(std::memory_order_acquire);
    const std::size_t read = readPosition_.load(std::memory_order_acquire);
    return write - read;
  }

  // Consumer-side fast-forward used to bound latency after an overload. This
  // must only be called by the same consumer that calls tryPop().
  std::size_t discard(std::size_t count) noexcept {
    const std::size_t read = readPosition_.load(std::memory_order_relaxed);
    const std::size_t write = writePosition_.load(std::memory_order_acquire);
    const std::size_t discarded = std::min(count, write - read);
    readPosition_.store(read + discarded, std::memory_order_release);
    return discarded;
  }

 private:
  std::vector<Value> storage_;
  std::size_t mask_;
  alignas(64) std::atomic<std::size_t> writePosition_{0};
  alignas(64) std::atomic<std::size_t> readPosition_{0};
};

}  // namespace sinesoda::runtime
