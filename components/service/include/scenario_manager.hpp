#pragma once
#ifdef ESP_PLATFORM
#include "core_commands.hpp"
#include "core_events.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "hub_runtime.hpp"
#include <atomic>
namespace service {
class ServiceRuntime;
class TargetNvsBackend : public scenario::StoreBackend {
  bool read(const char *, std::vector<uint8_t> &) noexcept;
  bool write(const char *, std::string_view) noexcept;

public:
  bool read_slot(unsigned, std::vector<uint8_t> &) noexcept override;
  bool write_slot(unsigned, std::string_view) noexcept override;
  bool read_active_generation(unsigned,
                              std::vector<uint8_t> &) noexcept override;
  bool write_active_generation(unsigned, std::string_view) noexcept override;
  bool read_metadata(std::string &) noexcept override;
  bool write_metadata(std::string_view) noexcept override;
};
class ScenarioManager : public scenario::RuntimeAdapter {
  ServiceRuntime &runtime_;
  TargetNvsBackend backend_;
  scenario::HubRuntime hub_;
  QueueHandle_t requests_{};
  struct CommandLink {
    uint32_t radio_id = 0;
    scenario::OperationId operation_id = 0;
    uint64_t deadline = 0;
  };
  std::array<CommandLink, core::CoreCommandDispatcher::kMaxPending>
      command_links_{};
  struct Request {
    std::atomic<unsigned> refs{2};
    SemaphoreHandle_t done{xSemaphoreCreateBinary()};
    std::string input, output;
    bool success = false;
  };
  static void release(Request *);
  struct Identity {
    core::DeviceId id{};
    uint16_t address = 0xffff;
  };
  std::array<Identity, scenario::MaxDevices> identities_{};
  bool send(const scenario::CommandAction &, const scenario::DeviceSnapshot &,
            const scenario::ChannelSnapshot &) noexcept override;
  bool join(uint16_t) noexcept override;
  bool remove(core::DeviceId) noexcept override;

public:
  explicit ScenarioManager(ServiceRuntime &);
  bool ready() const noexcept { return requests_ && hub_.ready(); }
  bool request(std::string_view, std::string &) noexcept;
  void identify(core::CoreEvent &) noexcept;
  void on_event(const core::CoreEvent &) noexcept;
  void tick() noexcept;
};
} // namespace service
#endif
