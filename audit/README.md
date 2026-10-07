# Воспроизводимый аудит upstream

Источник: [Allekslar/zigbee-gateway_v1](https://github.com/Allekslar/zigbee-gateway_v1).
Commit: `1838e5b35bfaf3cc8b328ddc044b997019533881`.
Автор upstream: Allekslar и участники исходного проекта. Лицензия: AGPL-3.0-only; оригинальный текст в `upstream/LICENSE`.

`upstream/` содержит 17 оригинальных файлов: четыре тестовые программы, четыре реализации Tuya и девять необходимых заголовков. Содержимое скопировано без продуктовых изменений. Это минимальная копия для аудита, а не полная прошивка. Новая логика ZigbeeRubi будет находиться в основных каталогах проекта после импорта baseline.

Файлы и лицензия сверены с GitHub blob SHA исходного commit; SHA-256 также сохранены в [source-manifest.json](source-manifest.json).

## Проверка

Из корня репозитория:

```sh
python3 tools/run_audit_tests.py
```

Команда для каждого теста использует `g++ -std=c++17 -Wall -Wextra -Werror`, каталоги заголовков service/core и соответствующие реализации. Результаты компиляции и запуска сохраняются в `build/audit/results.json`; ошибка любого теста даёт ненулевой exit code.

Сохранённый результат проверки переноса: [results/tuya-host.json](results/tuya-host.json). Проверены `test_tuya_dp_parser`, `test_tuya_contact_sensor_plugin`, `test_tuya_switch_plugin`, `test_tuya_fingerprint`.

## Основные выводы

- Contact-плагин сопоставляет `_TZ*` и `TS0203`. Упоминание TS0601 в комментарии не означает его поддержку.
- Switch-плагин рассчитан на `_TZ*` + TS0001/TS0011, DP1 и endpoint 1.
- В upstream core хранится одно состояние power_on; on/off HAL не принимает отдельный endpoint. Три независимых канала требуют изменений всей цепочки.
- Движок сценариев и редактор И/ИЛИ в изученном проекте не обнаружены.
- Для заказанных датчиков и выключателя fingerprint и карта каналов остаются неизвестными.

Полная target-сборка, радио, pairing, работа с нагрузкой и Wi-Fi/Zigbee coexistence этой проверкой не подтверждены. Полный аудит и критерии дальнейшей проверки описаны в техническом проекте.
