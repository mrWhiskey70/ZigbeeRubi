#pragma once
#include "cJSON.h"
#include <cstdint>
#include <initializer_list>
#include <string_view>
namespace scenario {
cJSON *parse_json(std::string_view) noexcept;
bool json_keys(const cJSON *, std::initializer_list<const char *>) noexcept;
bool json_integer(const cJSON *, int64_t, int64_t, int64_t &) noexcept;
} // namespace scenario
