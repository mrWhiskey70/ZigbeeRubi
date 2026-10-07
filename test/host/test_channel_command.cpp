#include "core_commands.hpp"
#include "core_state.hpp"
#include <cassert>
int main() {
  core::CoreCommand c;
  c.type = core::CoreCommandType::kSetDevicePower;
  c.correlation_id = 42;
  c.endpoint = 13;
  c.native_channel = true;
  c.device_short_addr = 0x1234;
  c.desired_power_on = true;
  c.device_id = core::DeviceId({0, 0, 0, 0, 0, 0, 0, 3});
  auto event = core::command_to_event(c);
  assert(event.endpoint == 13 && event.native_channel);
  auto reduced = core::core_reduce({}, event);
  assert(reduced.effects.count == 1);
  assert(reduced.effects.items[0].endpoint == 13 &&
         reduced.effects.items[0].native_channel);
  core::CoreState state;
  state.device_count = 1;
  state.devices[0].device_id = c.device_id;
  state.devices[0].short_addr = c.device_short_addr;
  state.devices[0].online = true;
  event.type = core::CoreEventType::kAttributeReported;
  event.cluster_id = 6;
  event.attribute_id = 0;
  event.value_bool = true;
  reduced = core::core_reduce(state, event);
  assert(!reduced.next.devices[0].power_on);
  event.endpoint = 1;
  reduced = core::core_reduce(state, event);
  assert(reduced.next.devices[0].power_on);
}
