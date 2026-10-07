// SPDX-License-Identifier: AGPL-3.0-only
#include "monotonic_clock.hpp"
namespace scenario {
uint64_t MonotonicClock::extend(uint32_t v) noexcept {
  if (!initialized_) {
    initialized_ = true;
    extended_ = v;
  } else {
    extended_ += static_cast<uint32_t>(v - last_);
  }
  last_ = v;
  return extended_;
}
} // namespace scenario
