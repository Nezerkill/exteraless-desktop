# Exteraless Desktop — карта проекта для агентов

`exteraless-desktop` — форк Telegram Desktop 7.3.1, в который переносятся
функции Android-клиента exteraless. Android-репозиторий лежит отдельно:
`/home/nezerkill/exteraless`.

## Структура

- `Telegram/` — upstream tdesktop; правки минимальные, точка входа `Telegram/SourceFiles`.
- `exteraless/` — наши модули без зависимости от UI, где возможно:
  - `config.*` — JSON-настройки `tdata/exteraless.json`;
  - `proxy_rules.*` — авто-отключение прокси по типу сети;
  - `ghost_mode.*` — фильтр исходящих MTProto-запросов;
  - `deleted_messages.*` — JSON-sidecar удалённых сообщений;
  - `backup.*` — формат `.extera`;
  - `network.*` — тип сети через Qt NetworkInformation + fallback по интерфейсам.
- `Telegram/SourceFiles/exteraless/pill_stack.*` — полоса Pill Stack в списке чатов.
- `Telegram/SourceFiles/settings/sections/settings_exteraless.cpp` — раздел настроек.
- `Telegram/SourceFiles/boxes/deleted_messages_box.cpp` — бокс удалённых сообщений.
- `Telegram/SourceFiles/mtproto/mtproto_request_filter.h` — интерфейс фильтра запросов.
- `Telegram/SourceFiles/window/window_main_menu.cpp` — пункты Exteraless в главном меню.
- Pill Stack выключен по умолчанию (`OEPillStackEnabled`), включается в
  настройках Exteraless или в главном меню.
- `nix/` — flake-пакет, NixOS-модуль и патч upstream.
- `STATUS.md` — что готово и что дальше.
- `CHANGELOG.md` — история изменений.

## Сборка

```bash
nix develop
cmake -S . -B out -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DTDESKTOP_API_ID=... -DTDESKTOP_API_HASH=...
cmake --build out --target Telegram -j3
```

На машине 16 ГБ RAM без свапа не поднимать параллелизм выше 3–4.
Debug-бинарник: `out/bin/Telegram`.

Nix-пакет:

```bash
nix build .#exteraless-desktop --cores 4 --max-jobs 1
./result/bin/Telegram
```

## Патч для Nix

Nix-пакет собирает upstream tdesktop и применяет
`nix/patches/exteraless.patch`. После правок исходников:

```bash
./nix/update-patch.sh
```

Скрипт делает diff от тега `v7.3.1`, поэтому патч всегда содержит все
изменения, а не только последний коммит. Новые файлы должны попадать в
`git add -N` (скрипт добавляет `exteraless` и `Telegram`).

## Правила

- Соблюдать `AGENTS.md` upstream tdesktop: Debug-сборка, стиль, append-only
  настройки.
- Не менять git-историю и не коммитить без явного запроса.
- Не расширять задачу: минимальные правки, поведение сохранять.
- После изменений компилировать затронутые цели и обновлять `CHANGELOG.md`.
