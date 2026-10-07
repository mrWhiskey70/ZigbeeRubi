#pragma once
#include "file_store.hpp"
#include "json_tools.hpp"
#include "scenario_engine.hpp"
namespace scenario {
class SimulatorRuntime {
  DeviceModel model_;
  ScenarioEngine engine_;
  FileStore backend_;
  ScenarioStore store_;
  ClockState clock_{};
  uint64_t join_until_ = 0;
  struct Operation {
    OperationId id = 0;
    CommandStatus status = CommandStatus::Confirmed;
  };
  std::array<Operation, 200> ops_{};
  size_t op_cursor_ = 0;
  OperationId manual_id_ = 0x80000000U;
  void pump();
  void fixtures(bool);
  bool load_names();
  bool save_names();
  void operation(OperationId, CommandStatus);
  cJSON *dispatch(const char *, const char *, std::string_view, int &);

public:
  explicit SimulatorRuntime(const std::filesystem::path &);
  ValidationResult handle_request(std::string_view, char *, size_t,
                                  size_t &) noexcept;
  void advance_real(uint64_t) noexcept;
};
} // namespace scenario
