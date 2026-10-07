#include "scenario_codec.hpp"
#include "test_support.hpp"
#include <string>
const char *json =
    R"({"id":1,"name":"<img onerror=alert(1)>","enabled":true,"triggers":[{"device_id":"0000000000000002","kind":"occupancy.detected"}],"conditions":[{"kind":"all","children":[1,2]},{"kind":"state","device_id":"0000000000000001","capability":"contact","op":"eq","value":true},{"kind":"time_window","start":1380,"end":120}],"actions":[{"kind":"set_channel_power","device_id":"0000000000000003","channel_id":1,"on":true},{"kind":"delay","delay_ms":60000}]})";
int main() {
  Scenario r;
  CHECK(decode_scenario(json, r));
  CHECK(r.node_count == 3 && r.actions[1].delay_ms == 60000);
  CHECK(std::string(r.name.data()).find("<img") == 0);
  char buf[8300];
  size_t n = 0;
  CHECK(encode_scenario(r, buf, sizeof(buf), n));
  Scenario round;
  CHECK(decode_scenario({buf, n}, round));
  CHECK(round.node_count == 3 &&
        round.triggers[0].kind == EventKind::OccupancyDetected);
  std::string s = json;
  auto id = s.find("\"id\":1");
  for (const char *v : {"true", "\"1\"", "-1", "1.5", "NaN", "4294967296"}) {
    auto t = s;
    t.replace(id + 5, 1, v);
    CHECK(!decode_scenario(t, round));
  }
  auto t = s;
  t.insert(1, "\"id\":2,");
  CHECK(!decode_scenario(t, round));
  CHECK(!decode_scenario(s + " garbage", round));
  t = s;
  t.insert(1, "\"unknown\":1,");
  CHECK(!decode_scenario(t, round));
  t = s;
  auto kind = t.find("occupancy.detected");
  t.replace(kind, 18, "bogus");
  CHECK(!decode_scenario(t, round));
  t = s;
  t.insert(t.find("<img"), std::string("\xc0\xaf", 2));
  CHECK(!decode_scenario(t, round));
  CHECK(decode_scenario(s + std::string(8192 - s.size(), ' '), round));
  CHECK(!decode_scenario(s + std::string(8193 - s.size(), ' '), round));
  auto original = round;
  CHECK(!decode_scenario("{}", round));
  CHECK(round.id == original.id);
  DeviceModel m;
  fixtures(m);
  r = rule();
  r.node_count = 5;
  for (int i = 0; i < 4; ++i) {
    r.nodes[i].kind = NodeKind::All;
    r.nodes[i].child_count = 1;
    r.nodes[i].children[0] = i + 1;
  }
  r.nodes[4].device_id = did(1);
  CHECK(!encode_scenario(r, buf, sizeof(buf), n));
}
