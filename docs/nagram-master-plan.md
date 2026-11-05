# Master plan: перенос функций NagramX/exteraless в desktop

Дата: 2026-11-05.
Статус: активен. Это краткая точка входа; полные детали лежат в
`docs/nagram-feature-audit.md` и `docs/nagram-porting-backlog.md`.

## 1. Цель и границы

Цель — перенести **все функции Android-клиента exteraless/NagramX, у которых
есть осмысленный desktop-аналог**. Не являются целью Android-only вещи
(CameraX, FCM, Monet, нижние табы, XML-иконки лаунчера), Java/Xposed-хуки,
APK-обновления и plugin-only API. Полный список исключений — в бэклоге,
раздел «Android-only, plugin/Xposed и не переносить».

Правило: не копировать Android UI буквально. Переносить поведение, настройки
и дефолты в существующие Qt-компоненты tdesktop.

## 2. Текущая база

- Desktop: форк tdesktop 7.3.1, ветка `main`, remote
  `git@github.com:Nezerkill/exteraless-desktop.git`.
- Последние опорные коммиты: `cfbe72c` (аудит и бэклог), `123a29f`
  (P0 Ghost view/toggles), `27b1591` (paste stripping), `2434eb3`
  (unread capsule).
- Реализовано 18 групп: General, Ghost 4/5, deleted messages (частично),
  Chats (tail/reactions/tracking/unread capsule), Pill Stack 8 пилюль,
  drawer/accounts/order, plugins stage 1–2, proxy auto-disable, backup 25
  ключей, брендинг, WEB proxy.
- Проверки: chats **499**, plugin manifest **32**, plugin host **213**, все
  PASS; `nix flake check --no-build` PASS; Nix patch применяется к v7.3.1.
- `result/` устарел; полная Nix-сборка после последних изменений не
  завершена. Это отдельный P1-долг, не повод менять уже проверенный
  Debug-бинарник.

## 3. Источники истины

1. `docs/nagram-porting-backlog.md` — 90 задач `NB-01…NB-90`, P0/P1/P2.
2. `docs/nagram-feature-audit.md` — полная карта семейств, ключей, статусов и
   ограничений.
3. `docs/feature-mapping.md` — краткое описание уже перенесённого.
4. `STATUS.md` — состояние сборки и проверок.

При конфликте: код и фактические тесты важнее отчёта; аудит правится вместе
с бэклогом, а не отдельно.

## 4. Рабочий цикл одной задачи

1. Взять верхнюю открытую задачу из бэклога (P0 → P1 → P2).
2. Прочитать её строку и соответствующий раздел аудита; проверить файлы в
   текущем дереве.
3. Реализовать минимальное изменение. Не трогать `Telegram/lib_ui/**`: патч
   Nix собирается только по `CMakeLists.txt`, `Telegram/` и `exteraless/`.
4. Добавить/обновить чистый helper и CLI-тест, если поведение можно проверить
   без GUI; иначе Xvfb smoke.
5. Собрать затронутые TU, обновить `libexteraless.a`, выполнить LLD-релинк
   при известном GNU ld overflow (см. раздел 6).
6. Прогнать все три раннера, `nix flake check --no-build`, обновить патч и
   проверить его на чистом v7.3.1 (`./nix/update-patch.sh --check`).
7. Обновить `CHANGELOG.md`, `STATUS.md`, `Agents/README.md` и соответствующие
   строки бэклога/аудита.
8. Сделать отдельный commit и push в `origin/main`. Сообщение — по стилю
   репозитория, без `[ai]` и без трейлеров. В бэклоге отметить `NB-XX` как
   ✅/🟡 с датой и коммитом.
9. Перейти к следующей задаче без остановки, если нет compile-ожидания.

## 5. Ближайший batch

### Шаг 0 — данные и приватность

- `NB-02` — ✅ закрыто 2026-11-05: гейт `OESaveDeletedMessages`, лимит
  1000/100 и ротация sidecar, тумблер в Chats.
- `NB-04` — ✅ закрыто 2026-11-05: все 25 ключей `.extera` в трёх секциях,
  валидация импорта, отчёт unknown с префиксом секции.
- `NB-05` — ✅ закрыто 2026-11-05: `Config::reset()`, кнопка Reset с
  подтверждением, пересоздание секции, CLI-группа `reset`.
- `NB-06` — ✅ закрыто 2026-11-05: `Config::setWorkingDir()` от
  `cWorkingDir()` в Application, `<workdir>/tdata/exteraless.json`,
  копирование старого файла при первом запуске, CLI через
  `EXTERALESS_WORKDIR`.
- `NB-03` — ✅ закрыто 2026-11-05: 30 новых ключей
  `lng_settings_exteraless_*` (всего 41), все подписи секции переведены на
  `tr::`, пересобран `td_lang` и `lang_instance`.

### Шаг 1 — безопасный UX batch

- `NB-11` — ✅ закрыто 2026-11-05: подсекция «Visible message menu items»,
  11 чекбоксов `OEChatsMenu*`, поддержка в обоих виджетах истории
  (`history_inner_widget.cpp` и `history_view_context_menu.cpp`), CLI
  `menu` (8 проверок), Xvfb подтвердил скрытие `Pin` в чате.
- `NB-07` — ✅ закрыто 2026-11-05: `OEGeneralShowSeconds`,
  `Exteraless::FormatClock`, тумблер в General, CLI `general/clock`
  (17 проверок), Xvfb показал секунды в сообщениях и списке чатов.
- `NB-37` — ✅ закрыто 2026-11-05: `OEGeneralHidePhone`, скрытие в
  `info_profile_values.cpp`, тумблер в General, CLI `general`
  (22 проверки), Xvfb подтвердил скрытие номера в профиле.

### После batch

- `NB-12` — ✅ закрыто 2026-11-05: `OEChatsDoubleTapIn`/`OEChatsDoubleTapOut`
  (-1 = как в Telegram, 0 = ничего, 1/2 = реакция, 4 = ответ), селекторы в
  Chats, `exteraless/doubletap.{h,cpp}`, CLI `doubletap` (22 проверки),
  Xvfb подтвердил reply/reaction/nothing; остальные Android-действия — NB-91.

- `NB-14` — ✅ закрыто 2026-11-05: `OEChatsStickerTimeSide`/
  `OEChatsHideStickerTime`, селектор `Sticker time`, отрисовка и hit-test в
  `UnwrappedMedia`, CLI `stickertime` (14 проверок), Xvfb подтвердил боковое
  время и скрытие.

- `NB-16` — ✅ закрыто 2026-11-05: drag-порядок пилюль с сохранением в
  `OEPillStackOrder`, CLI `pillorder` (9 проверок), `.extera`, Xvfb
  подтвердил порядок после рестарта.

- `NB-17` — ✅ закрыто 2026-11-05: статус `On`/`Off`/`Partial` в пункте
  главного меню, `OEGhostShowStatus`, строгий `IsGhostActive`, CLI `ghost`
  (13 проверок), Xvfb подтвердил пакетный переключатель и статус.

- `NB-18` — ✅ закрыто 2026-11-05: пять `OEGhostSend*Locked`, подсекция
  «Locked Ghost settings», пакетный `SetGhostActive` пропускает locked, CLI
  `ghost-lock` (19 ghost-проверок), Xvfb подтвердил сохранение read при
  Ghost On.

- `NB-19` — ✅ закрыто 2026-11-05: схема удалённых v2 (17 полей), Qt-only
  сериализация `exteraless/deleted_entry.{h,cpp}`, CLI `deleted-entry`
  (7 проверок), Xvfb подтвердил реакцию и reply в боксе; исправлен
  use-after-free в пункте меню «Deleted messages».

- `NB-20` — ⚠️ split-required 2026-11-05: 4 настройки метки, значок,
  прозрачность и цвет в боксе «Deleted messages», CLI `deletedmark`
  (10 проверок), Xvfb; inline-показ в чате выделен в NB-92.

Дальше — NB-21 (импорт базы AyuGram).
`NB-13` (wide posts), `NB-15` (sort unread), feed, AI, player, Vosk и
regex-фильтры — отдельными архитектурными этапами после стабилизации P0/P1.

## 6. Сборка и проверка

```bash
# конфигурация
nix develop --command cmake -S . -B out

# точечная компиляция TU через out/compile_commands.json
nix develop --command python3 /tmp/opencode/compile_tus.py '<substring>.cpp'

# обновление статической библиотеки, если менялся exteraless/*
ar rcs out/exteraless/libexteraless.a out/exteraless/CMakeFiles/exteraless.dir/*.o

# финальный линк (GNU ld падает на debug-info overflow)
/tmp/opencode/relink-lld-run.sh
cp -p /tmp/opencode/Telegram-lld-test out/bin/Telegram

python3 exteraless/tests/chats/run_checks.py
python3 exteraless/tests/plugins/run_checks.py
python3 exteraless/tests/plugins/run_host_checks.py
nix flake check --no-build
```

Перед коммитом обновить `nix/patches/exteraless.patch` через
`./nix/update-patch.sh --check`: база закреплена в `nix/upstream-base`
(коммит + tree-хеш, upstream `6fed91f`), скрипт применяет патч к чистому
worktree и печатает sha256.
Полную Nix-сборку выполнять только когда дерево стабильно; она долгая и
может конкурировать за память с Debug-сборкой.

## 7. Ограничения и принятые решения

- `exteraless.json` лежит в `<workdir>/tdata` и следует `-workdir`; при
  первом запуске с другим workdir старый файл копируется, если нового ещё
  нет. CLI-тесты используют `EXTERALESS_WORKDIR` для изоляции.
- Ghost: 4/4 функции; мёртвый `OEGhostSendOfflineAfterOnline` удалён в NB-38.
  Allowance глобальный на 5 секунд, не привязан к peer (NB-43).
- Deleted messages: sidecar v2 с 17 полями (реакции, reply, forward,
  entities, дата правки, флаг прочтения); гейт, лимит и ротация закрыты в
  NB-02, схема — в NB-19, метка в боксе — NB-20, inline-показ — NB-92.
- Плагины: bwrap + Python host, разрешения заявленные и не применяются как
  capability; stage 3 (меню/настройки/intents) — NB-79.
- WEB proxy уже есть в коде и Nix-сборке, но не описан в feature-mapping и не
  имеет развёрнутого relay; задача NB-90.
- `.extera` содержит все 25 desktop-ключей; плагины экспортируются только как
  enable-флаги, без файлов.
- Android-only и `lib_ui`-зависимые пункты не добавлять в код без отдельного
  решения по упаковке.

## 8. Точка возобновления после сжатия контекста

1. Прочитать этот файл.
2. Открыть `docs/nagram-porting-backlog.md`, взять первую открытую `NB-*`
   (сейчас `NB-21`; NB-92 — отдельный L-этап).
3. Проверить `git status`, `git log -3`, текущие счётчики тестов.
4. Выполнить задачу по циклу из раздела 4, закоммитить и запушить отдельным
   коммитом.
5. Обновить статус задачи в бэклоге в том же коммите.
