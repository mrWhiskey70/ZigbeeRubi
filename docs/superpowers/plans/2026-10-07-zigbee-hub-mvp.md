# ESP32-C6 Zigbee Hub MVP Implementation Plan

**Project repository / source of truth:** https://github.com/mrWhiskey70/ZigbeeRubi. Код, изменения плана и результаты проверок ведутся в этом репозитории.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Выполнение: в текущей сессии; подзадачи по порядку. Перед новым кодом требуется review этого письменного плана.

**Goal:** Подготовить автономный Zigbee-хаб на C6 с русским веб-интерфейсом, сценариями И/ИЛИ, таймерами и проверяемым симулятором до получения оборудования.

**Architecture:** Расширяем фиксированный upstream Allekslar; добавляем переносимые C++ модель устройств и движок сценариев. Host-симулятор и ESP32 используют один движок и один JSON-контракт. Команды каналов проходят через адаптер и очередь, а HTTP/JavaScript не исполняют сценарии сами.

**Tech Stack:** C++17, ESP-IDF 5.5.2, FreeRTOS, esp-zigbee-lib 1.6.8, esp-zboss-lib 1.6.4, NVS, CMake, GCC; cJSON 1.7.19 для общего codec; Python 3 standard library для host HTTP bridge; HTML/CSS/JavaScript без CDN.

**Spec:** `docs/superpowers/specs/2026-10-07-zigbee-hub-design.md`, согласован 7 октября 2026 года сообщением «продолжаем».

## Global Constraints

- Upstream: `Allekslar/zigbee-gateway_v1`, commit `1838e5b35bfaf3cc8b328ddc044b997019533881`; лицензия AGPL-3.0-only и авторство сохраняются.
- ESP-IDF 5.5.2; esp-zigbee-lib 1.6.8; esp-zboss-lib 1.6.4; mdns 1.9.1; target esp32c6; Flash 16 МБ после проверки физического объёма.
- Не требуется использовать все 16 МБ; исходная разметка 8 МБ допустима с неиспользуемым резервом.
- До 16 устройств, до 4 каналов на исполнительное устройство, до 24 сценариев.
- Сценарий: максимум 8 событий запуска, 16 узлов условий, глубина до 4, до 8 действий.
- Не более 64 ожидающих действий. Общий сериализованный объём сценариев не более 64 КиБ, одна запись не более 8 КиБ.
- Кольцевой журнал RAM: 200 записей; телеметрия и журнал не записываются во Flash при каждом отчёте.
- IEEE/EUI-64 — стабильная идентичность; short_addr — только текущий маршрут.
- События запуска объединяются через ИЛИ. Условия состояния объединяются через И/ИЛИ; пустые группы недопустимы.
- Первый отчёт не является событием; повтор одинакового отчёта не является переходом.
- UNKNOWN не равен закрытой двери или отсутствию движения. Исполнение только при TRUE.
- Условия времени: настраиваемый фиксированный UTC offset, по умолчанию UTC+7; неизвестные часы дают UNKNOWN.
- Таймеры монотонные; перезапуск сбрасывает ожидающие действия, но сохраняет правила/привязки/enabled.
- Новый запуск заменяет ожидающую последовательность того же правила. Ручная команда отменяет ожидающие действия своего канала.
- Ошибка команды прекращает последовательность; автоматических повторов команд MVP нет.
- Состояние реле не запускает другие сценарии. Не реализуется совпадение отдельных событий в окне времени.
- Все новые HTTP-контракты — `/api/v1`; существующие `/api/*` сохраняются.
- Интерфейс на русском, телефон/ПК, без облака/CDN; симулятор помечен постоянно видимым баннером.

## Review Focus

1. Переполнение/оборачивание 32-bit ESP uptime и большой шаг виртуального времени: задержки остаются корректными — тест в задаче 3.
2. Позднее подтверждение команды после ручного override: старое задание не продолжает отменённую последовательность — тест в задаче 3.
3. Несколько событий из одной порции Zigbee-отчётов: очередность детерминирована, каналы не перепутаны — тест в задачах 3/7.
4. Повреждённый JSON, повтор ключей, HTML в имени: запись отклонена либо имя отображено как текст; текущая конфигурация цела — тест в задачах 4/6.
5. Разрыв питания между записью слота и сменой указателя: сохраняется последняя подтверждённая конфигурация — fault-injection в задаче 4.

---

## Общие типы и договорённости

Типы определяет задача 1 в `components/scenario_engine/include/scenario_types.hpp`:

- `DeviceId = core::DeviceId`, `ScenarioId = uint32_t`, `ChannelId = uint8_t`, `OperationId = uint32_t`; ноль для трёх последних ID недопустим.
- `Truth { False, Unknown, True }`; `EventKind { ContactOpened, ContactClosed, OccupancyDetected, OccupancyCleared }`.
- `Scalar`: типизированный bool, signed integer или enum; отсутствующее значение задаётся `known=false`, не фиктивным нулём.
- `ChannelKey { DeviceId device_id; ChannelId channel_id; }`, сравнение по обоим полям.
- `DeviceSnapshot`: ID, known/availability, типизированные capabilities и максимум 4 отдельных ChannelSnapshot.
- `DeviceEvent`: ID, EventKind, значение, `uint64_t monotonic_ms`, `uint64_t sequence`.
- `ClockState`: `uint64_t monotonic_ms`, known wall time, UTC seconds, offset_minutes.
- `ConditionNode`: All/Any/State/TimeWindow, индексы children, predicate или start/end minute. Узлы в одном массиве до 16; depth считается от root=1.
- `Scenario`: ID, enabled, имя UTF-8 до 96 байт, bounded triggers/nodes/actions. Отсутствие conditions означает TRUE; явно пустая группа запрещена.
- `Action`: SetChannelPower с ChannelKey и bool или Delay с uint32_t delay_ms. Допустим delay 0..86400000 мс; нулевой delay не блокирует.
- `ValidationResult { bool ok; ErrorCode code; }`, `ErrorCode` с machine-readable именем и русским описанием на уровне API/UI.
- `ActionBatch`: максимум 64 записей с rule_id, generation, channel, value, due_ms, operation_id.
- `CommandStatus { Queued, Pending, Confirmed, Failed, Timeout }`.

Работа с числами JSON: finite integer, строгие диапазоны. DeviceId сериализуется 16 hex-символами; ID/sequence из JSON должны помещаться в точно представимый диапазон IEEE754 и в целевой целочисленный тип. Логические значения принимаются только как JSON bool.

Задачи 1–6 дают проверяемый host-MVP. Задачи 7–8 интегрируют его в target и выпускают набор для аппаратного этапа. Нельзя выдавать выполнение задач 1–6 за проверку Zigbee на C6.

### Task 1: Baseline и нормализованная модель устройств

**Files:**
- Import: tracked upstream files из фиксированного commit, без изменения байтов и лицензий; сохранить существующие docs/superpowers.
- Create: `upstream-baseline.json`, `components/scenario_engine/CMakeLists.txt`.
- Create: `components/scenario_engine/include/scenario_types.hpp`, `components/scenario_engine/include/device_model.hpp`, `components/scenario_engine/device_model.cpp`.
- Create: `test/scenarios/CMakeLists.txt`, `test/scenarios/test_device_model.cpp`.
- Modify: `sdkconfig.defaults.esp32c6` только при target-конфигурации задачи 8.

**Interfaces:**
- Consumes: upstream `core::DeviceId`, `core::DeviceId::parse`, `core::DeviceId::format`.
- Produces: `ValidationResult DeviceModel::apply_report(const DeviceReport&, uint64_t now_ms, EventBatch&) noexcept`; `bool DeviceModel::snapshot(DeviceId, DeviceSnapshot&) const noexcept`; `void DeviceModel::mark_unavailable(DeviceId) noexcept`; `void DeviceModel::reset_runtime_state() noexcept`.
- DeviceReport включает reported capability и optional channel_id; EventBatch ограничен 16 событиями. Отчёт без подтверждённого endpoint/channel mapping не обновляет случайный канал.

- [ ] **Step 1: Загрузить baseline и зафиксировать происхождение**
  Скачать tracked files строго из commit, проверить sha/размеры; не импортировать бинарные cache или secrets. Добавить origin metadata только в upstream-baseline.json. Версии из lock остаются фиксированными. Подготовить необходимые host build tools; USB/прошивка не требуются.
- [ ] **Step 2: Написать failing test_device_model**
  `first_report_produces_no_event`: первый contact=open → EventBatch.size=0; затем closed → один ContactClosed. `duplicate_report_produces_no_event`: повтор closed → 0. `three_channels_are_independent`: отчёт channel2=ON не меняет known/value каналов1/3. `unknown_fingerprint_is_not_mapped`: без mapping → ошибка и прежнее состояние. `capacity_is_16`: семнадцатое устройство отклонено.
- [ ] **Step 3: Подтвердить RED**
  `cmake -S test/scenarios -B build-scenarios && cmake --build build-scenarios && ctest --test-dir build-scenarios -R test_device_model --output-on-failure`; отсутствие новых symbols/реализации даёт FAIL. Не считать ошибку окружения доказательством RED.
- [ ] **Step 4: Реализовать интерфейсы и типы**
  Bounded arrays, стабильный DeviceId, отдельные channel records, события только на известном переходе, политика availability отдельно от scalar known. Числовой predicate не сравнивает scalar другого типа.
- [ ] **Step 5: Проверить и commit**
  Та же команда → PASS; повторить четыре штатных Tuya-теста. Commit: `feat: add normalized prototype device and channel model`.

### Task 2: Валидация и вычисление И/ИЛИ

**Files:**
- Create: `components/scenario_engine/include/scenario_validation.hpp`, `components/scenario_engine/scenario_validation.cpp`.
- Create: `components/scenario_engine/include/condition_evaluator.hpp`, `components/scenario_engine/condition_evaluator.cpp`.
- Create: `test/scenarios/test_condition_evaluator.cpp`, `test/scenarios/test_scenario_validation.cpp`.
- Modify: scenario component/test CMakeLists.

**Interfaces:**
- Consumes: Scenario/ClockState/DeviceModel из задачи 1.
- Produces: `ValidationResult validate_scenario(const Scenario&, const DeviceModel&) noexcept`; `Truth evaluate_condition(const Scenario&, uint8_t node_index, const DeviceModel&, const ClockState&) noexcept`; `bool matches_trigger(const Scenario&, const DeviceEvent&) noexcept`.
- Валидация проверяет graph без циклов и единственное дерево от root, типы/операторы, пределы 8/16/4/8 и существующие capabilities/channels. Rules со ссылкой на удалённое устройство сохраняются как неисполнимые при runtime reload; API создание с отсутствующей ссылкой отклоняется.

- [ ] **Step 1: Написать failing tests**
  Truth assertions: AND(True,Unknown)=Unknown; AND(False,Unknown)=False; OR(True,Unknown)=True; OR(False,Unknown)=Unknown. Nested `All(Any(door_open,motion),time)` проверяется при двух состояниях и UTC+7. `time_window_crosses_midnight`: 23:00–02:00 включает 01:00 и исключает 12:00. `unknown_clock`: TimeWindow→Unknown. Пределы: 16 nodes/depth4 принимаются, 17/depth5 отклоняются; empty children/cycle/no triggers/invalid channel/type mismatch отклоняются.
- [ ] **Step 2: Подтвердить RED**
  `cmake --build build-scenarios && ctest --test-dir build-scenarios -R 'test_condition_evaluator|test_scenario_validation' --output-on-failure` → FAIL на новых проверках.
- [ ] **Step 3: Реализовать evaluator/validator**
  Только типизированные predicates eq/ne и числовые lt/le/gt/ge. Границы TimeWindow: start включительно, end исключительно; start=end означает весь день. Offset задаётся явно; clock unknown не вычисляется через случайный epoch.
- [ ] **Step 4: Проверить GREEN**
  Повторить команду и test_device_model; все PASS. Пустые группы не превращаются в безусловное правило.
- [ ] **Step 5: Commit**
  `feat: validate and evaluate bounded AND OR conditions`.

### Task 3: Движок сценариев, команды и монотонные таймеры

**Files:**
- Create: `components/scenario_engine/include/scenario_engine.hpp`, `components/scenario_engine/scenario_engine.cpp`.
- Create: `components/scenario_engine/include/scenario_log.hpp`, `components/scenario_engine/scenario_log.cpp`.
- Create: `components/scenario_engine/include/monotonic_clock.hpp`, `components/scenario_engine/monotonic_clock.cpp`.
- Create: `test/scenarios/test_scenario_engine.cpp`, `test/scenarios/test_scenario_conflicts.cpp`, `test/scenarios/test_monotonic_clock.cpp`.

**Interfaces:**
- Consumes: DeviceModel, evaluator, Scenario и ChannelKey.
- Produces: `ValidationResult ScenarioEngine::upsert(const Scenario&) noexcept`; `void ScenarioEngine::on_event(const DeviceEvent&, const ClockState&) noexcept`; `ActionBatch ScenarioEngine::tick(const ClockState&) noexcept`; `void ScenarioEngine::on_command_result(OperationId, CommandStatus, uint64_t) noexcept`; `void ScenarioEngine::manual_override(ChannelKey) noexcept`; `void ScenarioEngine::remove(ScenarioId) noexcept`; `void ScenarioEngine::disable(ScenarioId) noexcept`; `void ScenarioEngine::reset_pending() noexcept`.
- `uint64_t MonotonicClock::extend(uint32_t uptime_ms) noexcept` используется только при источнике 32-bit; target предпочтительно берёт esp_timer_get_time/1000 напрямую в uint64_t.
- ActionBatch выдаёт задания один раз. Сервис переводит queued→pending при принятии адаптером, Confirmed приходит отдельным входом. По каналу не более одной физически активной команды; новые задания сериализуются. Отмена логической последовательности не отзывает уже отправленную физическую команду, но её поздний result не возобновляет последовательность.

- [ ] **Step 1: Написать failing tests последовательностей**
  `motion_on_off`: t0→ON, t59000→нет OFF, t60000→OFF. `retrigger_restarts_delay`: после clear→detected на t30000 OFF только t90000. `duplicate_report_does_not_retrigger`: DeviceModel не генерирует событие, deadline прежний. `manual_override_cancels_delayed_off`: ручное ON после t30000 исключает запланированный OFF. `late_result_after_cancel`: result старого operation не создаёт новых действий. `failed_command_stops_sequence`: после FAILED нет дальнейших действий.
- [ ] **Step 2: Добавить failing tests границ/порядка**
  `same_batch_order`: события sequence1/2 и правила сортируются по rule_id; результаты воспроизводимы. `channel_cancel_is_scoped`: отмена channel2 не отменяет1/3. `pending_capacity_64`: overflow не оставляет частично запланированные действия. `uptime_wrap`: 0xfffffff0→0x10 даёт +32 мс. `large_clock_jump`: однократное исполнение due-действия, без повтора каждого пропущенного tick. Rule edit/delete/disable отменяет generation; 24-е правило принимается,25-е нет.
- [ ] **Step 3: Подтвердить RED**
  `cmake --build build-scenarios && ctest --test-dir build-scenarios -R 'test_scenario_engine|test_scenario_conflicts|test_monotonic_clock' --output-on-failure` → FAIL.
- [ ] **Step 4: Реализовать интерфейсы и журнал**
  Новый запуск предварительно проверяет вместимость целой sequence. Задания имеют generation token; pending команду связываем с operation_id. Timeout команды: 5000 мс, без retry. Логи содержат возрастающий sequence, источник, device/channel/rule, результат и причину отказа. Переполнение журнала удаляет старейшую запись, не переполняет память.
- [ ] **Step 5: Проверить и commit**
  Полный `ctest --test-dir build-scenarios --output-on-failure` → PASS. Commit: `feat: execute local scenarios with cancellable timers`.

### Task 4: Общий JSON codec и устойчивое сохранение

**Files:**
- Create: `components/scenario_engine/include/scenario_codec.hpp`, `components/scenario_engine/scenario_codec.cpp`.
- Create: `components/scenario_engine/include/scenario_store.hpp`, `components/scenario_engine/scenario_store.cpp`.
- Create: `components/scenario_engine/vendor/cjson/cJSON.c`, `cJSON.h`, `LICENSE`, `SOURCE.md`.
- Create: `test/scenarios/test_scenario_codec.cpp`, `test/scenarios/test_scenario_store.cpp`.

**Interfaces:**
- Consumes: Task1 types, Task2 validation, ScenarioEngine.
- Produces: `ValidationResult decode_scenario(std::string_view, Scenario&) noexcept`; `ValidationResult encode_scenario(const Scenario&, char*, size_t, size_t&) noexcept`.
- `StoreBackend` предоставляет read/write_slot(index,bytes) и read/write_active_generation; `ValidationResult ScenarioStore::save(const ScenarioSet&) noexcept`, `ValidationResult ScenarioStore::load(ScenarioSet&) noexcept`. ScenarioSet до24 записей и суммарно64КиБ.
- Codec использует cJSON v1.7.19 из официального commit `c859b25da02955fef659d658b8f324b5cde87be3`. Проверено через GitHub tags API. Один codec для host/target; на target не линковать второй экземпляр cJSON: компонент использует SDK json, host — vendored cJSON с теми же API.
- Слот: schema_version=1, generation, length, CRC32, payload. Активная generation задаётся двумя чередуемыми commit records с собственными CRC. При повреждённой записи выбирается последняя целая commit record, ссылающаяся на целый слот; наличие более нового payload само по себе не подтверждает запись.

- [ ] **Step 1: Написать failing tests codec**
  roundtrip Nested AND/OR и delay60000; invalid bool/string IDs, negative/NaN/noninteger ID, duplicate keys, trailing JSON мусор, depth5, payload8193 bytes, invalid UTF-8 name отклоняются. Поле имени с `<img onerror=...>` остаётся строкой. Unknown fields отклоняются, неизвестные значения enum не преобразуются в ноль.
- [ ] **Step 2: Написать failing tests save/load**
  SAVE A; fault при записи B на каждом шаге→LOAD A. После успешного B→LOAD B. Corrupt B/active pointer проверяется по CRC/commit marker. Пустое хранилище→пустой ScenarioSet, без автодействий. SAVE без изменения не увеличивает число Flash writes. Один scenario8192 bytes и весьset65536 принимаются при допустимом JSON; превышение любого лимита отклонено до backend.write.
- [ ] **Step 3: Подтвердить RED**
  `cmake --build build-scenarios && ctest --test-dir build-scenarios -R 'test_scenario_codec|test_scenario_store' --output-on-failure` → FAIL.
- [ ] **Step 4: Реализовать codec/store**
  Parsing только cold/config paths, не на каждом tick; ни одного partial upsert. Новая commit record публикуется после полной проверки нового слота; предыдущая commit record остаётся валидной. Target backend: отдельный NVS partition для двух слотов размером не меньше256КиБ; при разметке задачи8 выделяется из SPIFFS-резерва без overlap и изменения Zigbee partitions.
- [ ] **Step 5: Проверить и commit**
  Полный scenario suite→PASS; fault-injection показывает сохранение последней версии. Commit: `feat: persist validated scenarios with recoverable slots`.

### Task 5: Host-симулятор и HTTP API

**Files:**
- Create: `host/CMakeLists.txt`, `host/simulator_main.cpp`, `host/simulator_runtime.cpp`, `host/include/simulator_runtime.hpp`.
- Create: `host/server.py`, `host/file_store.py`, `host/fixtures/devices.json`.
- Create: `test/host_sim/test_api.py`, `test/host_sim/test_restart.py`, `test/host_sim/test_timer.py`.

**Interfaces:**
- Consumes: тот же C++ Engine/DeviceModel/Codec/Store из задач1–4.
- Produces: persistent executable `build-simulator/zigbee_hub_sim`, JSON-lines request/response protocol, HTTP `/api/v1/*` из spec.
- `SimulatorRuntime::handle_request(std::string_view, char*, size_t, size_t&) noexcept` сериализует ответ; все mutations проходят один worker. Доступ к виртуальному времени делается command advance_ms; реальный режим использует monotonic heartbeat. Node/JS не содержит второй версии движка.
- server.py: Python stdlib HTTP, один долгоживущий C++ процесс, JSON-lines bridge с сериализацией запросов и timeout; не создавать process на каждый запрос. Bind по умолчанию127.0.0.1:8080; HTTP request body<=8192 bytes. Необходимый C++ response buffer отдельный64КиБ.
- Для симулятора дополнительные POST `/api/v1/sim/report`, `/sim/advance`, `/sim/restart`; не регистрировать их в target. Persist путь передаётся аргументом `--data-dir`, а restart воспроизводит загрузку config/UNKNOWN/no pending.

- [ ] **Step 1: Написать failing API tests**
  CREATE nested scenario→201; GET→тот же rule; contact/motion event→operation/channel и journal; invalid payload→400, missing device/channel→422, storage capacity→409, oversized body→413. Ручная команда→202 с operation_id. Имена/привязки сохраняются. Unknown route→404. Server error/child process crash возвращает503, не false success.
- [ ] **Step 2: Добавить failing end-to-end tests**
  Временный data-dir: rule motion→ON/delay60000/OFF, advance59000 и1000; restart сохраняет правило, показывает UNKNOWN, pending отсутствуют; первый report не запускает правило. Параллельные HTTP reports имеют последовательные sequence. Две разные data-dir не делят сценарии.
- [ ] **Step 3: Подтвердить RED**
  `python -m unittest discover -s test/host_sim -v` → FAIL до реализации. Tests запускают child/server на свободном локальном порту и завершают их после себя.
- [ ] **Step 4: Реализовать host runtime/API**
  Fixtures: виртуальные door/motion/relay3 с валидными стабильными ID. Первичная инициализация fixtures без событий. Join в симуляторе добавляет виртуальный экземпляр только внутри открытого окна. Use file backend с atomic replace/flush и fault-injection seam. Правила реально работают в C++ процессе при закрытом браузере.
- [ ] **Step 5: Проверить и commit**
  `cmake -S host -B build-simulator && cmake --build build-simulator`; C++ suite и unittest suite→PASS. Commit: `feat: expose portable engine through host simulator API`.

### Task 6: Русский веб-интерфейс устройств и редактор сценариев

**Files:**
- Modify: `components/web_ui/assets/index.html`, `style.css`, `app.js`.
- Create: `components/web_ui/assets/api.js`, `scenario_editor.js`, `devices_view.js`, `journal_view.js`, `simulator_panel.js`.
- Modify: `components/web_ui/CMakeLists.txt` и static handler asset registration для новых ресурсов.
- Create: `test/ui/scenario_editor.test.mjs`, `test/ui/ui_smoke.py`; если доступна браузерная тестовая среда, сценарии в `test/ui/browser_smoke.mjs`.

**Interfaces:**
- Consumes: versioned API задачи5; Scene JSON задачи4.
- Produces: `ScenarioEditor.mount(root, {devices, scenario, onSave})`; `ScenarioEditor.serialize() -> ScenarioJSON`; `Api.request(path, options) -> Promise<JSON>`.
- editor привязан к конкретным capabilities/channel IDs, а не display names. Использует DOM textContent. Сохранение только после успешного ответа; failed save не рисует успех и не теряет draft.

- [ ] **Step 1: Написать failing editor tests**
  Create All(dooropen, motion), Any(dooropen,motion), nested All(Any(...),time); JSON совпадает с codec. Удалённый канал помечен ошибкой; пустая группа/нет trigger/depth5 не сохраняются. `<img onerror>` в имени остаётся text node. Двойной click Save даёт один запрос и один rule. Failed API save сохраняет draft.
- [ ] **Step 2: Подтвердить RED**
  `node --test test/ui/scenario_editor.test.mjs` → FAIL на отсутствующих функциях/проверках.
- [ ] **Step 3: Реализовать интерфейс**
  Вкладки Устройства/Сценарии/Журнал/Настройки; отдельные toggles каналов1–3 со статусами команд; редактор triggers OR и вложенных условий All/Any; delays. Условия событий и состояний подписываются явно. При UNKNOWN нет ложного OFF. Simulator panel/banner рендерятся только при system.mode=simulator.
- [ ] **Step 4: Проверить пользовательский путь**
  Запустить server, открыть viewport390x844 и1366x768. Через UI создать motion→ON/60s/OFF, сгенерировать переход в симуляторе, ускорить время и увидеть канал/журнал; закрыть страницу и подтвердить действие через API; открыть снова. Необходимые ресурсы обслуживаются локально, horizontal overflow нет. Сохранить screenshots как evidence только после фактической проверки.
- [ ] **Step 5: Commit**
  `feat: add Russian device dashboard and AND OR scenario editor`.

### Task 7: Интеграция каналов и движка в upstream Zigbee runtime

**Files:**
- Modify: `components/core/include/core_commands.hpp`, `core_events.hpp`, `core_effects.hpp`, `core_state.hpp`, `core_command_dispatcher.cpp`, `core_reducer.cpp`.
- Modify: `components/service/include/application_requests.hpp`, `service_runtime.hpp`, `service_runtime_api.hpp`; `application_command_mapper.cpp`, `command_manager.cpp`, `service_runtime.cpp`, `hal_event_adapter.cpp`.
- Modify: `components/app_hal/include/hal_zigbee.h`, `components/app_hal/hal_zigbee.c`.
- Modify: Tuya plugin types/registry and snapshot builders; `components/service/config_manager.cpp` для retry=0 в prototype profile.
- Create: `components/service/include/scenario_manager.hpp`, `scenario_manager.cpp`, `scenario_nvs_backend.cpp`, `channel_locator.cpp`, `include/channel_locator.hpp`.
- Create: `components/web_ui/web_handlers_scenarios.cpp`, `web_handlers_v1.cpp`; modify web_routes registration.
- Create: `test/host/test_channel_command.cpp`, `test/host/test_scenario_runtime.cpp`, `test/integration/test_v1_handlers.cpp`; modify CMake tests.

**Interfaces:**
- Consumes: portable engine и тот же codec/API из задач1–6.
- Produces: `hal_zigbee_send_on_off_endpoint(uint32_t correlation_id, uint16_t short_addr, uint8_t endpoint, bool on)`; старый hal_zigbee_send_on_off сохраняет поведение wrapper.
- `ValidationResult ChannelLocator::resolve(ChannelKey, ChannelRoute&) const noexcept` возвращает current short_addr, endpoint и route kind StandardOnOff/TuyaDp с проверенной dp_map.
- `ScenarioManager::on_device_report(const DeviceReport&, const ClockState&)`, `ScenarioManager::tick(const ClockState&)`, `ScenarioManager::manual_command(ChannelKey,bool)`; работа внутри сервисной очереди, без прямых вызовов из Zigbee callback.
- TargetNvsBackend реализует StoreBackend задачи4. Operation confirmed требует обратного состояния устройства, а простой APS ack означает pending/accepted; при отсутствии отчёта истекает timeout.

- [ ] **Step 1: Написать failing channel/runtime tests**
  Каналы1/2/3 используют разные endpoints/verified DP без влияния на соседей; если DP карта неизвестна, unsupported возвращается до отправки. DeviceId rejoin с новым short_addr не ломает сценарий. Старый single-gang API работает как раньше. Capture ACK без attribute report не даёт confirmed. Source report endpoint2 обновляет толькоchannel2. Scenario failure не запускает delayed action. Retry config=0: timeout не отправляет второй Zigbee packet.
- [ ] **Step 2: Добавить failing integration tests API**
  POST scenario/channel commands проходит через service queue; web handler не изменяет core напрямую. Target system.mode=target не имеет sim routes/banner. Неполная/ошибочная команда не создаёт pending operation. NVS reload даёт правила без событий и pending.
- [ ] **Step 3: Подтвердить RED и реализовать seams**
  `cmake -S test/host -B build-host && cmake --build build-host`; новые ctest FAIL до реализации. Затем добавить channel_id/endpoint по всей цепочке commands→events→effects→HAL→reports, portable engine feed и target storage. Изменения upstream ограничиваются интеграцией новых функций.
- [ ] **Step 4: Проверить GREEN и regression**
  Host/integration CTest suites и `bash check_arch_invariants.sh`→PASS. Исходные tests, зависящие от single power_on, адаптируются с сохранением single-gang behavior; не удалять assertions ради зелёного результата.
- [ ] **Step 5: Commit**
  `feat: integrate scenarios and endpoint-aware Zigbee control`.

### Task 8: Target-сборка, упаковка и аппаратный handoff

**Files:**
- Modify: `sdkconfig.defaults.esp32c6`, `partitions.csv`, `main/app_main.cpp`, `main/Kconfig.projbuild` для prototype profile.
- Create: `scripts/build_prototype.sh`, `scripts/run_simulator.sh`, `README_RU.md`, `docs/hardware-first-run.md`.
- Create: `test/scenarios/test_partition_layout.py`, `docs/verification.md`.

**Interfaces:**
- Consumes: runtime/UI всех предыдущих задач.
- Produces: запускаемый host package и firmware build artifacts с manifest versions/checksums. В manifest статус hardware_verified=false до фактических проверок.
- Startup AP только для первоначальной настройки, затем STA; Zigbee запускается после завершения config flow. Debug simulation endpoints не попадают в target. Timezone default420; time filter UNKNOWN до sync. SSID/password не находятся в исходниках/пакете/log output.

- [ ] **Step 1: Написать failing layout/config tests**
  partition offsets/sizes не пересекаются; scenario NVS>=256КиБ; ota_0/ota_1 и Zigbee storage сохранены; последний раздел<=размерFlash. Prototype retries=0, native coordinator включён, sim API отключён. При невозможности target сборки такой статус явно записан, а не подменён host PASS.
- [ ] **Step 2: Настроить воспроизводимую target-сборку**
  ESP-IDF v5.5.2, riscv32-esp-elf target; зависимости фиксируются lock. Профиль Flash16MB задаёт соответствующий sdkconfig, сохраняя проверенные partitions. `idf.py set-target esp32c6 && idf.py build` и build target HAL tests выполняются без USB.
- [ ] **Step 3: Проверить артефакты**
  `idf.py size`, `idf.py size-components`; binary помещается в OTA slot, с запасомRAM. В verification.md записать фактические команды/версии/результаты, не заявлять измерение heap/stack без платы.
- [ ] **Step 4: Подготовить запуск и пакет**
  README_RU: команды запуска host, UI URL, что является виртуальным, как прошить после получения оборудования. hardware-first-run: board/Flash detection, pairing, model/manufacturer/endpoints, стандартные команды против Tuya DP, проверка трёх каналов с подходящей лампой, сохранение сети/правил, coex stress. Изменённый source zip включает LICENSE, provenance, build instructions, tests; binaries добавляются только после успешной target сборки. Сохранить deliverables и дать пользователю ссылки.
- [ ] **Step 5: Финальная проверка и commit**
  Все applicable host/UI/integration/architecture checks и target buildPASS. Выполнить review branch по execution skill. Commit: `build: package C6 prototype and simulator for hardware bring-up`.

## Self-review плана

- Spec sections1–3: baseline/модель/task1 и target/task7. Sections4–5: validator/evaluator/task2 и scheduling/task3. Section6: limits/store/log/tasks1/3/4. Section7: UI/task6, networking/time/tasks2/8. Section8: codec/API/tasks4/5/7. Sections9–10: meaningful tests/tasks1–8 и hardware handoff/task8. Sections11–12: границы не расширены.
- Five Review Focus checks связаны с конкретными failing tests в задачах3/4/6/7.
- All consumers используют общие типы и signatures; не допускается separate JS rule engine.
- Три канала не объявлены готовыми только потому, что есть DP parser. DP-карта заказанного выключателя остаётся аппаратной проверкой; unsupported не маскируется под успех.
- Target build, host tests и аппаратная проверка отчётливо разделены. Успешный workflow pages deployment не считается firmware CI.

## Execution handoff

Предпочтительный способ: реализовать в текущей сессии по порядку, сначала host-MVP/tasks1–6, затем target/tasks7–8. После review этого плана использовать superpowers:executing-plans. Это план, а не выполненная реализация; чекбоксы остаются пустыми до фактических действий и проверок.
