#include "scenario_manager.hpp"
#include "scenario_event_adapter.hpp"
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
  forward_scenario_event(hub_, e);
}
} // namespace service
#endif
