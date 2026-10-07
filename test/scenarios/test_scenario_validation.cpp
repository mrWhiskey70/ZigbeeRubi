#include "scenario_validation.hpp"
#include "test_support.hpp"
int main() {
  DeviceModel m;
  fixtures(m);
  Scenario r = rule();
  CHECK(validate_scenario(r, m));
  r.trigger_count = 0;
  CHECK(!validate_scenario(r, m));
  r = rule();
  r.actions[0].channel.channel_id = 4;
  CHECK(!validate_scenario(r, m));
  r = rule();
  r.node_count = 1;
  r.nodes[0].kind = NodeKind::All;
  CHECK(!validate_scenario(r, m));
  r.nodes[0].child_count = 1;
  r.nodes[0].children[0] = 0;
  CHECK(!validate_scenario(r, m));
  r.nodes[0] = {};
  r.nodes[0].device_id = did(1);
  r.nodes[0].expected = Scalar::integer(1);
  CHECK(!validate_scenario(r, m));
  r = rule();
  r.node_count = 16;
  r.nodes[0].kind = NodeKind::All;
  r.nodes[0].child_count = 15;
  for (int i = 1; i < 16; ++i) {
    r.nodes[0].children[i - 1] = i;
    r.nodes[i].device_id = did(1);
  }
  CHECK(validate_scenario(r, m));
  r.node_count = 17;
  CHECK(!validate_scenario(r, m));
  r = rule();
  r.node_count = 4;
  for (int i = 0; i < 3; ++i) {
    r.nodes[i].kind = NodeKind::All;
    r.nodes[i].child_count = 1;
    r.nodes[i].children[0] = i + 1;
  }
  r.nodes[3].device_id = did(1);
  CHECK(validate_scenario(r, m));
  r.node_count = 5;
  r.nodes[3].kind = NodeKind::All;
  r.nodes[3].child_count = 1;
  r.nodes[3].children[0] = 4;
  r.nodes[4].device_id = did(1);
  CHECK(!validate_scenario(r, m));
  r = rule();
  r.action_count = 1;
  r.actions[0].kind = ActionKind::Delay;
  r.actions[0].delay_ms = 86400001;
  CHECK(!validate_scenario(r, m));
  r.actions[0].delay_ms = 0;
  CHECK(validate_scenario(r, m));
  CHECK(valid_utf8("Дверь", 10));
  const char bad[] = {char(0xc0), char(0xaf)};
  CHECK(!valid_utf8(bad, 2));
}
