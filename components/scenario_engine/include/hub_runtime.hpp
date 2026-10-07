#pragma once
#include "json_tools.hpp"
#include "scenario_engine.hpp"
#include "scenario_store.hpp"
namespace scenario {
class RuntimeAdapter {
public:
  virtual ~RuntimeAdapter() = default;
  virtual bool send(const CommandAction &, const DeviceSnapshot &,
                    const ChannelSnapshot &) noexcept = 0;
  virtual bool join(uint16_t) noexcept = 0;
  virtual bool remove(DeviceId) noexcept { return false; }
};
class HubRuntime {
  DeviceModel model_;
  ScenarioEngine engine_;
  StoreBackend &backend_;
  RuntimeAdapter *adapter_ = nullptr;
  bool ready_ = false;
  struct Pending {
    bool used = false;
    CommandAction action{};
    uint64_t deadline = 0;
  };
  std::array<Pending, MaxPending> pending_{};
  bool send(const CommandAction &);
  void finish(OperationId, CommandStatus);

  ScenarioStore store_;
  ClockState clock_{};
  uint64_t join_until_ = 0;
  struct Operation {
    OperationId id = 0;
    CommandStatus status = CommandStatus::Confirmed;
  };
  std::array<Operation, 200> ops_{};
  size_t op_cursor_ = 0;
  OperationId manual_id_ = 0x80000000U;
  void pump();
  void fixtures(bool);
  bool load_names();
  bool save_names();
  void operation(OperationId, CommandStatus);
  cJSON *dispatch(const char *, const char *, std::string_view, int &);

public:
  explicit HubRuntime(StoreBackend &, RuntimeAdapter * = nullptr);
  bool ready() const noexcept { return ready_; }
  ValidationResult configure_device(const DeviceSnapshot &) noexcept;
  ValidationResult on_report(const DeviceReport &) noexcept;
  void unavailable(DeviceId id) noexcept { model_.mark_unavailable(id); }
  void command_ack(OperationId, bool) noexcept;
  void set_clock(const ClockState &clock) noexcept {
    auto offset = clock_.offset_minutes;
    clock_ = clock;
    clock_.offset_minutes = offset;
  }
  void tick() noexcept;
  bool device(DeviceId id, DeviceSnapshot &out) const noexcept {
    return model_.snapshot(id, out);
  }

  ValidationResult handle_request_to_string(std::string_view,
                                            std::string &) noexcept;
  ValidationResult handle_request(std::string_view, char *, size_t,
                                  size_t &) noexcept;
  void advance_real(uint64_t) noexcept;
};
} // namespace scenario
