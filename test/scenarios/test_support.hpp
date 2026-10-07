#pragma once
#include "device_model.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);             \
      std::exit(1);                                                            \
    }                                                                          \
  } while (false)
using namespace scenario;
inline DeviceId did(uint8_t n) {
  std::array<uint8_t, 8> b{};
  b[7] = n;
  return DeviceId(b);
}
inline DeviceSnapshot sensor(uint8_t n, Capability c) {
  DeviceSnapshot d;
  d.id = did(n);
  d.available = true;
  d.capabilities = capability_bit(c);
  return d;
}
inline DeviceSnapshot relay() {
  DeviceSnapshot d;
  d.id = did(3);
  d.available = true;
  d.channel_count = 3;
  for (uint8_t i = 0; i < 3; ++i) {
    d.channels[i].id = i + 1;
    d.channels[i].route = RouteKind::StandardOnOff;
    d.channels[i].endpoint = i + 1;
  }
  return d;
}
inline void fixtures(DeviceModel &m) {
  CHECK(m.add(sensor(1, Capability::Contact)));
  CHECK(m.add(sensor(2, Capability::Occupancy)));
  CHECK(m.add(relay()));
}
inline Scenario rule(uint32_t id = 1) {
  Scenario r;
  r.id = id;
  std::strcpy(r.name.data(), "Light");
  r.trigger_count = 1;
  r.triggers[0] = {did(2), EventKind::OccupancyDetected};
  r.action_count = 1;
  r.actions[0] = {ActionKind::SetChannelPower, {did(3), 1}, true, 0};
  return r;
}
inline void report(DeviceModel &m, DeviceId id, Capability c, bool v,
                   uint64_t now, EventBatch &e) {
  CHECK(m.apply_report({id, c, Scalar::boolean(v), 0, true}, now, e));
}
