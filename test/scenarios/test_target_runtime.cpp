#include "hub_runtime.hpp"
#include "scenario_event_adapter.hpp"
#include "test_support.hpp"
#include <map>
#include <string>
using namespace scenario;
struct Memory : StoreBackend {
  std::map<unsigned, std::vector<uint8_t>> s, p;
  std::string metadata;
  bool read_slot(unsigned i, std::vector<uint8_t> &o) noexcept override {
    if (!s.count(i))
      return false;
    o = s[i];
    return true;
  }
  bool write_slot(unsigned i, std::string_view b) noexcept override {
    s[i] = {b.begin(), b.end()};
    return true;
  }
  bool read_active_generation(unsigned i,
                              std::vector<uint8_t> &o) noexcept override {
    if (!p.count(i))
      return false;
    o = p[i];
    return true;
  }
  bool write_active_generation(unsigned i,
                               std::string_view b) noexcept override {
    p[i] = {b.begin(), b.end()};
    return true;
  }
  bool read_metadata(std::string &o) noexcept override {
    if (metadata.empty())
      return false;
    o = metadata;
    return true;
  }
  bool write_metadata(std::string_view b) noexcept override {
    metadata = b;
    return true;
  }
};
struct Radio : RuntimeAdapter {
  std::vector<CommandAction> sent;
  bool send(const CommandAction &a, const DeviceSnapshot &,
            const ChannelSnapshot &) noexcept override {
    sent.push_back(a);
    return true;
  }
  bool join(uint16_t) noexcept override { return true; }
};
std::string request(HubRuntime &r, const char *m, const char *p,
                    const char *b = "") {
  auto *j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "method", m);
  cJSON_AddStringToObject(j, "path", p);
  cJSON_AddStringToObject(j, "body", b);
  char *t = cJSON_PrintUnformatted(j);
  std::vector<char> out(65536);
  size_t n = 0;
  CHECK(r.handle_request(t, out.data(), out.size(), n));
  cJSON_free(t);
  cJSON_Delete(j);
  return {out.data(), n};
}
int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  if (argc > 1) {
    Memory storage;
    Radio radio;
    HubRuntime hub(storage, &radio);
    auto d = relay();
    d.short_addr = 0x1234;
    CHECK(hub.configure_device(d));
    auto contact = sensor(1, Capability::Contact);
    contact.short_addr = 0x11;
    CHECK(hub.configure_device(contact));
    auto motion = sensor(2, Capability::Occupancy);
    motion.short_addr = 0x22;
    CHECK(hub.configure_device(motion));
    CHECK(hub.on_report({did(1), Capability::Contact, Scalar::boolean(false)}));
    CHECK(
        hub.on_report({did(2), Capability::Occupancy, Scalar::boolean(false)}));
    auto r = rule();
    r.action_count = 3;
    r.actions[1] = {ActionKind::Delay, {}, false, 60000};
    r.actions[2] = {ActionKind::SetChannelPower, {did(3), 1}, false, 0};
    if (std::string(argv[1]) == "stale" || std::string(argv[1]) == "order") {
      r.node_count = 1;
      r.nodes[0].kind = NodeKind::State;
      r.nodes[0].device_id = did(1);
      r.nodes[0].capability = Capability::Contact;
      r.nodes[0].op = Compare::Eq;
      r.nodes[0].expected = Scalar::boolean(false);
    }
    std::vector<char> json(8193);
    size_t n = 0;
    CHECK(encode_scenario(r, json.data(), json.size(), n));
    CHECK(request(hub, "POST", "/api/v1/scenarios", json.data()).find("201") !=
          std::string::npos);
    core::CoreEvent event;
    event.device_id = did(1);
    event.device_short_addr = 0x11;
    event.type = core::CoreEventType::kDeviceStale;
    if (std::string(argv[1]) == "stale") {
      service::forward_scenario_event(hub, event);
      DeviceSnapshot snap;
      CHECK(hub.device(did(1), snap));
      CHECK(!snap.available);
      CHECK(hub.on_report(
          {did(2), Capability::Occupancy, Scalar::boolean(true)}));
      CHECK(radio.sent.empty());
      return 0;
    }
    if (std::string(argv[1]) == "order") {
      event.type = core::CoreEventType::kAttributeReported;
      event.device_id = did(2);
      event.device_short_addr = 0x22;
      event.cluster_id = 0x0406;
      event.value_u32 = 1;
      event.verified_standard_report = true;
      service::forward_scenario_event(hub, event);
      event.type = core::CoreEventType::kDeviceTelemetryUpdated;
      event.device_id = did(1);
      event.device_short_addr = 0x11;
      event.telemetry_kind = core::CoreTelemetryKind::kContactIasZoneStatus;
      event.telemetry_valid = true;
      event.telemetry_i32 = 1;
      service::forward_scenario_event(hub, event);
      CHECK(radio.sent.size() == 1);
      return 0;
    }
    CHECK(
        hub.on_report({did(2), Capability::Occupancy, Scalar::boolean(true)}));
    CHECK(radio.sent.size() == 1);
    auto response = request(
        hub, "POST", "/api/v1/channels/power",
        "{\"device_id\":\"0000000000000003\",\"channel_id\":1,\"on\":false}");
    CHECK(response.find("202") != std::string::npos);
    CHECK(radio.sent.size() == 1);
    CHECK(hub.on_report(
        {did(3), Capability::Contact, Scalar::boolean(true), 1, true}));
    CHECK(radio.sent.size() == 2 && !radio.sent.back().on);
    CHECK(hub.on_report(
        {did(3), Capability::Contact, Scalar::boolean(false), 1, true}));
    ClockState clock;
    clock.monotonic_ms = 60000;
    hub.set_clock(clock);
    hub.tick();
    CHECK(radio.sent.size() == 2);
    return 0;
  }
  Memory s;
  Radio radio;
  HubRuntime r(s, &radio);
  CHECK(r.ready());
  std::string direct;
  CHECK(r.handle_request_to_string(
      "{\"method\":\"GET\",\"path\":\"/api/v1/system\",\"body\":\"\"}",
      direct));
  CHECK(direct.find("target") != std::string::npos);
  CHECK(request(r, "GET", "/api/v1/system").find("target") !=
        std::string::npos);
  CHECK(request(r, "POST", "/api/v1/sim/advance", "{\"advance_ms\":1}")
            .find("404") != std::string::npos);
  auto d = relay();
  d.short_addr = 0x1234;
  CHECK(r.configure_device(d));
  CHECK(request(
            r, "POST", "/api/v1/channels/power",
            "{\"device_id\":\"0000000000000003\",\"channel_id\":2,\"on\":true}")
            .find("202") != std::string::npos);
  CHECK(radio.sent.size() == 1 && radio.sent[0].channel.channel_id == 2);
  auto op = radio.sent[0].operation_id;
  CHECK(request(r, "GET", ("/api/v1/operations/" + std::to_string(op)).c_str())
            .find("pending") != std::string::npos);
  r.command_ack(op, true);
  CHECK(request(r, "GET", ("/api/v1/operations/" + std::to_string(op)).c_str())
            .find("pending") != std::string::npos);
  ClockState c;
  c.monotonic_ms = 1;
  r.set_clock(c);
  CHECK(r.on_report(
      {did(3), Capability::Contact, Scalar::boolean(true), 2, true}));
  CHECK(request(r, "GET", ("/api/v1/operations/" + std::to_string(op)).c_str())
            .find("confirmed") != std::string::npos);
  d.short_addr = 0x4567;
  CHECK(r.configure_device(d));
  CHECK(request(
            r, "POST", "/api/v1/channels/power",
            "{\"device_id\":\"0000000000000003\",\"channel_id\":3,\"on\":true}")
            .find("202") != std::string::npos);
  c.monotonic_ms = 6001;
  r.set_clock(c);
  r.tick();
  CHECK(radio.sent.size() == 2);
  CHECK(request(r, "GET",
                ("/api/v1/operations/" +
                 std::to_string(radio.sent.back().operation_id))
                    .c_str())
            .find("timeout") != std::string::npos);
  d.channels[0].route = RouteKind::Unsupported;
  CHECK(r.configure_device(d));
  CHECK(request(
            r, "POST", "/api/v1/channels/power",
            "{\"device_id\":\"0000000000000003\",\"channel_id\":1,\"on\":true}")
            .find("422") != std::string::npos);
  CHECK(radio.sent.size() == 2);
  HubRuntime restarted(s, &radio);
  CHECK(restarted.ready());
  CHECK(request(restarted, "GET", "/api/v1/devices")
            .find("\"available\":false") != std::string::npos);
}
