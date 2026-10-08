idf_component_register(SRCS "condition_evaluator.cpp" "device_model.cpp" "hub_runtime.cpp" "monotonic_clock.cpp" "scenario_codec.cpp" "scenario_engine.cpp" "scenario_log.cpp" "scenario_store.cpp" "scenario_validation.cpp" INCLUDE_DIRS "include" REQUIRES core json)
# Большие временные объекты (ScenarioSet ~30 КБ) не должны попадать в стек задач.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wframe-larger-than=8192)
