// SPDX-License-Identifier: AGPL-3.0-only
#include "scenario_log.hpp"
#include <cstdio>
namespace scenario {
void ScenarioLog::append(uint64_t now, ScenarioId r, ChannelKey k,
                         OperationId o, CommandStatus s,
                         const char *reason) noexcept {
  size_t i;
  if (size_ < rows_.size()) {
    i = (start_ + size_++) % rows_.size();
  } else {
    i = start_;
    start_ = (start_ + 1) % rows_.size();
  }
  auto &e = rows_[i];
  e = {};
  e.sequence = ++sequence_;
  e.monotonic_ms = now;
  e.rule_id = r;
  e.channel = k;
  e.operation_id = o;
  e.status = s;
  std::snprintf(e.reason.data(), e.reason.size(), "%s", reason);
}
} // namespace scenario
