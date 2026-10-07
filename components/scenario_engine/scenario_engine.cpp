// SPDX-License-Identifier: AGPL-3.0-only
#include "scenario_engine.hpp"
#include <algorithm>
namespace scenario {
size_t ScenarioEngine::index(ScenarioId id) const noexcept {
  for (size_t i = 0; i < set_.size; ++i)
    if (set_.rules[i].id == id)
      return i;
  return MaxRules;
}
size_t ScenarioEngine::reserved() const noexcept {
  size_t n = 0;
  for (size_t i = 0; i < set_.size; ++i)
    if (runs_[i].active)
      for (size_t j = runs_[i].cursor; j < set_.rules[i].action_count; ++j)
        if (!runs_[i].skip[j])
          ++n;
  return n;
}
bool ScenarioEngine::busy(ChannelKey k) const noexcept {
  for (const auto &f : inflight_)
    if (f.used && f.action.channel == k)
      return true;
  return false;
}
void ScenarioEngine::cancel(size_t i) noexcept { runs_[i] = {}; }
ValidationResult ScenarioEngine::upsert(const Scenario &r) noexcept {
  auto v = validate_scenario(r, model_);
  if (!v)
    return v;
  size_t i = index(r.id);
  if (i != MaxRules) {
    cancel(i);
    set_.rules[i] = r;
    return {};
  }
  if (set_.size == MaxRules)
    return {ErrorCode::Capacity};
  i = set_.size++;
  while (i > 0 && set_.rules[i - 1].id > r.id) {
    set_.rules[i] = set_.rules[i - 1];
    runs_[i] = runs_[i - 1];
    --i;
  }
  set_.rules[i] = r;
  runs_[i] = {};
  return {};
}
ValidationResult ScenarioEngine::restore(const ScenarioSet &s) noexcept {
  if (s.size > MaxRules)
    return {ErrorCode::Capacity};
  for (size_t i = 0; i < s.size; ++i) {
    auto v = validate_shape(s.rules[i]);
    if (!v)
      return v;
    for (size_t j = 0; j < i; ++j)
      if (s.rules[i].id == s.rules[j].id)
        return {ErrorCode::Invalid};
  }
  reset_pending();
  set_ = s;
  std::sort(set_.rules.begin(), set_.rules.begin() + set_.size,
            [](const Scenario &a, const Scenario &b) { return a.id < b.id; });
  return {};
}
void ScenarioEngine::on_event(const DeviceEvent &e,
                              const ClockState &c) noexcept {
  if (e.sequence == 0 || e.sequence <= last_event_)
    return;
  last_event_ = e.sequence;
  for (size_t i = 0; i < set_.size; ++i) {
    const auto &r = set_.rules[i];
    if (!r.enabled || !matches_trigger(r, e))
      continue;
    if (!validate_scenario(r, model_) ||
        evaluate_condition(r, 0, model_, c) != Truth::True) {
      log_.append(c.monotonic_ms, r.id, {}, 0, CommandStatus::Failed,
                  "conditions_not_true");
      continue;
    }
    size_t old = 0;
    if (runs_[i].active)
      for (size_t j = runs_[i].cursor; j < r.action_count; ++j)
        if (!runs_[i].skip[j])
          ++old;
    if (reserved() - old + r.action_count > MaxPending) {
      log_.append(c.monotonic_ms, r.id, {}, 0, CommandStatus::Failed,
                  "pending_capacity");
      continue;
    }
    cancel(i);
    auto &run = runs_[i];
    run.active = true;
    run.generation = ++generation_;
    run.order = ++order_;
    run.due = c.monotonic_ms;
    log_.append(c.monotonic_ms, r.id, {}, 0, CommandStatus::Queued, "trigger");
  }
}
ActionBatch ScenarioEngine::tick(const ClockState &c) noexcept {
  ActionBatch batch;
  for (auto &f : inflight_)
    if (f.used && c.monotonic_ms >= f.sent && c.monotonic_ms - f.sent >= 5000)
      on_command_result(f.action.operation_id, CommandStatus::Timeout,
                        c.monotonic_ms);
  std::array<size_t, MaxRules> order{};
  for (size_t i = 0; i < set_.size; ++i)
    order[i] = i;
  std::sort(
      order.begin(), order.begin() + set_.size,
      [this](size_t a, size_t b) { return runs_[a].order < runs_[b].order; });
  for (size_t k = 0; k < set_.size; ++k) {
    size_t i = order[k];
    auto &run = runs_[i];
    const auto &r = set_.rules[i];
    if (!run.active || run.waiting)
      continue;
    while (run.cursor < r.action_count) {
      if (run.skip[run.cursor]) {
        ++run.cursor;
        continue;
      }
      const auto &a = r.actions[run.cursor];
      if (a.kind == ActionKind::Delay) {
        run.due += a.delay_ms;
        ++run.cursor;
        continue;
      }
      if (c.monotonic_ms < run.due || busy(a.channel))
        break;
      DeviceSnapshot d;
      bool mapped = false;
      if (model_.snapshot(a.channel.device_id, d) && d.available)
        for (size_t j = 0; j < d.channel_count; ++j)
          if (d.channels[j].id == a.channel.channel_id &&
              d.channels[j].route != RouteKind::Unsupported)
            mapped = true;
      if (!mapped) {
        log_.append(c.monotonic_ms, r.id, a.channel, 0, CommandStatus::Failed,
                    "output_unavailable");
        cancel(i);
        break;
      }
      Inflight *slot = nullptr;
      for (auto &f : inflight_)
        if (!f.used) {
          slot = &f;
          break;
        }
      if (!slot) {
        log_.append(c.monotonic_ms, r.id, a.channel, 0, CommandStatus::Failed,
                    "command_capacity");
        cancel(i);
        break;
      }
      OperationId id = next_operation_++;
      if (!id)
        id = next_operation_++;
      slot->used = true;
      slot->sent = c.monotonic_ms;
      slot->action = {r.id, run.generation, a.channel, a.on, run.due, id};
      run.waiting = id;
      batch.actions[batch.size++] = slot->action;
      log_.append(c.monotonic_ms, r.id, a.channel, id, CommandStatus::Pending,
                  "sent");
      break;
    }
    if (run.active && run.cursor == r.action_count)
      run.active = false;
  }
  return batch;
}
void ScenarioEngine::on_command_result(OperationId id, CommandStatus status,
                                       uint64_t now) noexcept {
  if (status != CommandStatus::Confirmed && status != CommandStatus::Failed &&
      status != CommandStatus::Timeout)
    return;
  for (auto &f : inflight_)
    if (f.used && f.action.operation_id == id) {
      const auto a = f.action;
      f.used = false;
      log_.append(now, a.rule_id, a.channel, id, status,
                  status == CommandStatus::Confirmed ? "confirmed"
                  : status == CommandStatus::Timeout ? "timeout"
                                                     : "failed");
      size_t i = index(a.rule_id);
      if (i != MaxRules && runs_[i].active &&
          runs_[i].generation == a.generation && runs_[i].waiting == id) {
        if (status == CommandStatus::Confirmed) {
          runs_[i].waiting = 0;
          ++runs_[i].cursor;
          runs_[i].due = now;
        } else
          cancel(i);
      }
      break;
    }
}
void ScenarioEngine::manual_override(ChannelKey k) noexcept {
  for (size_t i = 0; i < set_.size; ++i) {
    auto &run = runs_[i];
    if (!run.active)
      continue;
    for (size_t j = run.cursor; j < set_.rules[i].action_count; ++j) {
      const auto &a = set_.rules[i].actions[j];
      if (a.kind == ActionKind::SetChannelPower && a.channel == k) {
        run.skip[j] = true;
        if (j == run.cursor && run.waiting) {
          run.waiting = 0;
          ++run.cursor;
        }
      }
    }
    log_.append(0, set_.rules[i].id, k, 0, CommandStatus::Queued,
                "manual_override");
  }
}
void ScenarioEngine::remove(ScenarioId id) noexcept {
  size_t i = index(id);
  if (i == MaxRules)
    return;
  cancel(i);
  for (size_t j = i + 1; j < set_.size; ++j) {
    set_.rules[j - 1] = set_.rules[j];
    runs_[j - 1] = runs_[j];
  }
  --set_.size;
  set_.rules[set_.size] = {};
  runs_[set_.size] = {};
}
void ScenarioEngine::disable(ScenarioId id) noexcept {
  size_t i = index(id);
  if (i != MaxRules) {
    set_.rules[i].enabled = false;
    cancel(i);
  }
}
void ScenarioEngine::reset_pending() noexcept {
  runs_ = {};
  inflight_ = {};
  last_event_ = 0;
}
} // namespace scenario
