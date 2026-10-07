#include "scenario_manager.hpp"
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#include "hal_zigbee.h"
#include "service_runtime.hpp"
#include <cstring>
#include <ctime>
#include <new>
namespace service {
ScenarioManager::ScenarioManager(ServiceRuntime &r)
    : runtime_(r), hub_(backend_, this),
      requests_(xQueueCreate(8, sizeof(Request *))) {}
void ScenarioManager::release(Request *r) {
  if (r->refs.fetch_sub(1) == 1) {
    vSemaphoreDelete(r->done);
    delete r;
  }
}
bool ScenarioManager::request(std::string_view input,
                              std::string &output) noexcept {
  auto *r = new (std::nothrow) Request;
  if (!r)
    return false;
  if (!r->done) {
    delete r;
    return false;
  }
  r->input.assign(input);
  if (xQueueSend(requests_, &r, 0) != pdTRUE) {
    r->refs = 1;
    release(r);
    return false;
  }
  bool success = xSemaphoreTake(r->done, pdMS_TO_TICKS(10000)) == pdTRUE;
  if (success) {
    success = r->success;
    output = std::move(r->output);
  }
  release(r);
  return success;
}
void ScenarioManager::tick() noexcept {
  scenario::ClockState clock;
  clock.monotonic_ms = esp_timer_get_time() / 1000;
  clock.utc_seconds = std::time(nullptr);
  clock.wall_known = clock.utc_seconds > 1700000000;
  // HubRuntime preserves its configured timezone when updating target clocks.
  for (auto &link : command_links_)
    if (link.radio_id && clock.monotonic_ms >= link.deadline)
      link = {};
  hub_.set_clock(clock);
  hub_.tick();
  Request *r = nullptr;
  if (xQueueReceive(requests_, &r, 0) == pdTRUE) {
    r->success = bool(hub_.handle_request_to_string(r->input, r->output));
    xSemaphoreGive(r->done);
    release(r);
  }
}
void ScenarioManager::identify(core::CoreEvent &e) noexcept {
  std::array<uint8_t, 8> b{};
  if (hal_zigbee_get_device_eui64(e.device_short_addr, b.data())) {
    e.device_id = core::DeviceId(b);
    return;
  }
  for (const auto &i : identities_)
    if (i.address == e.device_short_addr && i.id.valid()) {
      e.device_id = i.id;
      return;
    }
}
bool ScenarioManager::send(const scenario::CommandAction &a,
                           const scenario::DeviceSnapshot &d,
                           const scenario::ChannelSnapshot &ch) noexcept {
  // Only standard OnOff routes observed from real reports are currently
  // enabled.
  if (ch.route != scenario::RouteKind::StandardOnOff || !ch.endpoint ||
      ch.endpoint > 240)
    return false;
  CommandLink *link = nullptr;
  for (auto &entry : command_links_)
    if (!entry.radio_id) {
      link = &entry;
      break;
    }
  if (!link)
    return false;
  core::CoreCommand cmd;
  cmd.type = core::CoreCommandType::kSetDevicePower;
  cmd.correlation_id = runtime_.next_operation_request_id();
  *link = {cmd.correlation_id, a.operation_id,
           uint64_t(esp_timer_get_time() / 1000) + 5000};
  cmd.device_id = d.id;
  cmd.device_short_addr = d.short_addr;
  cmd.endpoint = ch.endpoint;
  cmd.native_channel = true;
  cmd.desired_power_on = a.on;
  cmd.issued_at_ms = esp_timer_get_time() / 1000;
  if (runtime_.post_command(cmd) == core::CoreError::kOk)
    return true;
  *link = {};
  return false;
}
bool ScenarioManager::join(uint16_t seconds) noexcept {
  return runtime_.post_open_join_window(runtime_.next_operation_request_id(),
                                        seconds);
}
bool ScenarioManager::remove(core::DeviceId id) noexcept {
  scenario::DeviceSnapshot d;
  if (!hub_.device(id, d) || !d.available)
    return false;
  return runtime_.post_remove_device(runtime_.next_operation_request_id(),
                                     d.short_addr, false, 5000);
}
void ScenarioManager::on_event(const core::CoreEvent &e) noexcept {
  if (e.type == core::CoreEventType::kCommandResultSuccess ||
      e.type == core::CoreEventType::kCommandResultFailed ||
      e.type == core::CoreEventType::kCommandResultTimeout) {
    for (auto &link : command_links_)
      if (link.radio_id && link.radio_id == e.correlation_id) {
        hub_.command_ack(link.operation_id,
                         e.type == core::CoreEventType::kCommandResultSuccess);
        link = {};
        break;
      }
    return;
  }
  if (!e.device_id.valid())
    return;
  if (e.type == core::CoreEventType::kDeviceLeft) {
    hub_.unavailable(e.device_id);
    return;
  }
  for (auto &old : identities_)
    if (old.address == e.device_short_addr && old.id.valid() &&
        old.id != e.device_id) {
      hub_.unavailable(old.id);
      old.address = 0xffff;
    }
  for (auto &i : identities_)
    if (i.id == e.device_id || !i.id.valid()) {
      i = {e.device_id, e.device_short_addr};
      break;
    }
  scenario::DeviceSnapshot d;
  bool existed = hub_.device(e.device_id, d);
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
  if (changed && !hub_.configure_device(d))
    return;
  if (has_report)
    (void)hub_.on_report(report);
}
} // namespace service
#endif
