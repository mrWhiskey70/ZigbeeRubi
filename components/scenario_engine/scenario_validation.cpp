// SPDX-License-Identifier: AGPL-3.0-only
#include "scenario_validation.hpp"
#include <cstring>
namespace scenario {
bool valid_utf8(const char *s, size_t n) noexcept {
  for (size_t i = 0; i < n;) {
    uint8_t b = static_cast<uint8_t>(s[i++]);
    if (b == 0)
      return false;
    if (b < 0x80)
      continue;
    unsigned more = 0;
    uint32_t cp = 0, min = 0;
    if (b >= 0xc2 && b <= 0xdf) {
      more = 1;
      cp = b & 31;
      min = 0x80;
    } else if (b >= 0xe0 && b <= 0xef) {
      more = 2;
      cp = b & 15;
      min = 0x800;
    } else if (b >= 0xf0 && b <= 0xf4) {
      more = 3;
      cp = b & 7;
      min = 0x10000;
    } else
      return false;
    if (i + more > n)
      return false;
    while (more--) {
      b = static_cast<uint8_t>(s[i++]);
      if ((b & 0xc0) != 0x80)
        return false;
      cp = (cp << 6) | (b & 63);
    }
    if (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
      return false;
  }
  return true;
}
namespace {
bool walk(const Scenario &r, uint8_t i, unsigned depth,
          std::array<bool, MaxNodes> &seen) {
  if (i >= r.node_count || depth > 4 || seen[i])
    return false;
  seen[i] = true;
  const auto &n = r.nodes[i];
  if (n.kind == NodeKind::All || n.kind == NodeKind::Any) {
    if (!n.child_count || n.child_count > MaxNodes)
      return false;
    for (size_t j = 0; j < n.child_count; ++j)
      if (!walk(r, n.children[j], depth + 1, seen))
        return false;
    return true;
  }
  if (n.child_count)
    return false;
  if (n.kind == NodeKind::TimeWindow)
    return n.start_minute < 1440 && n.end_minute < 1440;
  if (n.kind != NodeKind::State || !n.device_id.valid() ||
      static_cast<size_t>(n.capability) >= CapabilityCount || !n.expected.known)
    return false;
  const auto type = (n.capability == Capability::Battery ||
                     n.capability == Capability::Temperature)
                        ? ScalarType::Integer
                        : ScalarType::Boolean;
  if (n.expected.value < -9007199254740991LL ||
      n.expected.value > 9007199254740991LL)
    return false;
  if (n.expected.type != type ||
      static_cast<unsigned>(n.op) > static_cast<unsigned>(Compare::Ge))
    return false;
  if (type == ScalarType::Boolean &&
      (n.expected.value < 0 || n.expected.value > 1 ||
       (n.op != Compare::Eq && n.op != Compare::Ne)))
    return false;
  return true;
}
} // namespace
ValidationResult validate_shape(const Scenario &r) noexcept {
  const auto *end =
      static_cast<const char *>(std::memchr(r.name.data(), 0, r.name.size()));
  if (!r.id || !end || end == r.name.data() ||
      !valid_utf8(r.name.data(), end - r.name.data()) || !r.trigger_count ||
      r.trigger_count > MaxTriggers || r.node_count > MaxNodes ||
      !r.action_count || r.action_count > MaxActions)
    return {ErrorCode::Invalid};
  for (size_t i = 0; i < r.trigger_count; ++i)
    if (!r.triggers[i].device_id.valid() ||
        static_cast<unsigned>(r.triggers[i].kind) > 3)
      return {ErrorCode::Invalid};
  std::array<bool, MaxNodes> seen{};
  if (r.node_count) {
    if (!walk(r, 0, 1, seen))
      return {ErrorCode::Invalid};
    for (size_t i = 0; i < r.node_count; ++i)
      if (!seen[i])
        return {ErrorCode::Invalid};
  }
  for (size_t i = 0; i < r.action_count; ++i) {
    const auto &a = r.actions[i];
    if (a.kind == ActionKind::Delay) {
      if (a.delay_ms > 86400000)
        return {ErrorCode::Invalid};
    } else if (a.kind != ActionKind::SetChannelPower ||
               !a.channel.device_id.valid() || !a.channel.channel_id)
      return {ErrorCode::Invalid};
  }
  return {};
}
ValidationResult validate_scenario(const Scenario &r,
                                   const DeviceModel &m) noexcept {
  auto shape = validate_shape(r);
  if (!shape)
    return shape;
  DeviceSnapshot d;
  for (size_t i = 0; i < r.trigger_count; ++i) {
    const auto &t = r.triggers[i];
    if (!m.snapshot(t.device_id, d))
      return {ErrorCode::MissingDevice};
    Capability c = static_cast<unsigned>(t.kind) < 2 ? Capability::Contact
                                                     : Capability::Occupancy;
    if (!(d.capabilities & capability_bit(c)))
      return {ErrorCode::Unsupported};
  }
  for (size_t i = 0; i < r.node_count; ++i) {
    const auto &n = r.nodes[i];
    if (n.kind != NodeKind::State)
      continue;
    if (!m.snapshot(n.device_id, d))
      return {ErrorCode::MissingDevice};
    if (!(d.capabilities & capability_bit(n.capability)))
      return {ErrorCode::Unsupported};
  }
  for (size_t i = 0; i < r.action_count; ++i) {
    const auto &a = r.actions[i];
    if (a.kind == ActionKind::Delay)
      continue;
    if (!m.snapshot(a.channel.device_id, d))
      return {ErrorCode::MissingDevice};
    bool found = false;
    for (size_t j = 0; j < d.channel_count; ++j)
      if (d.channels[j].id == a.channel.channel_id)
        found = true;
    if (!found)
      return {ErrorCode::MissingChannel};
  }
  return {};
}
} // namespace scenario
