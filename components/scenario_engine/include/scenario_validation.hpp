#pragma once
#include "device_model.hpp"
namespace scenario {
bool valid_utf8(const char *, size_t) noexcept;
ValidationResult validate_shape(const Scenario &) noexcept;
ValidationResult validate_scenario(const Scenario &,
                                   const DeviceModel &) noexcept;
} // namespace scenario
