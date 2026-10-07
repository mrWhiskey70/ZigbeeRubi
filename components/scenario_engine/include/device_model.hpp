// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "scenario_types.hpp"
#include <string_view>
namespace scenario {
class DeviceModel {
  std::array<DeviceSnapshot, MaxDevices> devices_{};
  size_t size_ = 0;
  uint64_t sequence_ = 0;

public:
  ValidationResult configure(const DeviceSnapshot &) noexcept;
  ValidationResult add(const DeviceSnapshot &) noexcept;
  ValidationResult apply_report(const DeviceReport &, uint64_t,
                                EventBatch &) noexcept;
  bool snapshot(DeviceId, DeviceSnapshot &) const noexcept;
  void mark_unavailable(DeviceId) noexcept;
  void reset_runtime_state() noexcept;
  ValidationResult rename(DeviceId, std::string_view) noexcept;
  void remove(DeviceId) noexcept;
  size_t size() const noexcept { return size_; }
  const DeviceSnapshot &at(size_t i) const noexcept { return devices_[i]; }
};
} // namespace scenario
