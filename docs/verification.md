# Проверки прототипа — 7 октября2026

`hardware_verified=false`. Успешная компиляция native C6 не является испытанием радио/датчиков на плате.

| Проверка | Команда | Результат |
|---|---|---|
| Upstream и endpoint-команды | `cmake -S test/host -B build-host; cmake --build build-host; ctest --test-dir build-host --output-on-failure` | 74/74 |
| HTTP/static/server интеграция upstream | `cmake -S test/integration -B build-integration; cmake --build build-integration; ctest --test-dir build-integration --output-on-failure` | 7/7 |
| Переносимый движок и target-mode общий API | `cmake -S host -B build-simulator; cmake --build build-simulator; ctest --test-dir build-simulator --output-on-failure` | 14/14 |
| HTTP API / сохранение / таймеры | `python3 -m unittest discover -s test/host_sim -v` | 12/12 |
| JS редактор и безопасное отображение | `npm ci; npm test` | 3/3 |
| Браузер / мобильный экран / закрытая страница | `python3 test/ui/ui_smoke.py` | 1/1 |
| Разделы Flash / профиль / статус оборудования | `python3 test/scenarios/test_partition_layout.py` | 3/3 |
| Архитектурные ограничения | `bash check_arch_invariants.sh` | PASS,0 нарушений |
| Прошивка продукта | `idf.py -B build-c6 -DIDF_TARGET=esp32c6 build` | PASS, native Zigbee coordinator |
| Компиляция target HAL/Unity harness | та же команда в `test/target` | PASS; тесты на плате не запускались |
| Размеры | `idf.py -B build-c6 size; idf.py -B build-c6 size-components; bash scripts/check_ota_slot_size.sh build-c6 partitions.csv` | PASS; численные результаты ниже и в evidence |

Браузер: Playwright1.56.0 / Chromium141,1366×768 и390×844. `pip install playwright==1.56.0; python3 -m playwright install chromium`. Скриншоты: [desktop](evidence/simulator-desktop.png), [mobile](evidence/simulator-mobile.png). Проверены: создание правила, ON по PIR, OFF через60с, независимая работа при закрытом браузере, отсутствие JS ошибок и горизонтального скролла.

SDK: ESP-IDF v5.5.2 (`30aaf64524299d3bde422ca9a2848090d1bc5d0f`), riscv32-esp-elf GCC14.2.0 / esp-14.2.0_20251107, Python3.12.14, CMake4.4.4, Ninja1.13.0. В этом окружении idf-component-manager2.5.2 не находил namespace PID; использован2.4.0 без правок SDK. Зависимости продукта: Zigbee1.6.8, ZBOSS1.6.4, mDNS1.9.1, cJSON1.7.19 (IDF json; host vendored), jsdom26.1.0.

В среде несколько созданных ELF-тестов получали mode644: перед запуском восстановлено разрешение исполнения только для `build-*/test_*`. Это не изменение проверяемых исходников. Все указанные suites затем выполнены целиком.

## Ограничения проверки

- Живые callbacks/radio, EUI64 byte order, discovery/reporting, NVS power-cut, AP→STA, coex, OTA, фактические heap/stack high-water требуют платы. Target service request queue компилируется в SDK; host тестирует общий HubRuntime и production core seam, а не FreeRTOS очередь.
- Заказанные Tuya/PIR/выключатель не идентифицированы. Новые правила не используют неподтверждённые DP. Стандартный контакт IAS и PIR Occupancy нормализуются; IAS PIR требует проверки zone type.
- Discovery каналов сейчас основан на стандартном OnOff attribute report; до отчёта управляемый канал отсутствует. Дескрипторная discovery всех endpoints и точные Tuya adapters — аппаратный этап.
- Журнал ограничен200 записями RAM, API возвращает кольцо целиком. Пагинация не добавлена. Время условийUNKNOWN до синхронизации; задержки работают независимо.
- Симулятор файлового хранилища проверен на Linux; Windows запуск рекомендован через WSL2.

Контрольные суммы реальных firmware artifacts и fingerprint исходников: [build-manifest.json](build-manifest.json). Признак `source_dirty` честно фиксирует сборку рабочей копии между коммитами; `source_content_sha256` охватывает исходники продукта, профиль и lock независимо от последующих изменений документации.

## Финальная сборка

Firmware: 1781136байт, OTA slot3145728байт, запас1364592байт. Static DIRAM210086байт (46.47%), остаток242026байт до динамических выделений. Shared HubRuntime на host67КиБ; service task24КиБ. Динамические radio/Wi-Fi allocations и пиковое потребление API не измерены без оборудования.

## Финальное ревью

Одно независимое ревью всего изменения выявило пять Important, без Critical/Minor. Все пять воспроизведены тестами до исправления и затем прошли: очередь ручного управления занятого канала, настоящий перезапуск процесса с сохранённым правилом, протухший датчик, первое сообщение после возврата, порядок Occupancy/Contact. Production event adapter извлечён в общий host-testable файл; тесты исполняют тот же адаптер, который вызывает target service. После исправлений все suites в таблице запущены целиком. Отложенных Minor нет; [решения и ограничения](rulings.md).

## Windows packaging

`test_file_store` native Windows RED: SDK-independent MSVC build passed; replacement metadata in a Unicode directory failed (13/14 CTest). `test_custom_assets_and_browser_receive_live_url` RED: server rejected missing --assets/--open. After implementation Linux14/14 CTest and12/12 HTTP passed; desktop entry exercised real C++/HTTP save, repeated replacement, UTF-8 names, timer and process restart. Windows workflow runs the same CTest and HTTP suites, then PyInstaller6.22.3 packaging and `test/windows/package_smoke.py` against the extracted ZIP. The POSIX BROWSER executable probe is skipped on Windows; actual package HTTP/assets/engine/persistence is checked there. Artifact is uploaded only after these checks succeed.
