#include "scenario_manager.hpp"
#ifdef ESP_PLATFORM
#include "hal_nvs.h"
namespace service {
bool TargetNvsBackend::read(const char *k, std::vector<uint8_t> &out) noexcept {
  uint8_t first = 0;
  uint32_t n = 0;
  auto status = hal_nvs_get_blob(k, &first, 1, &n);
  if (status == HAL_NVS_STATUS_NOT_FOUND)
    return false;
  if (status != HAL_NVS_STATUS_NO_SPACE && status != HAL_NVS_STATUS_OK)
    return false;
  if (!n || n > 65560)
    return false;
  out.resize(n);
  return hal_nvs_get_blob(k, out.data(), n, &n) == HAL_NVS_STATUS_OK;
}
bool TargetNvsBackend::write(const char *k, std::string_view b) noexcept {
  return hal_nvs_set_blob(k, b.data(), b.size()) == HAL_NVS_STATUS_OK;
}
bool TargetNvsBackend::read_slot(unsigned i, std::vector<uint8_t> &o) noexcept {
  return i < 2 && read(i ? "rule_slot1" : "rule_slot0", o);
}
bool TargetNvsBackend::write_slot(unsigned i, std::string_view b) noexcept {
  return i < 2 && write(i ? "rule_slot1" : "rule_slot0", b);
}
bool TargetNvsBackend::read_active_generation(
    unsigned i, std::vector<uint8_t> &o) noexcept {
  return i < 2 && read(i ? "rule_commit1" : "rule_commit0", o);
}
bool TargetNvsBackend::write_active_generation(unsigned i,
                                               std::string_view b) noexcept {
  return i < 2 && write(i ? "rule_commit1" : "rule_commit0", b);
}
bool TargetNvsBackend::read_metadata(std::string &o) noexcept {
  std::vector<uint8_t> b;
  if (!read("rule_metadata", b))
    return false;
  o.assign(b.begin(), b.end());
  return true;
}
bool TargetNvsBackend::write_metadata(std::string_view b) noexcept {
  return b.size() <= 8192 && write("rule_metadata", b);
}
} // namespace service
#endif
