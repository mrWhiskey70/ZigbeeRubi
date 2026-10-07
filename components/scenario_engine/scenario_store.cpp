// SPDX-License-Identifier: AGPL-3.0-only
#include "scenario_store.hpp"
#include <cstring>
#include <string>
namespace scenario {
namespace {
uint32_t crc(std::string_view s) {
  uint32_t c = ~0U;
  for (unsigned char b : s) {
    c ^= b;
    for (int i = 0; i < 8; ++i)
      c = (c >> 1) ^
          (0xedb88320U & static_cast<uint32_t>(-static_cast<int32_t>(c & 1)));
  }
  return ~c;
}
void put(std::string &s, uint64_t n, unsigned bytes) {
  for (unsigned i = 0; i < bytes; ++i)
    s.push_back(static_cast<char>(n >> (i * 8)));
}
uint64_t get(std::string_view s, size_t p, unsigned bytes) {
  uint64_t n = 0;
  for (unsigned i = 0; i < bytes; ++i)
    n |= static_cast<uint64_t>(static_cast<uint8_t>(s[p + i])) << (i * 8);
  return n;
}
std::string_view view(const std::vector<uint8_t> &v) {
  return {reinterpret_cast<const char *>(v.data()), v.size()};
}
struct Current {
  uint64_t generation = 0;
  std::string payload;
  bool any = false;
};
Current current(StoreBackend &b) {
  Current best;
  for (unsigned i = 0; i < 2; ++i) {
    std::vector<uint8_t> record;
    if (!b.read_active_generation(i, record))
      continue;
    best.any = true;
    auto r = view(record);
    if (r.size() != 20 || get(r, 0, 4) != 0x52434d31 ||
        crc(r.substr(0, 16)) != get(r, 16, 4))
      continue;
    uint64_t generation = get(r, 4, 8);
    unsigned slot = get(r, 12, 4);
    if (!generation || slot > 1 || generation <= best.generation)
      continue;
    std::vector<uint8_t> data;
    if (!b.read_slot(slot, data))
      continue;
    auto s = view(data);
    if (s.size() < 24 || s.size() > 65560 || get(s, 0, 4) != 0x52534c31 ||
        get(s, 4, 4) != 1 || get(s, 8, 8) != generation ||
        get(s, 16, 4) != s.size() - 24 || crc(s.substr(24)) != get(s, 20, 4))
      continue;
    best.generation = generation;
    best.payload.assign(s.substr(24));
  }
  return best;
}
ValidationResult serialize(const ScenarioSet &s, std::string &out) {
  if (s.size > MaxRules)
    return {ErrorCode::Capacity};
  out.clear();
  put(out, s.size, 4);
  std::vector<char> buffer(8193);
  for (size_t i = 0; i < s.size; ++i) {
    for (size_t j = 0; j < i; ++j)
      if (s.rules[i].id == s.rules[j].id)
        return {ErrorCode::Invalid};
    size_t n = 0;
    auto r = encode_scenario(s.rules[i], buffer.data(), buffer.size(), n);
    if (!r)
      return r;
    if (out.size() + n + 4 > 65536)
      return {ErrorCode::TooLarge};
    put(out, n, 4);
    out.append(buffer.data(), n);
  }
  return {};
}
ValidationResult deserialize(std::string_view p, ScenarioSet &out) {
  if (p.size() < 4 || p.size() > 65536)
    return {ErrorCode::Storage};
  uint64_t count = get(p, 0, 4);
  if (count > MaxRules)
    return {ErrorCode::Storage};
  ScenarioSet s;
  size_t pos = 4;
  for (size_t i = 0; i < count; ++i) {
    if (pos + 4 > p.size())
      return {ErrorCode::Storage};
    size_t n = get(p, pos, 4);
    pos += 4;
    if (n > 8192 || n > p.size() - pos)
      return {ErrorCode::Storage};
    auto r = decode_scenario(p.substr(pos, n), s.rules[i]);
    if (!r)
      return {ErrorCode::Storage};
    for (size_t j = 0; j < i; ++j)
      if (s.rules[i].id == s.rules[j].id)
        return {ErrorCode::Storage};
    ++s.size;
    pos += n;
  }
  if (pos != p.size())
    return {ErrorCode::Storage};
  out = s;
  return {};
}
} // namespace
ValidationResult ScenarioStore::load(ScenarioSet &out) noexcept {
  auto c = current(backend_);
  if (!c.generation) {
    if (c.any)
      return {ErrorCode::Storage};
    out = {};
    return {};
  }
  return deserialize(c.payload, out);
}
ValidationResult ScenarioStore::save(const ScenarioSet &s) noexcept {
  std::string payload;
  auto r = serialize(s, payload);
  if (!r)
    return r;
  auto old = current(backend_);
  if (old.generation && old.payload == payload)
    return {};
  if (old.any && !old.generation)
    return {ErrorCode::Storage};
  if (old.generation == UINT64_MAX)
    return {ErrorCode::Storage};
  uint64_t generation = old.generation + 1;
  unsigned slot = generation % 2;
  std::string data;
  put(data, 0x52534c31, 4);
  put(data, 1, 4);
  put(data, generation, 8);
  put(data, payload.size(), 4);
  put(data, crc(payload), 4);
  data += payload;
  if (!backend_.write_slot(slot, data))
    return {ErrorCode::Storage};
  std::vector<uint8_t> read;
  if (!backend_.read_slot(slot, read) || view(read) != data)
    return {ErrorCode::Storage};
  std::string record;
  put(record, 0x52434d31, 4);
  put(record, generation, 8);
  put(record, slot, 4);
  put(record, crc(record), 4);
  if (!backend_.write_active_generation(slot, record))
    return {ErrorCode::Storage};
  return {};
}
} // namespace scenario
