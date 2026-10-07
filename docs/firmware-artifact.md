# Прошивка из GitHub Actions

В успешном запуске workflow ZigbeeRubi checks откройте Artifacts → esp32-c6-16mb-prototype. Архив содержит `build/` с четырьмя бинарными файлами и `flash_args`, а также `docs/build-manifest.json` с контрольными суммами. Это сборка ESP32-C6 / 16 МБ; hardware_verified=false.

Распакуйте архив, активируйте ESP-IDF v5.5.2 и из каталога `build/` выполните:

```sh
python -m esptool --chip esp32c6 -p PORT -b 460800 --before default_reset --after hard_reset write_flash @flash_args
```

Замените PORT на последовательный порт платы. Первый аппаратный запуск: [hardware-first-run.md](hardware-first-run.md). Доступность скачивания artifact зависит от входа в GitHub и срока хранения workflow. Сборка из исходников: [README_RU.md](../README_RU.md).
