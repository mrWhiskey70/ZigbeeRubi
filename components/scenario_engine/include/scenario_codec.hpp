#pragma once
#include "scenario_validation.hpp"
#include <string_view>
namespace scenario {
ValidationResult decode_scenario(std::string_view, Scenario &) noexcept;
ValidationResult encode_scenario(const Scenario &, char *, size_t,
                                 size_t &) noexcept;
} // namespace scenario
