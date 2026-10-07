#include "hub_runtime.hpp"
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
namespace scenario {
namespace {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
cJSON *field(const cJSON *j, const char *k) {
  return cJSON_GetObjectItemCaseSensitive(j, k);
}
std::string textid(DeviceId id) {
  char b[17]{};
  id.format(b, 16);
  return b;
}
bool deviceid(const cJSON *j, DeviceId &d) {
  return cJSON_IsString(j) && std::strlen(j->valuestring) == 16 &&
         DeviceId::parse(j->valuestring, 16, &d);
}
int error_status(ErrorCode c) {
  switch (c) {
  case ErrorCode::Capacity:
  case ErrorCode::TooLarge:
    return 409;
  case ErrorCode::MissingDevice:
  case ErrorCode::MissingChannel:
  case ErrorCode::Unsupported:
  case ErrorCode::Unavailable:
    return 422;
  case ErrorCode::Storage:
    return 503;
  default:
    return 400;
  }
}
const char *error_name(ErrorCode c) {
  switch (c) {
  case ErrorCode::Capacity:
    return "capacity";
  case ErrorCode::TooLarge:
    return "too_large";
  case ErrorCode::MissingDevice:
    return "missing_device";
  case ErrorCode::MissingChannel:
    return "missing_channel";
  case ErrorCode::Unsupported:
    return "unsupported";
  case ErrorCode::Unavailable:
    return "unavailable";
  case ErrorCode::Storage:
    return "storage_error";
  case ErrorCode::TypeMismatch:
    return "type_mismatch";
  default:
    return "invalid_request";
  }
}
cJSON *failure(int &status, ErrorCode c) {
  status = error_status(c);
  auto *j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "error", error_name(c));
  return j;
}
cJSON *ok() {
  auto *j = cJSON_CreateObject();
  cJSON_AddBoolToObject(j, "ok", true);
  return j;
}
const char *caps[] = {"contact", "occupancy", "battery", "temperature"};
const char *statuses[] = {"queued", "pending", "confirmed", "failed",
                          "timeout"};
} // namespace
HubRuntime::HubRuntime(StoreBackend &b, RuntimeAdapter *adapter)
    : engine_(model_), backend_(b), adapter_(adapter), store_(backend_) {
  if (!adapter_) {
    clock_.wall_known = true;
    clock_.utc_seconds = std::time(nullptr);
    fixtures(true);
  }
  auto set_ptr = std::make_unique<ScenarioSet>();
  auto &set = *set_ptr;
  ready_ = bool(store_.load(set)) && bool(engine_.restore(set)) && load_names();
  if (!adapter_ && set.size)
    model_.reset_runtime_state();
}
void HubRuntime::fixtures(bool initialize) {
  model_ = DeviceModel{};
  DeviceSnapshot d;
  d.id = DeviceId({0, 0, 0, 0, 0, 0, 0, 1});
  d.available = initialize;
  d.capabilities = capability_bit(Capability::Contact);
  std::strcpy(d.name.data(), "Дверь");
  model_.add(d);
  d = {};
  d.id = DeviceId({0, 0, 0, 0, 0, 0, 0, 2});
  d.available = initialize;
  d.capabilities = capability_bit(Capability::Occupancy);
  std::strcpy(d.name.data(), "Движение");
  model_.add(d);
  d = {};
  d.id = DeviceId({0, 0, 0, 0, 0, 0, 0, 3});
  d.available = initialize;
  std::strcpy(d.name.data(), "Выключатель · 3 канала");
  d.channel_count = 3;
  for (uint8_t i = 0; i < 3; ++i) {
    d.channels[i].id = i + 1;
    d.channels[i].route = RouteKind::StandardOnOff;
    d.channels[i].endpoint = i + 1;
  }
  model_.add(d);
  if (initialize) {
    EventBatch events;
    for (size_t i = 0; i < 2; ++i) {
      const auto &s = model_.at(i);
      model_.apply_report({s.id,
                           i == 0 ? Capability::Contact : Capability::Occupancy,
                           Scalar::boolean(false), 0, true},
                          0, events);
    }
    for (uint8_t i = 1; i <= 3; ++i)
      model_.apply_report(
          {d.id, Capability::Contact, Scalar::boolean(false), i, true}, 0,
          events);
  }
}
void HubRuntime::operation(OperationId id, CommandStatus status) {
  for (auto &o : ops_)
    if (o.id == id) {
      o.status = status;
      return;
    }
  ops_[op_cursor_++ % ops_.size()] = {id, status};
}
bool HubRuntime::send(const CommandAction &a) {
  DeviceSnapshot d;
  if (!model_.snapshot(a.channel.device_id, d) || !d.available)
    return false;
  const ChannelSnapshot *channel = nullptr;
  for (size_t i = 0; i < d.channel_count; ++i)
    if (d.channels[i].id == a.channel.channel_id)
      channel = &d.channels[i];
  if (!channel || channel->route == RouteKind::Unsupported ||
      !channel->endpoint || d.short_addr == 0xffff)
    return false;
  for (auto &p : pending_)
    if (p.used && p.sent && p.action.channel == a.channel)
      return false;
  Pending *slot = nullptr;
  for (auto &p : pending_)
    if (!p.used) {
      slot = &p;
      break;
    }
  if (!slot)
    return false;
  *slot = {true, a, clock_.monotonic_ms + 5000, true};
  operation(a.operation_id, CommandStatus::Pending);
  if (!adapter_->send(a, d, *channel)) {
    slot->used = false;
    return false;
  }
  return true;
}
void HubRuntime::finish(OperationId id, CommandStatus status) {
  for (auto &p : pending_)
    if (p.used && p.action.operation_id == id)
      p.used = false;
  operation(id, status);
  engine_.on_command_result(id, status, clock_.monotonic_ms);
}
void HubRuntime::command_ack(OperationId id, bool success) noexcept {
  // APS/ZCL acceptance is not a power-state confirmation.
  if (!success)
    finish(id, CommandStatus::Failed);
}
bool HubRuntime::submit_manual(const CommandAction &a) {
  bool busy = false;
  for (const auto &p : pending_)
    if (p.used && p.sent && p.action.channel == a.channel)
      busy = true;
  if (!busy)
    return send(a);
  for (auto &p : pending_)
    if (!p.used) {
      p = {true, a, 0, false};
      operation(a.operation_id, CommandStatus::Queued);
      return true;
    }
  return false;
}
void HubRuntime::drain_manual() {
  for (auto &p : pending_)
    if (p.used && !p.sent) {
      bool busy = false;
      for (const auto &other : pending_)
        if (other.used && other.sent &&
            other.action.channel == p.action.channel)
          busy = true;
      if (busy)
        continue;
      const auto action = p.action;
      p.used = false;
      if (!send(action))
        finish(action.operation_id, CommandStatus::Failed);
    }
}
void HubRuntime::tick() noexcept {
  for (auto &p : pending_)
    if (p.used && p.sent && clock_.monotonic_ms >= p.deadline)
      finish(p.action.operation_id, CommandStatus::Timeout);
  drain_manual();
  pump();
}
ValidationResult HubRuntime::on_report(const DeviceReport &report) noexcept {
  EventBatch events;
  auto v = model_.apply_report(report, clock_.monotonic_ms, events);
  if (!v)
    return v;
  for (auto &p : pending_)
    if (p.used && p.sent && p.action.channel.device_id == report.id &&
        p.action.channel.channel_id == report.channel_id &&
        report.value.known && (report.value.value != 0) == p.action.on)
      finish(p.action.operation_id, CommandStatus::Confirmed);
  drain_manual();
  for (size_t i = 0; i < events.size; ++i)
    engine_.on_event(events.events[i], clock_);
  pump();
  return {};
}
ValidationResult
HubRuntime::configure_device(const DeviceSnapshot &d) noexcept {
  auto v = model_.configure(d);
  if (v && !save_names())
    return {ErrorCode::Storage};
  return v;
}
void HubRuntime::pump() {
  for (size_t pass = 0; pass < MaxPending; ++pass) {
    auto batch = engine_.tick(clock_);
    if (!batch.size)
      break;
    for (size_t i = 0; i < batch.size; ++i) {
      const auto &a = batch.actions[i];
      if (adapter_) {
        if (!send(a))
          finish(a.operation_id, CommandStatus::Failed);
      } else {
        EventBatch e;
        auto v = model_.apply_report({a.channel.device_id, Capability::Contact,
                                      Scalar::boolean(a.on),
                                      a.channel.channel_id, true},
                                     clock_.monotonic_ms, e);
        auto status = v ? CommandStatus::Confirmed : CommandStatus::Failed;
        engine_.on_command_result(a.operation_id, status, clock_.monotonic_ms);
        operation(a.operation_id, status);
      }
    }
  }
}
void HubRuntime::advance_real(uint64_t delta) noexcept {
  clock_.monotonic_ms += delta;
  clock_.utc_seconds = std::time(nullptr);
  pump();
}
bool HubRuntime::load_names() {
  std::string text;
  if (!backend_.read_metadata(text))
    return true;
  Json root(parse_json(text), cJSON_Delete);
  if (!root || !json_keys(root.get(), {"schema", "offset_minutes", "devices"}))
    return false;
  int64_t schema = 0, offset = 0;
  if (!json_integer(field(root.get(), "schema"), 1, 1, schema) ||
      !json_integer(field(root.get(), "offset_minutes"), -720, 840, offset))
    return false;
  auto *devices = field(root.get(), "devices");
  if (!cJSON_IsArray(devices) || cJSON_GetArraySize(devices) > 16)
    return false;
  auto next_ptr = std::make_unique<DeviceModel>();
  auto &next = *next_ptr;
  for (auto *j = devices->child; j; j = j->next) {
    DeviceSnapshot d;
    auto *name = field(j, "name");
    int64_t cap = 0;
    if (!json_keys(j, {"device_id", "name", "capabilities", "channels"}) ||
        !deviceid(field(j, "device_id"), d.id) || !cJSON_IsString(name) ||
        !valid_utf8(name->valuestring, std::strlen(name->valuestring)) ||
        std::strlen(name->valuestring) > 96 ||
        !json_integer(field(j, "capabilities"), 0, 15, cap))
      return false;
    std::strcpy(d.name.data(), name->valuestring);
    d.capabilities = cap;
    auto *channels = field(j, "channels");
    if (!cJSON_IsArray(channels) || cJSON_GetArraySize(channels) > 4)
      return false;
    for (auto *c = channels->child; c; c = c->next) {
      auto &ch = d.channels[d.channel_count++];
      int64_t id = 0, endpoint = 0, route = 0, dp = 0;
      if (!json_keys(c, {"id", "endpoint", "route", "dp"}) ||
          !json_integer(field(c, "id"), 1, 255, id) ||
          !json_integer(field(c, "endpoint"), 0, 240, endpoint) ||
          !json_integer(field(c, "route"), 0, 2, route) ||
          !json_integer(field(c, "dp"), 0, 255, dp))
        return false;
      ch.id = id;
      ch.endpoint = endpoint;
      ch.route = static_cast<RouteKind>(route);
      ch.dp = dp;
    }
    if (!next.add(d))
      return false;
  }
  model_ = next;
  clock_.offset_minutes = offset;
  return true;
}
bool HubRuntime::save_names() {
  Json root(cJSON_CreateObject(), cJSON_Delete);
  cJSON_AddNumberToObject(root.get(), "schema", 1);
  cJSON_AddNumberToObject(root.get(), "offset_minutes", clock_.offset_minutes);
  auto *a = cJSON_AddArrayToObject(root.get(), "devices");
  for (size_t i = 0; i < model_.size(); ++i) {
    const auto &d = model_.at(i);
    auto *j = cJSON_CreateObject();
    cJSON_AddItemToArray(a, j);
    cJSON_AddStringToObject(j, "device_id", textid(d.id).c_str());
    cJSON_AddStringToObject(j, "name", d.name.data());
    cJSON_AddNumberToObject(j, "capabilities", d.capabilities);
    auto *channels = cJSON_AddArrayToObject(j, "channels");
    for (size_t k = 0; k < d.channel_count; ++k) {
      const auto &ch = d.channels[k];
      auto *c = cJSON_CreateObject();
      cJSON_AddItemToArray(channels, c);
      cJSON_AddNumberToObject(c, "id", ch.id);
      cJSON_AddNumberToObject(c, "endpoint", ch.endpoint);
      cJSON_AddNumberToObject(c, "route", static_cast<unsigned>(ch.route));
      cJSON_AddNumberToObject(c, "dp", ch.dp);
    }
  }
  char *text = cJSON_PrintUnformatted(root.get());
  if (!text)
    return false;
  bool result = std::strlen(text) <= 8192 && backend_.write_metadata(text);
  cJSON_free(text);
  return result;
}
cJSON *HubRuntime::dispatch(const char *method, const char *path,
                            std::string_view body, int &status) {
  const std::string route(path);
  if (!ready_)
    return failure(status, ErrorCode::Storage);
  if (adapter_ && route.rfind("/api/v1/sim/", 0) == 0) {
    status = 404;
    auto *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "error", "not_found");
    return j;
  }
  const bool get = std::strcmp(method, "GET") == 0,
             post = std::strcmp(method, "POST") == 0,
             put = std::strcmp(method, "PUT") == 0,
             del = std::strcmp(method, "DELETE") == 0;
  Json input(nullptr, cJSON_Delete);
  if (!get) {
    input.reset(parse_json(body.empty() ? "{}" : body));
    if (!input || !cJSON_IsObject(input.get()))
      return failure(status, ErrorCode::Malformed);
  }
  auto *in = input.get();
  if (route == "/api/v1/system" && get) {
    auto *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "mode", adapter_ ? "target" : "simulator");
    cJSON_AddStringToObject(j, "version", "0.1.0-dev");
    cJSON_AddBoolToObject(j, "hardware_verified", false);
    cJSON_AddNumberToObject(j, "monotonic_ms",
                            static_cast<double>(clock_.monotonic_ms));
    cJSON_AddNumberToObject(j, "pending", engine_.pending());
    cJSON_AddNumberToObject(j, "offset_minutes", clock_.offset_minutes);
    cJSON_AddBoolToObject(j, "time_known", clock_.wall_known);
    cJSON_AddBoolToObject(j, "join_open", clock_.monotonic_ms < join_until_);
    return j;
  }
  if (route == "/api/v1/devices" && get) {
    auto *j = cJSON_CreateObject();
    auto *a = cJSON_AddArrayToObject(j, "devices");
    for (size_t i = 0; i < model_.size(); ++i) {
      const auto &d = model_.at(i);
      auto *x = cJSON_CreateObject();
      cJSON_AddItemToArray(a, x);
      cJSON_AddStringToObject(x, "device_id", textid(d.id).c_str());
      cJSON_AddStringToObject(x, "name", d.name.data());
      cJSON_AddBoolToObject(x, "available", d.available);
      cJSON_AddStringToObject(x, "manufacturer",
                              adapter_ ? "Zigbee" : "Virtual");
      auto *states = cJSON_AddObjectToObject(x, "states");
      auto *cap = cJSON_AddArrayToObject(x, "capabilities");
      for (size_t k = 0; k < CapabilityCount; ++k)
        if (d.capabilities & capability_bit(static_cast<Capability>(k))) {
          cJSON_AddItemToArray(cap, cJSON_CreateString(caps[k]));
          if (!d.values[k].known)
            cJSON_AddNullToObject(states, caps[k]);
          else if (k < 2)
            cJSON_AddBoolToObject(states, caps[k], d.values[k].value != 0);
          else
            cJSON_AddNumberToObject(states, caps[k],
                                    static_cast<double>(d.values[k].value));
        }
      auto *channels = cJSON_AddArrayToObject(x, "channels");
      for (size_t k = 0; k < d.channel_count; ++k) {
        auto *c = cJSON_CreateObject();
        cJSON_AddItemToArray(channels, c);
        cJSON_AddNumberToObject(c, "channel_id", d.channels[k].id);
        cJSON_AddBoolToObject(c, "supported",
                              d.channels[k].route != RouteKind::Unsupported);
        if (d.channels[k].power.known)
          cJSON_AddBoolToObject(c, "power", d.channels[k].power.value != 0);
        else
          cJSON_AddNullToObject(c, "power");
      }
    }
    return j;
  }
  if (route.rfind("/api/v1/devices/", 0) == 0) {
    DeviceId id;
    auto value = route.substr(16);
    if (!DeviceId::parse(value.data(), value.size(), &id))
      return failure(status, ErrorCode::Invalid);
    if (put) {
      auto *n = field(in, "name");
      if (!json_keys(in, {"name"}) || !cJSON_IsString(n))
        return failure(status, ErrorCode::Invalid);
      DeviceSnapshot old;
      if (!model_.snapshot(id, old))
        return failure(status, ErrorCode::MissingDevice);
      auto v = model_.rename(id, n->valuestring);
      if (!v)
        return failure(status, v.code);
      if (!save_names()) {
        model_.rename(id, old.name.data());
        return failure(status, ErrorCode::Storage);
      }
      return ok();
    }
    if (del) {
      DeviceSnapshot old;
      if (!model_.snapshot(id, old))
        return failure(status, ErrorCode::MissingDevice);
      if (adapter_ && !adapter_->remove(id))
        return failure(status, ErrorCode::Unavailable);
      auto backup_ptr = std::make_unique<DeviceModel>(model_);
      auto &backup = *backup_ptr;
      model_.remove(id);
      if (!save_names()) {
        model_ = backup;
        return failure(status, ErrorCode::Storage);
      }
      for (size_t k = 0; k < old.channel_count; ++k)
        engine_.manual_override({id, old.channels[k].id});
      return ok();
    }
  }
  if (route == "/api/v1/scenarios" && get) {
    auto *j = cJSON_CreateObject();
    auto *a = cJSON_AddArrayToObject(j, "scenarios");
    std::vector<char> buffer(8193);
    for (size_t i = 0; i < engine_.scenarios().size; ++i) {
      size_t n = 0;
      encode_scenario(engine_.scenarios().rules[i], buffer.data(),
                      buffer.size(), n);
      cJSON_AddItemToArray(a, cJSON_ParseWithLength(buffer.data(), n));
    }
    return j;
  }
  if (route == "/api/v1/scenarios" && post) {
    Scenario r;
    auto v = decode_scenario(body, r);
    if (!v)
      return failure(status, v.code);
    v = validate_scenario(r, model_);
    if (!v)
      return failure(status, v.code);
    auto next_ptr = std::make_unique<ScenarioSet>(engine_.scenarios());
    auto &next = *next_ptr;
    for (size_t i = 0; i < next.size; ++i)
      if (next.rules[i].id == r.id)
        return failure(status, ErrorCode::Invalid);
    if (next.size == MaxRules)
      return failure(status, ErrorCode::Capacity);
    next.rules[next.size++] = r;
    v = store_.save(next);
    if (!v)
      return failure(status, v.code);
    engine_.upsert(r);
    status = 201;
    auto *j = ok();
    cJSON_AddNumberToObject(j, "id", r.id);
    return j;
  }
  if (route.rfind("/api/v1/scenarios/", 0) == 0 && (put || del)) {
    std::string token = route.substr(18);
    char *end = nullptr;
    unsigned long id = std::strtoul(token.c_str(), &end, 10);
    if (token.empty() || *end || id == 0 || id > UINT32_MAX)
      return failure(status, ErrorCode::Invalid);
    auto next_ptr = std::make_unique<ScenarioSet>(engine_.scenarios());
    auto &next = *next_ptr;
    size_t i = 0;
    while (i < next.size && next.rules[i].id != id)
      ++i;
    if (i == next.size) {
      status = 404;
      auto *j = ok();
      cJSON_AddBoolToObject(j, "ok", false);
      return j;
    }
    Scenario changed;
    if (put) {
      auto v = decode_scenario(body, changed);
      if (!v || changed.id != id)
        return failure(status, ErrorCode::Invalid);
      v = validate_scenario(changed, model_);
      if (!v)
        return failure(status, v.code);
      next.rules[i] = changed;
    } else {
      for (size_t k = i + 1; k < next.size; ++k)
        next.rules[k - 1] = next.rules[k];
      --next.size;
    }
    auto v = store_.save(next);
    if (!v)
      return failure(status, v.code);
    if (put)
      engine_.upsert(changed);
    else
      engine_.remove(id);
    return ok();
  }
  if (route == "/api/v1/channels/power" && post) {
    DeviceId id;
    int64_t channel = 0;
    auto *power = field(in, "on");
    if (!power)
      power = field(in, "desired_power_on");
    if (!json_keys(in, {"device_id", "channel_id", "on", "desired_power_on"}) ||
        !deviceid(field(in, "device_id"), id) ||
        !json_integer(field(in, "channel_id"), 1, 255, channel) ||
        !cJSON_IsBool(power) ||
        (field(in, "on") && field(in, "desired_power_on")))
      return failure(status, ErrorCode::Invalid);
    DeviceSnapshot d;
    if (!model_.snapshot(id, d))
      return failure(status, ErrorCode::MissingDevice);
    if (!d.available)
      return failure(status, ErrorCode::Unavailable);
    bool found = false;
    for (size_t i = 0; i < d.channel_count; ++i)
      if (d.channels[i].id == channel &&
          d.channels[i].route != RouteKind::Unsupported)
        found = true;
    if (!found)
      return failure(status, ErrorCode::MissingChannel);
    OperationId op = manual_id_++;
    if (adapter_) {
      CommandAction a;
      a.channel = {id, static_cast<ChannelId>(channel)};
      a.on = cJSON_IsTrue(power);
      a.operation_id = op;
      if (!submit_manual(a))
        return failure(status, ErrorCode::Unavailable);
    } else {
      EventBatch e;
      auto v = model_.apply_report({id, Capability::Contact,
                                    Scalar::boolean(cJSON_IsTrue(power)),
                                    static_cast<ChannelId>(channel), true},
                                   clock_.monotonic_ms, e);
      if (!v)
        return failure(status, v.code);
      operation(op, CommandStatus::Confirmed);
    }
    engine_.manual_override({id, static_cast<ChannelId>(channel)});
    status = 202;
    auto *j = ok();
    cJSON_AddNumberToObject(j, "operation_id", op);
    return j;
  }
  if (route.rfind("/api/v1/operations/", 0) == 0 && get) {
    auto token = route.substr(19);
    char *end = nullptr;
    unsigned long id = std::strtoul(token.c_str(), &end, 10);
    if (!token.empty() && !*end)
      for (auto &o : ops_)
        if (o.id == id && id) {
          auto *j = cJSON_CreateObject();
          cJSON_AddNumberToObject(j, "operation_id", id);
          cJSON_AddStringToObject(j, "status",
                                  statuses[static_cast<unsigned>(o.status)]);
          return j;
        }
  }
  if (route == "/api/v1/log" && get) {
    auto *j = cJSON_CreateObject();
    auto *a = cJSON_AddArrayToObject(j, "entries");
    for (size_t i = 0; i < engine_.log().size(); ++i) {
      const auto &l = engine_.log().at(i);
      auto *x = cJSON_CreateObject();
      cJSON_AddItemToArray(a, x);
      cJSON_AddNumberToObject(x, "sequence", static_cast<double>(l.sequence));
      cJSON_AddNumberToObject(x, "monotonic_ms",
                              static_cast<double>(l.monotonic_ms));
      cJSON_AddNumberToObject(x, "rule_id", l.rule_id);
      cJSON_AddNumberToObject(x, "channel_id", l.channel.channel_id);
      cJSON_AddStringToObject(x, "device_id",
                              textid(l.channel.device_id).c_str());
      cJSON_AddStringToObject(x, "status",
                              statuses[static_cast<unsigned>(l.status)]);
      cJSON_AddStringToObject(x, "reason", l.reason.data());
    }
    return j;
  }
  if (route == "/api/v1/network/join" && post) {
    int64_t seconds = 0;
    if (!json_keys(in, {"seconds"}) ||
        !json_integer(field(in, "seconds"), 1, 120, seconds))
      return failure(status, ErrorCode::Invalid);
    if (adapter_ && !adapter_->join(seconds))
      return failure(status, ErrorCode::Unavailable);
    join_until_ = clock_.monotonic_ms + seconds * 1000;
    return ok();
  }
  if (route == "/api/v1/settings" && put) {
    int64_t offset = 0;
    if (!json_keys(in, {"offset_minutes"}) ||
        !json_integer(field(in, "offset_minutes"), -720, 840, offset))
      return failure(status, ErrorCode::Invalid);
    int previous = clock_.offset_minutes;
    clock_.offset_minutes = offset;
    if (!save_names()) {
      clock_.offset_minutes = previous;
      return failure(status, ErrorCode::Storage);
    }
    return ok();
  }
  if (route == "/api/v1/sim/pair" && post) {
    if (clock_.monotonic_ms >= join_until_)
      return failure(status, ErrorCode::Unavailable);
    auto *k = field(in, "kind");
    if (!json_keys(in, {"kind"}) || !cJSON_IsString(k))
      return failure(status, ErrorCode::Invalid);
    bool motion = std::strcmp(k->valuestring, "motion") == 0;
    if (!motion && std::strcmp(k->valuestring, "door") != 0)
      return failure(status, ErrorCode::Invalid);
    DeviceSnapshot device, found;
    for (uint8_t candidate = 4; candidate < 255; ++candidate) {
      device.id = DeviceId({0, 0, 0, 0, 0, 0, 0, candidate});
      if (!model_.snapshot(device.id, found))
        break;
    }
    device.capabilities =
        capability_bit(motion ? Capability::Occupancy : Capability::Contact);
    std::strcpy(device.name.data(),
                motion ? "Новый датчик движения" : "Новый датчик двери");
    auto v = model_.add(device);
    if (!v)
      return failure(status, v.code);
    if (!save_names()) {
      model_.remove(device.id);
      return failure(status, ErrorCode::Storage);
    }
    status = 201;
    auto *j = ok();
    cJSON_AddStringToObject(j, "device_id", textid(device.id).c_str());
    return j;
  }
  if (route == "/api/v1/sim/report" && post) {
    DeviceId id;
    if (!json_keys(in, {"device_id", "capability", "value", "channel_id",
                        "available"}) ||
        !deviceid(field(in, "device_id"), id))
      return failure(status, ErrorCode::Invalid);
    DeviceSnapshot d;
    if (!model_.snapshot(id, d))
      return failure(status, ErrorCode::MissingDevice);
    if (field(in, "available")) {
      if (!cJSON_IsBool(field(in, "available")))
        return failure(status, ErrorCode::Invalid);
      if (!cJSON_IsTrue(field(in, "available"))) {
        model_.mark_unavailable(id);
        return ok();
      }
    }
    auto *cap = field(in, "capability");
    auto *value = field(in, "value");
    if (!cJSON_IsString(cap))
      return failure(status, ErrorCode::Invalid);
    int kind = -1;
    for (size_t i = 0; i < CapabilityCount; ++i)
      if (std::strcmp(cap->valuestring, caps[i]) == 0)
        kind = i;
    if (kind < 0)
      return failure(status, ErrorCode::Unsupported);
    int64_t channel = 0;
    if (field(in, "channel_id") &&
        !json_integer(field(in, "channel_id"), 1, 255, channel))
      return failure(status, ErrorCode::Invalid);
    Scalar scalar;
    if (kind < 2) {
      if (!cJSON_IsBool(value))
        return failure(status, ErrorCode::TypeMismatch);
      scalar = Scalar::boolean(cJSON_IsTrue(value));
    } else {
      int64_t number = 0;
      if (!json_integer(value, -9007199254740991LL, 9007199254740991LL, number))
        return failure(status, ErrorCode::TypeMismatch);
      scalar = Scalar::integer(number);
    }
    EventBatch events;
    auto v = model_.apply_report({id, static_cast<Capability>(kind), scalar,
                                  static_cast<ChannelId>(channel), true},
                                 clock_.monotonic_ms, events);
    if (!v)
      return failure(status, v.code);
    for (size_t i = 0; i < events.size; ++i)
      engine_.on_event(events.events[i], clock_);
    pump();
    auto *j = ok();
    cJSON_AddNumberToObject(
        j, "sequence",
        events.size ? static_cast<double>(events.events[0].sequence) : 0);
    return j;
  }
  if (route == "/api/v1/sim/advance" && post) {
    int64_t delta = 0;
    if (!json_keys(in, {"advance_ms"}) ||
        !json_integer(field(in, "advance_ms"), 0, 86400000, delta))
      return failure(status, ErrorCode::Invalid);
    clock_.monotonic_ms += delta;
    clock_.utc_seconds += delta / 1000;
    pump();
    return ok();
  }
  if (route == "/api/v1/sim/restart" && post) {
    auto set_ptr = std::make_unique<ScenarioSet>();
    auto &set = *set_ptr;
    auto v = store_.load(set);
    if (!v)
      return failure(status, v.code);
    engine_.restore(set);
    engine_.clear_log();
    fixtures(false);
    if (!load_names())
      return failure(status, ErrorCode::Storage);
    ops_ = {};
    join_until_ = 0;
    return ok();
  }
  status = 404;
  auto *j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "error", "not_found");
  return j;
}
ValidationResult HubRuntime::handle_request(std::string_view request, char *out,
                                            size_t capacity,
                                            size_t &used) noexcept {
  std::string response;
  auto result = handle_request_to_string(request, response);
  if (!result)
    return result;
  if (response.size() >= capacity)
    return {ErrorCode::Capacity};
  std::memcpy(out, response.c_str(), response.size() + 1);
  used = response.size();
  return {};
}
ValidationResult
HubRuntime::handle_request_to_string(std::string_view request,
                                     std::string &out) noexcept {
  // The outer transport envelope may exceed8192 because raw JSON is escaped.
  std::string s(request);
  Json envelope(cJSON_ParseWithLengthOpts(s.c_str(), s.size() + 1, nullptr, 1),
                cJSON_Delete);
  if (!envelope)
    return {ErrorCode::Malformed};
  auto *m = field(envelope.get(), "method"), *p = field(envelope.get(), "path"),
       *b = field(envelope.get(), "body");
  if (!cJSON_IsString(m) || !cJSON_IsString(p) || !cJSON_IsString(b))
    return {ErrorCode::Malformed};
  int status = 200;
  Json body(dispatch(m->valuestring, p->valuestring, b->valuestring, status),
            cJSON_Delete);
  Json response(cJSON_CreateObject(), cJSON_Delete);
  cJSON_AddNumberToObject(response.get(), "status", status);
  cJSON_AddItemToObject(response.get(), "body", body.release());
  char *printed = cJSON_PrintUnformatted(response.get());
  if (!printed)
    return {ErrorCode::Capacity};
  out.assign(printed);
  cJSON_free(printed);
  return {};
}
} // namespace scenario
