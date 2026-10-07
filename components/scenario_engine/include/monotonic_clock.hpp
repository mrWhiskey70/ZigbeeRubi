#pragma once
#include <cstdint>
namespace scenario {
class MonotonicClock {
  bool initialized_ = false;
  uint32_t last_ = 0;
  uint64_t extended_ = 0;

public:
  uint64_t extend(uint32_t) noexcept;
};
} // namespace scenario
