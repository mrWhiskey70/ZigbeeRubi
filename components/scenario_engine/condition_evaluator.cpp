// SPDX-License-Identifier: AGPL-3.0-only
#include "condition_evaluator.hpp"
namespace scenario {
namespace {
Truth eval(const Scenario &r, uint8_t i, const DeviceModel &m,
           const ClockState &c, unsigned depth) {
  if (i >= r.node_count || r.node_count > MaxNodes || depth > 4)
    return Truth::Unknown;
  const auto &n = r.nodes[i];
  if (n.kind == NodeKind::All || n.kind == NodeKind::Any) {
    if (!n.child_count || n.child_count > MaxNodes)
      return Truth::Unknown;
    bool unknown = false;
    for (size_t j = 0; j < n.child_count; ++j) {
      auto t = eval(r, n.children[j], m, c, depth + 1);
      if (n.kind == NodeKind::All && t == Truth::False)
        return Truth::False;
      if (n.kind == NodeKind::Any && t == Truth::True)
        return Truth::True;
      unknown |= t == Truth::Unknown;
    }
    return unknown ? Truth::Unknown
                   : (n.kind == NodeKind::All ? Truth::True : Truth::False);
  }
  if (n.kind == NodeKind::TimeWindow) {
    if (!c.wall_known || c.offset_minutes < -720 || c.offset_minutes > 840 ||
        n.start_minute >= 1440 || n.end_minute >= 1440)
      return Truth::Unknown;
    int64_t seconds =
        (c.utc_seconds % 86400 + c.offset_minutes * 60 + 86400) % 86400;
    unsigned minute = seconds / 60;
    bool yes = n.start_minute == n.end_minute ||
               (n.start_minute < n.end_minute
                    ? (minute >= n.start_minute && minute < n.end_minute)
                    : (minute >= n.start_minute || minute < n.end_minute));
    return yes ? Truth::True : Truth::False;
  }
  if (n.kind != NodeKind::State ||
      static_cast<size_t>(n.capability) >= CapabilityCount)
    return Truth::Unknown;
  DeviceSnapshot d;
  if (!m.snapshot(n.device_id, d) || !d.available ||
      !(d.capabilities & capability_bit(n.capability)))
    return Truth::Unknown;
  const auto &v = d.values[static_cast<size_t>(n.capability)];
  if (!v.known || !n.expected.known || v.type != n.expected.type)
    return Truth::Unknown;
  bool yes = false;
  switch (n.op) {
  case Compare::Eq:
    yes = v.value == n.expected.value;
    break;
  case Compare::Ne:
    yes = v.value != n.expected.value;
    break;
  case Compare::Lt:
    yes = v.value < n.expected.value;
    break;
  case Compare::Le:
    yes = v.value <= n.expected.value;
    break;
  case Compare::Gt:
    yes = v.value > n.expected.value;
    break;
  case Compare::Ge:
    yes = v.value >= n.expected.value;
    break;
  default:
    return Truth::Unknown;
  }
  return yes ? Truth::True : Truth::False;
}
} // namespace
Truth evaluate_condition(const Scenario &r, uint8_t i, const DeviceModel &m,
                         const ClockState &c) noexcept {
  return r.node_count ? eval(r, i, m, c, 1) : Truth::True;
}
bool matches_trigger(const Scenario &r, const DeviceEvent &e) noexcept {
  for (size_t i = 0; i < r.trigger_count && i < MaxTriggers; ++i)
    if (r.triggers[i].device_id == e.id && r.triggers[i].kind == e.kind)
      return true;
  return false;
}
} // namespace scenario
