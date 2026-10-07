// SPDX-License-Identifier: AGPL-3.0-only
#include "scenario_codec.hpp"
#include "cJSON.h"
#include "json_tools.hpp"
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <string>
namespace scenario {
namespace {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
cJSON *get(const cJSON *j, const char *k) {
  return cJSON_GetObjectItemCaseSensitive(j, k);
}
bool keys(const cJSON *j, std::initializer_list<const char *> allowed) {
  if (!cJSON_IsObject(j))
    return false;
  for (auto *x = j->child; x; x = x->next) {
    bool ok = false;
    for (auto *k : allowed)
      if (x->string && std::strcmp(x->string, k) == 0)
        ok = true;
    if (!ok)
      return false;
  }
  return true;
}
bool unique(const cJSON *j, unsigned depth = 0) {
  if (depth > 32)
    return false;
  if (cJSON_IsObject(j))
    for (auto *x = j->child; x; x = x->next)
      for (auto *y = x->next; y; y = y->next)
        if (x->string && y->string && std::strcmp(x->string, y->string) == 0)
          return false;
  for (auto *x = j->child; x; x = x->next)
    if (!unique(x, depth + 1))
      return false;
  return true;
}
bool number(const cJSON *j, int64_t lo, int64_t hi, int64_t &v) {
  if (!cJSON_IsNumber(j) || !std::isfinite(j->valuedouble) ||
      std::floor(j->valuedouble) != j->valuedouble ||
      j->valuedouble < static_cast<double>(lo) ||
      j->valuedouble > static_cast<double>(hi))
    return false;
  v = static_cast<int64_t>(j->valuedouble);
  return true;
}
bool id(const cJSON *j, DeviceId &out) {
  return cJSON_IsString(j) && std::strlen(j->valuestring) == 16 &&
         DeviceId::parse(j->valuestring, 16, &out);
}
int named(const cJSON *j, std::initializer_list<const char *> names) {
  if (!cJSON_IsString(j))
    return -1;
  int i = 0;
  for (auto *s : names) {
    if (std::strcmp(j->valuestring, s) == 0)
      return i;
    ++i;
  }
  return -1;
}
const char *event_names[] = {"contact.opened", "contact.closed",
                             "occupancy.detected", "occupancy.cleared"};
const char *cap_names[] = {"contact", "occupancy", "battery", "temperature"};
const char *op_names[] = {"eq", "ne", "lt", "le", "gt", "ge"};
bool lexical(std::string_view s) {
  bool quoted = false;
  unsigned depth = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (quoted) {
      if (c == '\\') {
        if (i + 5 < s.size() && s.substr(i + 1, 5) == "u0000")
          return false;
        ++i;
      } else if (c == '"')
        quoted = false;
    } else if (c == '"')
      quoted = true;
    else if (c == '{' || c == '[') {
      if (++depth > 32)
        return false;
    } else if (c == '}' || c == ']') {
      if (!depth)
        return false;
      --depth;
    }
  }
  return !quoted && depth == 0;
}
void addid(cJSON *j, const char *k, DeviceId id) {
  char b[17]{};
  id.format(b, 16);
  cJSON_AddStringToObject(j, k, b);
}
} // namespace
cJSON *parse_json(std::string_view s) noexcept {
  if (s.size() > 8192 || !valid_utf8(s.data(), s.size()) || !lexical(s))
    return nullptr;
  std::string t(s);
  auto *j = cJSON_ParseWithLengthOpts(t.c_str(), t.size() + 1, nullptr, 1);
  if (j && !unique(j)) {
    cJSON_Delete(j);
    j = nullptr;
  }
  return j;
}
bool json_keys(const cJSON *j, std::initializer_list<const char *> k) noexcept {
  return keys(j, k);
}
bool json_integer(const cJSON *j, int64_t lo, int64_t hi, int64_t &v) noexcept {
  return number(j, lo, hi, v);
}
ValidationResult decode_scenario(std::string_view input,
                                 Scenario &out) noexcept {
  if (input.size() > 8192)
    return {ErrorCode::TooLarge};
  if (!valid_utf8(input.data(), input.size()) || !lexical(input))
    return {ErrorCode::Malformed};
  std::string text(input);
  const char *end = nullptr;
  Json json(cJSON_ParseWithLengthOpts(text.c_str(), text.size() + 1, &end, 1),
            cJSON_Delete);
  auto *j = json.get();
  if (!j || !unique(j) ||
      !keys(j, {"id", "name", "enabled", "triggers", "conditions", "actions"}))
    return {ErrorCode::Malformed};
  Scenario r;
  int64_t n = 0;
  if (!number(get(j, "id"), 1, UINT32_MAX, n))
    return {ErrorCode::Invalid};
  r.id = static_cast<uint32_t>(n);
  auto *name = get(j, "name");
  if (!cJSON_IsString(name) || std::strlen(name->valuestring) > 96)
    return {ErrorCode::Invalid};
  std::strcpy(r.name.data(), name->valuestring);
  auto *enabled = get(j, "enabled");
  if (!cJSON_IsBool(enabled))
    return {ErrorCode::Invalid};
  r.enabled = cJSON_IsTrue(enabled);
  auto *tr = get(j, "triggers");
  auto *nodes = get(j, "conditions");
  auto *actions = get(j, "actions");
  if (!cJSON_IsArray(tr) || cJSON_GetArraySize(tr) > 8 ||
      !cJSON_IsArray(actions) || cJSON_GetArraySize(actions) > 8 ||
      (nodes && (!cJSON_IsArray(nodes) || cJSON_GetArraySize(nodes) > 16)))
    return {ErrorCode::Invalid};
  for (auto *t = tr->child; t; t = t->next) {
    auto &x = r.triggers[r.trigger_count++];
    int k = named(get(t, "kind"), {"contact.opened", "contact.closed",
                                   "occupancy.detected", "occupancy.cleared"});
    if (!keys(t, {"device_id", "kind"}) ||
        !id(get(t, "device_id"), x.device_id) || k < 0)
      return {ErrorCode::Invalid};
    x.kind = static_cast<EventKind>(k);
  }
  if (nodes)
    for (auto *t = nodes->child; t; t = t->next) {
      auto &x = r.nodes[r.node_count++];
      int k = named(get(t, "kind"), {"all", "any", "state", "time_window"});
      if (k < 0)
        return {ErrorCode::Invalid};
      x.kind = static_cast<NodeKind>(k);
      if (k < 2) {
        if (!keys(t, {"kind", "children"}))
          return {ErrorCode::Invalid};
        auto *children = get(t, "children");
        if (!cJSON_IsArray(children) || cJSON_GetArraySize(children) > 16)
          return {ErrorCode::Invalid};
        for (auto *v = children->child; v; v = v->next) {
          if (!number(v, 0, 15, n))
            return {ErrorCode::Invalid};
          x.children[x.child_count++] = static_cast<uint8_t>(n);
        }
      } else if (k == 2) {
        if (!keys(t, {"kind", "device_id", "capability", "op", "value"}) ||
            !id(get(t, "device_id"), x.device_id))
          return {ErrorCode::Invalid};
        int cap = named(get(t, "capability"),
                        {"contact", "occupancy", "battery", "temperature"}),
            op = named(get(t, "op"), {"eq", "ne", "lt", "le", "gt", "ge"});
        if (cap < 0 || op < 0)
          return {ErrorCode::Invalid};
        x.capability = static_cast<Capability>(cap);
        x.op = static_cast<Compare>(op);
        auto *v = get(t, "value");
        if (cap < 2) {
          if (!cJSON_IsBool(v))
            return {ErrorCode::TypeMismatch};
          x.expected = Scalar::boolean(cJSON_IsTrue(v));
        } else {
          if (!number(v, -9007199254740991LL, 9007199254740991LL, n))
            return {ErrorCode::TypeMismatch};
          x.expected = Scalar::integer(n);
        }
      } else {
        if (!keys(t, {"kind", "start", "end"}) ||
            !number(get(t, "start"), 0, 1439, n))
          return {ErrorCode::Invalid};
        x.start_minute = n;
        if (!number(get(t, "end"), 0, 1439, n))
          return {ErrorCode::Invalid};
        x.end_minute = n;
      }
    }
  for (auto *t = actions->child; t; t = t->next) {
    auto &x = r.actions[r.action_count++];
    int k = named(get(t, "kind"), {"set_channel_power", "delay"});
    if (k < 0)
      return {ErrorCode::Invalid};
    x.kind = static_cast<ActionKind>(k);
    if (k == 0) {
      if (!keys(t, {"kind", "device_id", "channel_id", "on"}) ||
          !id(get(t, "device_id"), x.channel.device_id) ||
          !number(get(t, "channel_id"), 1, 255, n) ||
          !cJSON_IsBool(get(t, "on")))
        return {ErrorCode::Invalid};
      x.channel.channel_id = n;
      x.on = cJSON_IsTrue(get(t, "on"));
    } else {
      if (!keys(t, {"kind", "delay_ms"}) ||
          !number(get(t, "delay_ms"), 0, 86400000, n))
        return {ErrorCode::Invalid};
      x.delay_ms = n;
    }
  }
  auto v = validate_shape(r);
  if (!v)
    return v;
  out = r;
  return {};
}
ValidationResult encode_scenario(const Scenario &r, char *out, size_t capacity,
                                 size_t &used) noexcept {
  used = 0;
  auto v = validate_shape(r);
  if (!v)
    return v;
  Json root(cJSON_CreateObject(), cJSON_Delete);
  auto *j = root.get();
  if (!j)
    return {ErrorCode::Capacity};
  cJSON_AddNumberToObject(j, "id", r.id);
  cJSON_AddStringToObject(j, "name", r.name.data());
  cJSON_AddBoolToObject(j, "enabled", r.enabled);
  auto *tr = cJSON_AddArrayToObject(j, "triggers");
  auto *nodes = cJSON_AddArrayToObject(j, "conditions");
  auto *acts = cJSON_AddArrayToObject(j, "actions");
  for (size_t i = 0; i < r.trigger_count; ++i) {
    auto *t = cJSON_CreateObject();
    cJSON_AddItemToArray(tr, t);
    addid(t, "device_id", r.triggers[i].device_id);
    cJSON_AddStringToObject(
        t, "kind", event_names[static_cast<unsigned>(r.triggers[i].kind)]);
  }
  const char *kinds[] = {"all", "any", "state", "time_window"};
  for (size_t i = 0; i < r.node_count; ++i) {
    const auto &n = r.nodes[i];
    auto *t = cJSON_CreateObject();
    cJSON_AddItemToArray(nodes, t);
    cJSON_AddStringToObject(t, "kind", kinds[static_cast<unsigned>(n.kind)]);
    if (n.kind == NodeKind::All || n.kind == NodeKind::Any) {
      auto *a = cJSON_AddArrayToObject(t, "children");
      for (size_t k = 0; k < n.child_count; ++k)
        cJSON_AddItemToArray(a, cJSON_CreateNumber(n.children[k]));
    } else if (n.kind == NodeKind::State) {
      addid(t, "device_id", n.device_id);
      cJSON_AddStringToObject(t, "capability",
                              cap_names[static_cast<unsigned>(n.capability)]);
      cJSON_AddStringToObject(t, "op", op_names[static_cast<unsigned>(n.op)]);
      if (n.expected.type == ScalarType::Boolean)
        cJSON_AddBoolToObject(t, "value", n.expected.value != 0);
      else
        cJSON_AddNumberToObject(t, "value",
                                static_cast<double>(n.expected.value));
    } else {
      cJSON_AddNumberToObject(t, "start", n.start_minute);
      cJSON_AddNumberToObject(t, "end", n.end_minute);
    }
  }
  for (size_t i = 0; i < r.action_count; ++i) {
    const auto &a = r.actions[i];
    auto *t = cJSON_CreateObject();
    cJSON_AddItemToArray(acts, t);
    cJSON_AddStringToObject(
        t, "kind", a.kind == ActionKind::Delay ? "delay" : "set_channel_power");
    if (a.kind == ActionKind::Delay)
      cJSON_AddNumberToObject(t, "delay_ms", a.delay_ms);
    else {
      addid(t, "device_id", a.channel.device_id);
      cJSON_AddNumberToObject(t, "channel_id", a.channel.channel_id);
      cJSON_AddBoolToObject(t, "on", a.on);
    }
  }
  char *printed = cJSON_PrintUnformatted(j);
  if (!printed)
    return {ErrorCode::Capacity};
  size_t n = std::strlen(printed);
  ValidationResult result;
  if (n > 8192)
    result = {ErrorCode::TooLarge};
  else if (!out || capacity <= n)
    result = {ErrorCode::Capacity};
  else {
    std::memcpy(out, printed, n + 1);
    used = n;
  }
  cJSON_free(printed);
  return result;
}
} // namespace scenario
