// SPDX-License-Identifier: AGPL-3.0-only
#include "device_model.hpp"
#include "scenario_validation.hpp"
#include <cstring>
namespace scenario {
ValidationResult DeviceModel::add(const DeviceSnapshot &d) noexcept {
  if (!d.id.valid() || d.channel_count > MaxChannels ||
      d.capabilities >= (1U << CapabilityCount))
    return {ErrorCode::Invalid};
  for (size_t i = 0; i < size_; ++i)
    if (devices_[i].id == d.id)
      return {ErrorCode::Invalid};
  for (size_t i = 0; i < d.channel_count; ++i) {
    if (d.channels[i].id == 0)
      return {ErrorCode::Invalid};
    for (size_t j = 0; j < i; ++j)
      if (d.channels[j].id == d.channels[i].id)
        return {ErrorCode::Invalid};
  }
  if (size_ == MaxDevices) {
    return {ErrorCode::Capacity};
  }
  devices_[size_++] = d;
  return {};
}
ValidationResult DeviceModel::apply_report(const DeviceReport &r, uint64_t now,
                                           EventBatch &e) noexcept {
  e.size = 0;
  DeviceSnapshot *d = nullptr;
  for (size_t i = 0; i < size_; ++i)
    if (devices_[i].id == r.id) {
      d = &devices_[i];
      break;
    }
  if (!d) {
    return {ErrorCode::MissingDevice};
  }
  if (!r.mapping_verified) {
    return {ErrorCode::Unsupported};
  }
  Scalar *value = nullptr;
  ScalarType expected = ScalarType::Boolean;
  if (r.channel_id) {
    for (size_t i = 0; i < d->channel_count; ++i)
      if (d->channels[i].id == r.channel_id) {
        if (d->channels[i].route == RouteKind::Unsupported)
          return {ErrorCode::Unsupported};
        value = &d->channels[i].power;
        break;
      }
    if (!value)
      return {ErrorCode::MissingChannel};
  } else {
    auto c = static_cast<size_t>(r.capability);
    if (c >= CapabilityCount ||
        !(d->capabilities & capability_bit(r.capability)))
      return {ErrorCode::Unsupported};
    value = &d->values[c];
    if (r.capability == Capability::Temperature ||
        r.capability == Capability::Battery)
      expected = ScalarType::Integer;
  }
  if (r.value.type != expected ||
      (r.value.known && expected == ScalarType::Boolean &&
       (r.value.value < 0 || r.value.value > 1)))
    return {ErrorCode::TypeMismatch};
  const Scalar previous = *value;
  *value = r.value;
  d->available = true;
  if (!r.channel_id && r.value.known && previous.known &&
      previous.value != r.value.value &&
      (r.capability == Capability::Contact ||
       r.capability == Capability::Occupancy)) {
    const bool v = r.value.value != 0;
    EventKind k =
        r.capability == Capability::Contact
            ? (v ? EventKind::ContactOpened : EventKind::ContactClosed)
            : (v ? EventKind::OccupancyDetected : EventKind::OccupancyCleared);
    e.events[e.size++] = {r.id, k, v, now, ++sequence_};
  }
  return {};
}
bool DeviceModel::snapshot(DeviceId id, DeviceSnapshot &out) const noexcept {
  for (size_t i = 0; i < size_; ++i)
    if (devices_[i].id == id) {
      out = devices_[i];
      return true;
    }
  return false;
}
void DeviceModel::mark_unavailable(DeviceId id) noexcept {
  for (size_t i = 0; i < size_; ++i)
    if (devices_[i].id == id)
      devices_[i].available = false;
}
void DeviceModel::reset_runtime_state() noexcept {
  for (size_t i = 0; i < size_; ++i) {
    auto &d = devices_[i];
    d.available = false;
    for (auto &v : d.values)
      v.known = false;
    for (auto &c : d.channels)
      c.power.known = false;
  }
  sequence_ = 0;
}
ValidationResult DeviceModel::rename(DeviceId id,
                                     std::string_view name) noexcept {
  if (name.empty() || name.size() > 96 || !valid_utf8(name.data(), name.size()))
    return {ErrorCode::Invalid};
  for (size_t i = 0; i < size_; ++i)
    if (devices_[i].id == id) {
      devices_[i].name = {};
      std::memcpy(devices_[i].name.data(), name.data(), name.size());
      return {};
    }
  return {ErrorCode::MissingDevice};
}
void DeviceModel::remove(DeviceId id) noexcept {
  for (size_t i = 0; i < size_; ++i)
    if (devices_[i].id == id) {
      for (size_t j = i + 1; j < size_; ++j)
        devices_[j - 1] = devices_[j];
      devices_[--size_] = {};
      break;
    }
}
} // namespace scenario
