#pragma once
#include "scenario_types.hpp"
namespace scenario {
struct LogEntry {
  uint64_t sequence = 0, monotonic_ms = 0;
  ScenarioId rule_id = 0;
  ChannelKey channel{};
  OperationId operation_id = 0;
  CommandStatus status = CommandStatus::Queued;
  std::array<char, 48> reason{};
};
class ScenarioLog {
  std::array<LogEntry, 200> rows_{};
  size_t size_ = 0, start_ = 0;
  uint64_t sequence_ = 0;

public:
  void clear() noexcept {
    size_ = 0;
    start_ = 0;
    sequence_ = 0;
  }
  void append(uint64_t, ScenarioId, ChannelKey, OperationId, CommandStatus,
              const char *) noexcept;
  size_t size() const noexcept { return size_; }
  const LogEntry &at(size_t i) const noexcept {
    return rows_[(start_ + i) % 200];
  }
};
} // namespace scenario
