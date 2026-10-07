#include "scenario_engine.hpp"
#include "test_support.hpp"
Scenario delayed() {
  auto r = rule();
  r.action_count = 3;
  r.actions[1] = {ActionKind::Delay, {}, false, 60000};
  r.actions[2] = {ActionKind::SetChannelPower, {did(3), 1}, false, 0};
  return r;
}
int main() {
  DeviceModel m;
  fixtures(m);
  ScenarioEngine eng(m);
  auto r = delayed();
  CHECK(eng.upsert(r));
  ClockState c;
  eng.on_event({did(2), EventKind::OccupancyDetected, true, 0, 1}, c);
  auto b = eng.tick(c);
  CHECK(b.size == 1 && b.actions[0].on);
  eng.on_command_result(b.actions[0].operation_id, CommandStatus::Confirmed, 0);
  c.monotonic_ms = 59000;
  CHECK(eng.tick(c).size == 0);
  c.monotonic_ms = 60000;
  b = eng.tick(c);
  CHECK(b.size == 1 && !b.actions[0].on);
  eng.on_command_result(b.actions[0].operation_id, CommandStatus::Confirmed,
                        60000);
  CHECK(eng.tick(c).size == 0);
  // Retrigger replaces delayed OFF; duplicate sequence cannot retrigger.
  c.monotonic_ms = 0;
  eng.on_event({did(2), EventKind::OccupancyDetected, true, 0, 2}, c);
  b = eng.tick(c);
  eng.on_command_result(b.actions[0].operation_id, CommandStatus::Confirmed, 0);
  c.monotonic_ms = 30000;
  eng.on_event({did(2), EventKind::OccupancyDetected, true, 30000, 3}, c);
  b = eng.tick(c);
  CHECK(b.size == 1);
  eng.on_command_result(b.actions[0].operation_id, CommandStatus::Confirmed,
                        30000);
  c.monotonic_ms = 60000;
  CHECK(eng.tick(c).size == 0);
  eng.on_event({did(2), EventKind::OccupancyDetected, true, 60000, 3}, c);
  c.monotonic_ms = 90000;
  b = eng.tick(c);
  CHECK(b.size == 1 && !b.actions[0].on);
  eng.on_command_result(b.actions[0].operation_id, CommandStatus::Confirmed,
                        90000);
  c.monotonic_ms = 100000;
  eng.on_event({did(2), EventKind::OccupancyDetected, true, 100000, 4}, c);
  b = eng.tick(c);
  eng.manual_override({did(3), 1});
  eng.on_command_result(b.actions[0].operation_id, CommandStatus::Confirmed,
                        100000);
  c.monotonic_ms = 200000;
  CHECK(eng.tick(c).size == 0);
  eng.on_event({did(2), EventKind::OccupancyDetected, true, 200000, 5}, c);
  b = eng.tick(c);
  CHECK(b.size == 1);
  eng.on_command_result(b.actions[0].operation_id, CommandStatus::Failed,
                        200000);
  c.monotonic_ms = 300000;
  CHECK(eng.tick(c).size == 0);
  eng.on_event({did(2), EventKind::OccupancyDetected, true, 300000, 6}, c);
  b = eng.tick(c);
  CHECK(b.size == 1);
  c.monotonic_ms += 5000;
  CHECK(eng.tick(c).size == 0);
  CHECK(eng.pending() == 0);
  // Unavailable output fails before any physical command.
  m.mark_unavailable(did(3));
  eng.on_event({did(2), EventKind::OccupancyDetected, true, c.monotonic_ms, 7},
               c);
  CHECK(eng.tick(c).size == 0);
  ScenarioLog log;
  for (int i = 0; i < 201; ++i)
    log.append(i, 1, {}, 0, CommandStatus::Failed, "test");
  CHECK(log.size() == 200 && log.at(0).sequence == 2 &&
        log.at(199).sequence == 201);
}
