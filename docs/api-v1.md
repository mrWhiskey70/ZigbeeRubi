# Общий API v1

Все запросы обрабатывает HubRuntime. Host serializes requests к одному C++ процессу; target HTTP передаёт их в сервисную очередь. Максимум8192байт JSON тела; строгие типы, duplicate keys/invalidUTF8/unknown fields отклоняются. Device ID — EUI64 в16 lowercase hex, logical channel ID1..255, IDs сценариев uint32 ненулевые. API не имеет аутентификации: прототип предназначен для доверенной локальной сети.

| Method / Path | Тело / результат |
|---|---|
| GET `/api/v1/system` | mode simulator/target, hardware_verified=false, pending, time_known, offset_minutes |
| GET `/api/v1/devices` | devices: ID/name/available/capabilities/states/channels; UNKNOWN=null |
| PUT `/api/v1/devices/ID` | `{ "name": "Кухня" }` |
| DELETE `/api/v1/devices/ID` | убрать определение; target ставит Zigbee remove в очередь |
| GET `/api/v1/scenarios` | scenarios массив |
| POST `/api/v1/scenarios` | scenario;201 после сохранения |
| PUT `/api/v1/scenarios/N` | полностью изменённый scenario с id=N |
| DELETE `/api/v1/scenarios/N` | удалить после сохранения |
| POST `/api/v1/channels/power` | `{ "device_id":"…", "channel_id":2, "on":true }`;202 + operation_id |
| GET `/api/v1/operations/N` | pending/confirmed/failed/timeout; ACK не означает confirmed |
| GET `/api/v1/log` | entries, последние200 RAM |
| POST `/api/v1/network/join` | `{ "seconds":60 }`,1..120 |
| PUT `/api/v1/settings` | `{ "offset_minutes":420 }`,-720..840 |

Условия — bounded дерево в плоском preorder массиве: root0; `children` содержит индексы. Пустой массив условий = TRUE, пустая группа запрещена. Несколько triggers имеют OR; разные узлы all/any вложены до4уровней. State поддерживает eq/ne/lt/le/gt/ge и типизированные bool/integer; bool толькоeq/ne. Время — минуты от полуночи, включённое начало/исключённый конец, переход через полночь разрешён. Temperature измеряется в сотых°C, battery в процентах.

```json
{"id":1,"name":"Свет при движении","enabled":true,"triggers":[{"device_id":"0000000000000002","kind":"occupancy.detected"}],"conditions":[{"kind":"all","children":[1,2]},{"kind":"state","device_id":"0000000000000001","capability":"contact","op":"eq","value":true},{"kind":"time_window","start":1200,"end":420}],"actions":[{"kind":"set_channel_power","device_id":"0000000000000003","channel_id":1,"on":true},{"kind":"delay","delay_ms":60000},{"kind":"set_channel_power","device_id":"0000000000000003","channel_id":1,"on":false}]}
```

IDs примера виртуальные; на target используйте IDs списка devices. Коды:400 malformed/invalid,409 capacity,422 missing/unavailable/unsupported,503 storage/service error. Endpoint и DP не задаются клиентом команды: их берёт сервис из проверенной карты.

Только simulator: POST `/api/v1/sim/report`, `/sim/advance`, `/sim/restart`, `/sim/pair`. На target эти маршруты404. Wi-Fi provisioning использует старый service-backed POST `/api/network/connect` с ssid/password/save_credentials; пароль не возвращается API и не печатается журналом.
