#pragma once
#include "core_events.hpp"
#include "hub_runtime.hpp"
namespace service {
void forward_scenario_event(scenario::HubRuntime &,
                            const core::CoreEvent &) noexcept;
}
