#include "scenario_store.hpp"
#include "test_support.hpp"
#include <string>
struct Memory : StoreBackend {
  std::array<std::vector<uint8_t>, 2> slots{}, markers{};
  int writes = 0, fail = -1;
  bool read_slot(unsigned i, std::vector<uint8_t> &v) noexcept override {
    v = slots[i];
    return !v.empty();
  }
  bool read_active_generation(unsigned i,
                              std::vector<uint8_t> &v) noexcept override {
    v = markers[i];
    return !v.empty();
  }
  bool put(std::vector<uint8_t> &v, std::string_view s) {
    if (++writes == fail) {
      v.assign(s.begin(), s.begin() + s.size() / 2);
      return false;
    }
    v.assign(s.begin(), s.end());
    return true;
  }
  bool write_slot(unsigned i, std::string_view s) noexcept override {
    return put(slots[i], s);
  }
  bool write_active_generation(unsigned i,
                               std::string_view s) noexcept override {
    return put(markers[i], s);
  }
};
int main() {
  Memory m;
  ScenarioStore store(m);
  ScenarioSet a, out;
  a.size = 1;
  a.rules[0] = rule();
  CHECK(store.load(out) && out.size == 0);
  CHECK(store.save(a));
  int w = m.writes;
  CHECK(store.save(a) && m.writes == w);
  CHECK(store.load(out) && out.rules[0].id == 1);
  auto b = a;
  b.rules[0].id = 2;
  for (int step = 1; step <= 2; ++step) {
    auto copy = m;
    copy.fail = copy.writes + step;
    ScenarioStore faulty(copy);
    CHECK(!faulty.save(b));
    CHECK(faulty.load(out) && out.rules[0].id == 1);
  }
  CHECK(store.save(b));
  CHECK(store.load(out) && out.rules[0].id == 2);
  auto good = m;
  m.slots[0][10] ^= 1;
  CHECK(store.load(out) && out.rules[0].id == 1);
  m = good;
  m.markers[0][0] ^= 1;
  CHECK(store.load(out) && out.rules[0].id == 1);
  m = good;
  auto bad = b;
  bad.rules[0].trigger_count = 0;
  w = m.writes;
  CHECK(!store.save(bad) && m.writes == w);
  CHECK(store.load(out) && out.rules[0].id == 2);
  a.size = 24;
  for (int i = 0; i < 24; ++i)
    a.rules[i] = rule(i + 1);
  CHECK(store.save(a));
  CHECK(store.load(out) && out.size == 24);
  auto too_large = a;
  for (auto &r : too_large.rules) {
    r.name.fill(char(1));
    r.name[96] = 0;
    r.trigger_count = 8;
    for (auto &t : r.triggers)
      t = {did(2), EventKind::OccupancyDetected};
    r.node_count = 16;
    r.nodes[0].kind = NodeKind::All;
    r.nodes[0].child_count = 15;
    for (int j = 1; j < 16; ++j) {
      r.nodes[0].children[j - 1] = j;
      r.nodes[j].device_id = did(1);
    }
    r.action_count = 8;
    for (auto &act : r.actions)
      act = {ActionKind::SetChannelPower, {did(3), 1}, true, 0};
  }
  w = m.writes;
  CHECK(!store.save(too_large) && m.writes == w);
}
