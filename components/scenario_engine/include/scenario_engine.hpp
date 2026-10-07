#pragma once
#include "condition_evaluator.hpp"
#include "scenario_log.hpp"
#include "scenario_validation.hpp"
namespace scenario {
class ScenarioEngine {
  struct Run {
    bool active = false;
    uint64_t generation = 0, order = 0, due = 0;
    uint8_t cursor = 0;
    OperationId waiting = 0;
    std::array<bool, MaxActions> skip{};
  };
  struct Inflight {
    bool used = false;
    CommandAction action{};
    uint64_t sent = 0;
  };
  const DeviceModel &model_;
  ScenarioSet set_{};
  std::array<Run, MaxRules> runs_{};
  std::array<Inflight, MaxPending> inflight_{};
  uint64_t order_ = 0, generation_ = 0, last_event_ = 0;
  OperationId next_operation_ = 1;
  ScenarioLog log_{};
  size_t index(ScenarioId) const noexcept;
  size_t reserved() const noexcept;
  bool busy(ChannelKey) const noexcept;
  void cancel(size_t) noexcept;

public:
  explicit ScenarioEngine(const DeviceModel &m) : model_(m) {}
  ValidationResult upsert(const Scenario &) noexcept;
  ValidationResult restore(const ScenarioSet &) noexcept;
  void on_event(const DeviceEvent &, const ClockState &) noexcept;
  ActionBatch tick(const ClockState &) noexcept;
  void on_command_result(OperationId, CommandStatus, uint64_t) noexcept;
  void manual_override(ChannelKey) noexcept;
  void remove(ScenarioId) noexcept;
  void disable(ScenarioId) noexcept;
  void reset_pending() noexcept;
  const ScenarioSet &scenarios() const noexcept { return set_; }
  void clear_log() noexcept { log_.clear(); }
  const ScenarioLog &log() const noexcept { return log_; }
  size_t pending() const noexcept { return reserved(); }
};
} // namespace scenario
