#pragma once
#include "device_model.hpp"
namespace scenario {
Truth evaluate_condition(const Scenario &, uint8_t, const DeviceModel &,
                         const ClockState &) noexcept;
bool matches_trigger(const Scenario &, const DeviceEvent &) noexcept;
} // namespace scenario
