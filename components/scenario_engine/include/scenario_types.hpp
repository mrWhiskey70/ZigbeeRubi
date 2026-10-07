// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "device_id.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace scenario {
using DeviceId = core::DeviceId;
using ScenarioId = uint32_t;
using ChannelId = uint8_t;
using OperationId = uint32_t;
constexpr size_t MaxDevices = 16, MaxChannels = 4, MaxRules = 24,
                 MaxTriggers = 8, MaxNodes = 16, MaxActions = 8,
                 MaxPending = 64;
enum class ErrorCode {
  Ok,
  Invalid,
  Capacity,
  MissingDevice,
  MissingChannel,
  Unsupported,
  TypeMismatch,
  Unavailable,
  Storage,
  Malformed,
  TooLarge
};
struct ValidationResult {
  ErrorCode code = ErrorCode::Ok;
  explicit operator bool() const noexcept { return code == ErrorCode::Ok; }
};
enum class Truth { False, Unknown, True };
enum class ScalarType { Boolean, Integer, Enum };
struct Scalar {
  bool known = false;
  ScalarType type = ScalarType::Boolean;
  int64_t value = 0;
  static Scalar boolean(bool v) {
    return {true, ScalarType::Boolean, v ? 1 : 0};
  }
  static Scalar integer(int64_t v) { return {true, ScalarType::Integer, v}; }
};
enum class Capability : uint8_t {
  Contact,
  Occupancy,
  Battery,
  Temperature,
  Count
};
constexpr uint32_t capability_bit(Capability c) {
  return 1U << static_cast<uint8_t>(c);
}
constexpr size_t CapabilityCount = static_cast<size_t>(Capability::Count);
struct ChannelKey {
  DeviceId device_id{};
  ChannelId channel_id = 0;
  friend bool operator==(const ChannelKey &a, const ChannelKey &b) {
    return a.device_id == b.device_id && a.channel_id == b.channel_id;
  }
};
enum class RouteKind { Unsupported, StandardOnOff, TuyaDp };
struct ChannelSnapshot {
  ChannelId id = 0;
  Scalar power{};
  RouteKind route = RouteKind::Unsupported;
  uint8_t endpoint = 0, dp = 0;
};
struct DeviceSnapshot {
  DeviceId id{};
  std::array<char, 97> name{};
  bool available = false;
  uint32_t capabilities = 0;
  std::array<Scalar, CapabilityCount> values{};
  std::array<ChannelSnapshot, MaxChannels> channels{};
  uint8_t channel_count = 0;
  uint16_t short_addr = 0xffff;
};
struct DeviceReport {
  DeviceId id{};
  Capability capability = Capability::Contact;
  Scalar value{};
  ChannelId channel_id = 0;
  bool mapping_verified = true;
};
enum class EventKind {
  ContactOpened,
  ContactClosed,
  OccupancyDetected,
  OccupancyCleared
};
struct DeviceEvent {
  DeviceId id{};
  EventKind kind{};
  bool value = false;
  uint64_t monotonic_ms = 0, sequence = 0;
};
struct EventBatch {
  std::array<DeviceEvent, 16> events{};
  size_t size = 0;
};
struct ClockState {
  uint64_t monotonic_ms = 0;
  bool wall_known = false;
  int64_t utc_seconds = 0;
  int offset_minutes = 420;
};
enum class NodeKind { All, Any, State, TimeWindow };
enum class Compare { Eq, Ne, Lt, Le, Gt, Ge };
struct ConditionNode {
  NodeKind kind = NodeKind::State;
  std::array<uint8_t, MaxNodes> children{};
  uint8_t child_count = 0;
  DeviceId device_id{};
  Capability capability = Capability::Contact;
  Compare op = Compare::Eq;
  Scalar expected = Scalar::boolean(true);
  uint16_t start_minute = 0, end_minute = 0;
};
struct Trigger {
  DeviceId device_id{};
  EventKind kind = EventKind::ContactOpened;
};
enum class ActionKind { SetChannelPower, Delay };
struct Action {
  ActionKind kind = ActionKind::SetChannelPower;
  ChannelKey channel{};
  bool on = false;
  uint32_t delay_ms = 0;
};
struct Scenario {
  ScenarioId id = 0;
  std::array<char, 97> name{};
  bool enabled = true;
  std::array<Trigger, MaxTriggers> triggers{};
  uint8_t trigger_count = 0;
  std::array<ConditionNode, MaxNodes> nodes{};
  uint8_t node_count = 0;
  std::array<Action, MaxActions> actions{};
  uint8_t action_count = 0;
};
struct ScenarioSet {
  std::array<Scenario, MaxRules> rules{};
  size_t size = 0;
};
enum class CommandStatus { Queued, Pending, Confirmed, Failed, Timeout };
struct CommandAction {
  ScenarioId rule_id = 0;
  uint64_t generation = 0;
  ChannelKey channel{};
  bool on = false;
  uint64_t due_ms = 0;
  OperationId operation_id = 0;
};
struct ActionBatch {
  std::array<CommandAction, MaxPending> actions{};
  size_t size = 0;
};
} // namespace scenario
