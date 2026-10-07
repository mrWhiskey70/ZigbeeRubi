#include "test_support.hpp"
int main() {
  DeviceModel m;
  fixtures(m);
  EventBatch e;
  report(m, did(1), Capability::Contact, true, 0, e);
  CHECK(e.size == 0);
  report(m, did(1), Capability::Contact, false, 1, e);
  CHECK(e.size == 1);
  CHECK(e.events[0].kind == EventKind::ContactClosed);
  report(m, did(1), Capability::Contact, false, 2, e);
  CHECK(e.size == 0);
  CHECK(m.apply_report(
      {did(3), Capability::Contact, Scalar::boolean(true), 2, true}, 3, e));
  DeviceSnapshot d;
  CHECK(m.snapshot(did(3), d));
  CHECK(d.channels[1].power.known && d.channels[1].power.value == 1);
  CHECK(!d.channels[0].power.known && !d.channels[2].power.known);
  CHECK(!m.apply_report(
      {did(3), Capability::Contact, Scalar::boolean(false), 2, false}, 4, e));
  CHECK(m.snapshot(did(3), d) && d.channels[1].power.value == 1);
  CHECK(!m.apply_report(
      {did(1), Capability::Contact, Scalar::integer(1), 0, true}, 4, e));
  for (uint8_t i = 4; i <= 16; ++i)
    CHECK(m.add(sensor(i, Capability::Contact)));
  CHECK(!m.add(sensor(17, Capability::Contact)));
  CHECK(m.size() == 16);
  m.mark_unavailable(did(1));
  CHECK(m.snapshot(did(1), d) && !d.available);
  m.reset_runtime_state();
  CHECK(m.snapshot(did(1), d) && !d.values[0].known);
  report(m, did(1), Capability::Contact, true, 5, e);
  CHECK(e.size == 0);
  CHECK(!m.apply_report(
      {did(3), Capability::Contact, Scalar::boolean(true), 4, true}, 0, e));
}
