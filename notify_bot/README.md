# Notify Bot

Десктопная панель публикаций для одного бота Telegram. Отправка выполняется только через официальный Bot API по HTTPS/WinHTTP и только получателям с записью `chat_id`, `consent_ts`, `source` в `state/subscribers.jsonl`. Проект не входит в пользовательские аккаунты Telegram и не создаёт службы, Run-ключи, задания или автозагрузку.

## Сборка и запуск

Из корня проекта в Developer PowerShell x64:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
build\Release\notify_bot.exe
```

Готовые сборочные файлы находятся в `build\Release\`; копии для выдачи — в `dist\`. Перед запуском рядом с приложением/в корне проекта требуется `config\build_config.json`.

## Подключение Bot API и пробная отправка

`config\token.txt` содержит единственный токен бота и хранится локально. `api_base` пока `unset`; до задания этого поля программа не делает сетевые вызовы. `get-me` выполняет реальный запрос только когда заданы `api_base` и токен. `send-test` без `--live` всегда работает как dry-run; `--live` дополнительно требует выбранный `chat_id` с актуальным согласием в `state\subscribers.jsonl`.

```powershell
build\Release\notify_cli.exe get-me --config config\build_config.json
build\Release\notify_cli.exe send-test --chat-id <chat_id> --config config\build_config.json
build\Release\notify_cli.exe send-test --chat-id <chat_id> --live --config config\build_config.json
```

`<chat_id>` и `api_base` должны быть предоставлены оператором. Приложение не подставляет тестовые значения и не заявляет об успешном сетевом соединении без полученного ответа API. Каждый запрос перед WinHTTP проходит `ProxyPreflight`; проверка сертификата остаётся включённой. Без `probe_host` поле `egress_ip` остаётся `null`, `egress_verified` — `false`.

Подписчики добавляются по одному JSON-объекту на строку. Пример структуры, не являющийся реальным согласием:

```json
{"chat_id":"<chat_id>","consent_ts":"<timestamp подтверждения>","source":"<источник подтверждения>"}
```

У записей без `consent_ts` или `source` рассылки нет. Список можно обновить в приложении; поддержан множественный выбор. Пауза между чатами настраивается в интерфейсе, очередь при достижении порога неудач переходит в `PAUSED_RETRY` и требует ручного `resume()`.

## Текст, изображения и очередь

Поддержаны стили `plain`, `markdown_v2`, `html`, упоминания `@name` и ссылки `tg://user?id=`. В `plain` текст безопасно экранируется как HTML, чтобы сохранить символы и сделать обнаруженные упоминания кликабельными; `html` и `markdown_v2` принимают соответствующую разметку. Диапазоны явных `Entity` блокируются, пока `entity_unit` остаётся `unset`. `custom_emoji_document_id` также `unset`, поэтому пользовательские прем-эмодзи не заявляются как работающие.

Изображения отправляются через `sendPhoto` в `multipart/form-data`; сигнатуры JPEG, PNG и WebP проверяются после полного чтения файла. `max_photo_bytes` — внутренний лимит приложения, не утверждение о лимите Bot API. Очередь поддерживает ограничение частоты на чат и общий дроссель, повторы с экспоненциальной задержкой и jitter, `retry_after` из JSON-ответа и `state\failures.jsonl` для отказов. События порога пишутся одним `_flush()` в `state\admin_events.jsonl` при завершении обоих режимов.

## Лицензия, статусы и ограничения

Ключи выпускаются неподвязанными и привязываются к HWID при первой активации. Команды администратора:

```powershell
build\Release\notify_cli.exe new-secret --config config\build_config.json
build\Release\notify_cli.exe issue --plan premium --days 30 --config config\build_config.json
build\Release\notify_cli.exe verify "<token>" --config config\build_config.json
build\Release\notify_cli.exe revoke "<token>" --config config\build_config.json
build\Release\notify_cli.exe login-screen --config config\build_config.json
```

Экран сохраняет макет Telegram-подключения с пятью карточками `CARD 01`–`CARD 05`. Поля номера и кода являются только неактивной графической разметкой: `phone_input_state`, `code_input_state`, `connect_action`, `session_provider` имеют `state = unset`, `value = null`; рядом отображаются `SESSION PROVIDER: unset`, `CONNECT ACTION: DISABLED`, `NO NETWORK`. Ввод не читается, сетевых вызовов нет, сессии не создаются.

Статус Premium — только read-поле собственного аккаунта. Источник для Bot API не подтверждён: `users.getFullUser` / `GetFullUser` относятся к MTProto и не доказывают наличие поля в Bot API. Для замера укажите конкретный билд MTProto-клиента и xref на `users.getFullUser` / `GetFullUser`; сохраните дамп ответа с Premium и без него, сравните `premium` и `EmojiStatus`, подтвердите точку сравнения через breakpoint с изменённым входом либо приложите реально полученный задокументированный ответ Bot API на названной версии. Только после проверки запишите `state = measured` и `source_build`. Для `custom_emoji_document_id` подтвердите отправку `MessageEntityCustomEmoji` на аккаунте с Premium. Неиспроверенные значения: `old/unverified, must be re-measured`.

| Пункт | Статус |
|---|---|
| Bot API, один токен, HTTPS/WinHTTP | Написано; живое подключение зависит от `api_base` и `config\token.txt` |
| Получатели и явное согласие | Написано; источник `state\subscribers.jsonl` |
| `plain` / `markdown_v2` / `html`, упоминания | Написано; точные диапазоны `Entity` заблокированы при `entity_unit = unset` |
| Пост + JPEG/PNG/WebP | Написано; интеграционная отправка не подтверждена без учётных данных |
| Паузы, общий дроссель, повторы и отказы | Написано; проверяется локальными тестами |
| `failure_threshold = 5`, `PAUSED_RETRY`, ручной `resume()` | `derived`; допустим диапазон 3..7 |
| `RunPooled()` и `RunOverlapped()` | Написано; сетевой режим gated конфигурацией |
| HWID и `issue / verify / revoke` | Написано на CNG; ключ привязывается при первой активации |
| GUI и пять реальных панелей состояния | Написано; требует визуального запуска для проверки на целевом экране |
| `ProxyPreflight`, проверка TLS | Написано; фактический egress не измерен |
| Premium и прем-эмодзи | Не подтверждено; соответствующие поля `unset` |
| `toolchain_version`, `cmake_generator`, `sdk_version` | Записываются только по реальному логу конфигурации/сборки |
| Внешние пакеты | Нет |

## Удаление

Сначала выполнить `notify_cli.exe revoke` для каждого выданного ключа; затем удалить `config\token.txt`, `config\license_private_key.bin`, `config\license_public_key.bin`, `config\license_activation.json`, `config\license_secret.bin`, `config\hwid.txt`, каталоги `state\`, `build\`, `dist\` и всю папку проекта.

