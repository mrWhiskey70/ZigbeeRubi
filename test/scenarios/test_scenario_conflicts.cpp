#include "scenario_engine.hpp"
#include "test_support.hpp"
int main() {
  DeviceModel m;
  fixtures(m);
  ScenarioEngine e(m);
  ClockState c;
  auto a = rule(2);
  a.actions[0].channel.channel_id = 2;
  CHECK(e.upsert(a));
  a = rule(1);
  CHECK(e.upsert(a));
  e.on_event({did(2), EventKind::OccupancyDetected, true, 0, 1}, c);
  auto b = e.tick(c);
  CHECK(b.size == 2 && b.actions[0].rule_id == 1 && b.actions[1].rule_id == 2);
  CHECK(e.tick(c).size == 0);
  e.reset_pending();
  // Commands to one channel are serialized even after logical cancellation.
  a = rule(2);
  CHECK(e.upsert(a));
  e.on_event({did(2), EventKind::OccupancyDetected, true, 0, 2}, c);
  b = e.tick(c);
  CHECK(b.size == 1 && b.actions[0].rule_id == 1);
  auto old = b.actions[0].operation_id;
  e.remove(1);
  CHECK(e.tick(c).size == 0);
  e.on_command_result(old, CommandStatus::Confirmed, 0);
  b = e.tick(c);
  CHECK(b.size == 1 && b.actions[0].rule_id == 2);
  e.reset_pending();
  e.remove(2);
  // Manual channel2 cancellation leaves channels1/3 in the same sequence.
  a = rule();
  a.action_count = 3;
  for (int i = 0; i < 3; ++i)
    a.actions[i] = {ActionKind::SetChannelPower,
                    {did(3), static_cast<uint8_t>(i + 1)},
                    true,
                    0};
  CHECK(e.upsert(a));
  e.on_event({did(2), EventKind::OccupancyDetected, true, 0, 3}, c);
  e.manual_override({did(3), 2});
  b = e.tick(c);
  CHECK(b.size == 1 && b.actions[0].channel.channel_id == 1);
  e.on_command_result(b.actions[0].operation_id, CommandStatus::Confirmed, 0);
  b = e.tick(c);
  CHECK(b.size == 1 && b.actions[0].channel.channel_id == 3);
  e.reset_pending();
  for (uint32_t id = 1; id <= 24; ++id)
    CHECK(e.upsert(rule(id)));
  CHECK(!e.upsert(rule(25)));
  e.reset_pending();
  // Atomic reservation: eight 8-action runs fit; next rule is fully rejected.
  for (uint32_t id = 1; id <= 24; ++id) {
    a = rule(id);
    a.action_count = 8;
    for (auto &act : a.actions) {
      act.kind = ActionKind::Delay;
      act.delay_ms = 10000;
    }
    CHECK(e.upsert(a));
  }
  e.on_event({did(2), EventKind::OccupancyDetected, true, 0, 4}, c);
  CHECK(e.pending() == 64);
  c.monotonic_ms = 100000;
  CHECK(e.tick(c).size == 0);
  CHECK(e.pending() == 0);
  e.reset_pending();
  for (uint32_t id = 2; id <= 24; ++id)
    e.remove(id);
  a = rule(1);
  CHECK(e.upsert(a));
  e.on_event({did(2), EventKind::OccupancyDetected, true, c.monotonic_ms, 5},
             c);
  e.disable(1);
  CHECK(e.tick(c).size == 0);
  a.enabled = true;
  CHECK(e.upsert(a));
  e.on_event({did(2), EventKind::OccupancyDetected, true, c.monotonic_ms, 6},
             c);
  CHECK(e.upsert(a));
  CHECK(e.tick(c).size == 0);
}
