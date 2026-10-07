#pragma once
#include "scenario_codec.hpp"
#include <string_view>
#include <vector>
namespace scenario {
class StoreBackend {
public:
  virtual ~StoreBackend() = default;
  virtual bool read_slot(unsigned, std::vector<uint8_t> &) noexcept = 0;
  virtual bool write_slot(unsigned, std::string_view) noexcept = 0;
  virtual bool read_active_generation(unsigned,
                                      std::vector<uint8_t> &) noexcept = 0;
  virtual bool write_active_generation(unsigned, std::string_view) noexcept = 0;
};
class ScenarioStore {
  StoreBackend &backend_;

public:
  explicit ScenarioStore(StoreBackend &b) : backend_(b) {}
  ValidationResult save(const ScenarioSet &) noexcept;
  ValidationResult load(ScenarioSet &) noexcept;
};
} // namespace scenario
