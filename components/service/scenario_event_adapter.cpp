#include "scenario_event_adapter.hpp"
#include <cstring>
namespace service {
void forward_scenario_event(scenario::HubRuntime &hub,
                            const core::CoreEvent &e) noexcept {
  if (e.scenario_forwarded)
    return;
  if (!e.device_id.valid())
    return;
  if (e.type == core::CoreEventType::kDeviceLeft ||
      e.type == core::CoreEventType::kDeviceStale) {
    hub.unavailable(e.device_id);
    return;
  }
  scenario::DeviceSnapshot d;
  bool existed = hub.device(e.device_id, d);
  if (!existed) {
    d.id = e.device_id;
    char id[17]{};
    d.id.format(id, 16);
    std::strcpy(d.name.data(), id);
  }
  bool changed = !existed || d.short_addr != e.device_short_addr;
  d.short_addr = e.device_short_addr;
  if (e.type == core::CoreEventType::kDeviceJoined) {
    d.available = true;
    changed = true;
  }
  scenario::DeviceReport report;
  report.id = d.id;
  bool has_report = false;
  if (e.type == core::CoreEventType::kAttributeReported &&
      e.verified_standard_report && e.cluster_id == 6 && e.attribute_id == 0 &&
      e.endpoint > 0 && e.endpoint <= 240) {
    scenario::ChannelSnapshot *ch = nullptr;
    for (size_t i = 0; i < d.channel_count; ++i)
      if (d.channels[i].endpoint == e.endpoint &&
          d.channels[i].route == scenario::RouteKind::StandardOnOff)
        ch = &d.channels[i];
    if (!ch && d.channel_count < scenario::MaxChannels) {
      ch = &d.channels[d.channel_count++];
      ch->id = d.channel_count;
      ch->endpoint = e.endpoint;
      ch->route = scenario::RouteKind::StandardOnOff;
      changed = true;
    }
    if (ch) {
      report.channel_id = ch->id;
      report.value = scenario::Scalar::boolean(e.value_bool);
      has_report = true;
    }
  } else if (e.type == core::CoreEventType::kAttributeReported &&
             e.verified_standard_report && e.cluster_id == 0x0406 &&
             e.attribute_id == 0) {
    report.capability = scenario::Capability::Occupancy;
    report.value = scenario::Scalar::boolean((e.value_u32 & 1) != 0);
    has_report = true;
    if (!(d.capabilities & scenario::capability_bit(report.capability))) {
      d.capabilities |= scenario::capability_bit(report.capability);
      changed = true;
    }
  } else if (e.type == core::CoreEventType::kDeviceTelemetryUpdated &&
             e.telemetry_valid && e.verified_standard_report) {
    using T = core::CoreTelemetryKind;
    using C = scenario::Capability;
    switch (e.telemetry_kind) {
    case T::kOccupancy:
      report.capability = C::Occupancy;
      report.value = scenario::Scalar::boolean(e.telemetry_i32 != 0);
      has_report = true;
      break;
    case T::kContactIasZoneStatus:
      report.capability = C::Contact;
      report.value = scenario::Scalar::boolean((e.telemetry_i32 & 1) != 0);
      has_report = true;
      break;
    case T::kBatteryPercent:
      report.capability = C::Battery;
      report.value = scenario::Scalar::integer(e.telemetry_i32);
      has_report = true;
      break;
    case T::kTemperatureCentiC:
      report.capability = C::Temperature;
      report.value = scenario::Scalar::integer(e.telemetry_i32);
      has_report = true;
      break;
    default:
      break;
    }
    if (has_report &&
        !(d.capabilities & scenario::capability_bit(report.capability))) {
      d.capabilities |= scenario::capability_bit(report.capability);
      changed = true;
    }
  }
  if (changed && !hub.configure_device(d))
    return;
  if (has_report)
    (void)hub.on_report(report);
}
} // namespace service
