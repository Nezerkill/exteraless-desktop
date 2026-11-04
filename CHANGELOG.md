# Changelog

## 2026-11-05

- Chats / Context menu: добавлена подсекция «Visible message menu items»
  с 11 чекбоксами видимости пунктов меню сообщения (`Reply`, `Edit`,
  `Pin / Unpin`, `Copy text`, `Copy link`, `Copy image`, `Forward`, `Select`,
  `Translate`, `Report`, `Delete`). Все пункты поддержаны в обоих контекстных
  меню (`history_inner_widget` и `history_view_context_menu`), экспортируются
  в `.extera`, импортируются с поддержкой Android-алиасов (`showCopyPhotoButton`,
  `showReportButton`); горячие клавиши не затрагиваются.
- Localization: все подписи секции Exteraless (Ghost, Chats, Pill Stack,
  Reset, Plugins, диалоги экспорта/импорта бэкапа и подтверждения сброса)
  переведены на ключи `tr::lng_settings_exteraless_*`; в `lang.strings`
  добавлено 30 ключей (всего 41).
- Config: `exteraless.json` теперь лежит в `<workdir>/tdata` и следует
  `-workdir`; при первом запуске с другим workdir старый файл из app data
  копируется, если нового ещё нет.
- Exteraless settings: кнопка «Reset Exteraless settings» с подтверждением
  очищает все `OE*`-ключи, возвращает дефолты и переприменяет proxy rules;
  секция пересоздаётся, чтобы тумблеры сразу показывали новые значения.
- Backup `.extera`: экспорт/импорт всех 25 desktop-ключей в секциях
  `exteraconfig`, `pillstackconfig` и `mainconfig`; импорт валидирует типы и
  диапазоны, санитизирует Ghost-exclusions и plugin states, принимает старый
  плоский формат и репортит неизвестные ключи с префиксом секции.
- Chats/Deleted messages: `OESaveDeletedMessages` (default `false`) теперь
  гейтит запись удалённых сообщений, sidecar ограничен 1000 записями и 100
  на чат с ротацией при добавлении и загрузке; тумблер «Save deleted
  messages» добавлен в подсекцию Chats, CLI-тесты — `deleted`,
  `deleted-set`, `deleted-trim`.

## 2026-11-04

- Docs: добавлены `docs/nagram-feature-audit.md` (обзор функций
  exteraless/NagramX, покрытие desktop, Android-only, расхождения docs и
  кода) и `docs/nagram-porting-backlog.md` (приоритизированный бэклог
  P0/P1/P2 с точками интеграции и smoke-проверками).
- Ghost Mode: исправлена инверсия `messages.getMessagesViews` (просмотры
  каналов больше не блокируются при обычном режиме) и инвертированные
  подписи четырёх тумблеров «Don't send …» в настройках Exteraless.
- Chats: при скрытии реакций в канале/группе/личном чате скрывается также
  hover-кнопка добавления реакции у сообщения.
- Chats paste: `OEChatsStripTrackingOnPaste` (default `true`) очищает
  tracking-параметры в plain-text/URL вставке; rich-text не меняется, ключ
  desktop-only.
- General/Nagram X: `OEGeneralDisableNumberRounding` (default `false`)
  показывает точные значения вместо сокращённых `K/M/B`; Android backup-ключ
  `disableNumberRounding` поддержан, тумблер — в General.
- Chats: флаг `OEChatsUnreadCapsule` (default `false`) рисует ярлык
  непрочитанных сообщений компактной пилюлей по центру вместо полноширинной
  полосы; высота, отступы, раскладка и прокрутка не меняются, в бэкап флаг не
  экспортируется, тумблер «Compact unread messages label» — в подсекции Chats.

- Плагины, этап 1: каталог `tdata/plugins`, сканер манифестов `.plugin`/`.py`
  одним процессом `python3 -I -B` через `ast.parse`/`ast.literal_eval` без
  исполнения кода плагина, лимит файла 2 MiB, whitelist разрешений (`ui`,
  `messages.read`, `messages.send`, `network`, `files`, `intents`,
  `settings`, `hooks`, `native`), `.elyx`/`.eaf` показываются как unsupported.
- Настройки Exteraless: подсекция и кнопка «Plugins», бокс со списком
  плагинов, per-file ошибками и пустым состоянием; установки, включения и
  запуска плагинов нет, кнопки только «Open folder» и «Close».
- Тестовый harness плагинов: CLI `exteraless-plugins-scan`, фикстуры и
  regression runner (`exteraless/tests/plugins/run_checks.py`, 32 проверки).
- Плагины, этап 2: один host-процесс `bwrap` + `python3 -I -B -c` на всё
  приложение с JSON-lines IPC (`seq`/`ok`, `load`/`unload`), persistent
  `OEPluginStates` (default `false`), audit-лог `tdata/plugins-audit.log` и
  тумблеры в боксе Plugins. Включённые плагины загружаются только при
  открытии бокса; запуск приложения host не стартует, выход из приложения
  останавливает его. bwrap даёт read-only корень и `/plugins`, скрытый HOME,
  минимальный environment, без сети и сброшенные capabilities; session
  keyring плагинов отделяется best-effort через `KEYCTL_JOIN_SESSION_KEYRING`.
  Python audit hook остаётся policy layer, а не полной OS-изоляцией.
- Тестовый harness этапа 2: CLI `exteraless-plugin-host-cli`, фикстуры
  `fixtures/runtime/`, regression runner
  (`exteraless/tests/plugins/run_host_checks.py`, 213 проверок).
- Nix-пакет: `postFixup` сохраняет upstream-обработку D-Bus без дублирования:
  upstream `substituteInPlace` остаётся ровно один раз, добавлен только
  `wrapProgram`, который кладёт `bubblewrap` и `python3` в `PATH`. Прежний
  вариант повторял подмену и падал на `--replace-fail` при повторном
  совпадении; devShell содержит `bubblewrap` и `python3`.
- Документация: Known Limitations этапа 2 зафиксированы явно — audit hook
  best-effort и отключается плагином через `import __main__`
  (`_IN_AUDIT[0] = True`), потоки сейчас разрешены (`threading.Thread` в
  Python 3.14 идёт через `_thread.start_joinable_thread`, запись
  `threading.Thread` в deny-наборе мёртвая), один host на все плагины,
  bwrap только Linux, пакетный wrapper, полной Nix-сборки после этапа 2 ещё
  не было. Deny-by-default больше не описывается как security boundary.
- Drawer-аккаунты подтверждены на итоговом бинарнике: Debug-сборка и Xvfb
  smoke прошли, `nix flake check --no-build` проходит.
- Chats: перенесены `OEChatsRemoveMessageTail` (default `true`) и
  `OEChatsHideReactionsChannels/Groups/Private` (default `false`); значения
  читаются при построении чата и применяются после его повторного открытия,
  реакции в pinned bar могут обновиться только после перезапуска. Добавлен
  `OEChatsStripTrackingOnOpen` (default `true`): при клике по http/https
  удаляются `utm_*` и известные click-id (`fbclid`, `gclid`, `dclid`,
  `gbraid`, `wbraid`, `msclkid`, `yclid`, `twclid`, `ttclid`, `igshid`,
  `li_fat_id`, `mc_cid`, `mc_eid`, `_hsenc`, `_hsmi`, `mkt_tok`,
  `rb_clickid`, `s_cid`, `vero_id`, `wickedid`, `oly_anon_id`, `oly_enc_id`);
  `ref`/`source`/`s`/`si` и прочие неоднозначные параметры сохраняются,
  percent-encoded имена не декодируются, очистки вставленного текста нет.
  Android backup: `removeMessageTail`, `hideReactionsInChannels`,
  `hideReactionsInGroups`, `hideReactionsInPrivateChats`; strip tracking —
  desktop-only и в backup не входит.
- Тесты Chats: `exteraless/tests/chats/run_checks.py` — 79 проверок PASS
  (38 strip, 10 reactions, 2 CLI, 7 config, 22 backup); плагины без
  изменений — 32 и 213 проверок. Xvfb smoke: подсекция Chats, tail on/off,
  реакции скрыты в канале и группе; личного чата с реакциями в профиле не
  было, поэтому private проверен только CLI. Итоговый `out/bin/Telegram`
  sha256 `d88ddc7c…`, mtime 2026-11-04 17:19; `ninja -C out -n Telegram` без
  compile/link edges; `nix flake check --no-build` проходит; полная
  Nix-сборка не запускалась.

## 2026-11-03

- Добавлены `ACKNOWLEDGEMENTS.md` и раздел «Благодарности» в `README.md`:
  все девять проектов из списка exteraless, происхождение цепочки форков
  Android-клиента (включая Telegram-FOSS), дизайн-кредиты Android-клиента и
  примечание о сторонних компонентах.
- В `README.md` уточнено происхождение: Exteraless Desktop — форк Telegram
  Desktop, функции переносятся из Android-клиента exteraless; указана
  GPL-3.0, как у exteraless; дополнительно сохранено исключение OpenSSL
  из upstream Telegram Desktop (`LICENSE`, `LEGAL`).
- В `.gitignore` добавлены локальные артефакты: `HANDOFF.md`, smoke-прогоны,
  `exteraless.json`, логи и `DebugLogs/`.
- Drawer: порядок пунктов Exteraless в главном меню через `OEMainMenuOrder`
  (по умолчанию `exteraless,ghost,pillstack,proxy,storage`), перетаскивание
  сохраняет новый порядок в конфиг.
- Drawer: единственный аккаунт виден в главном меню, активный аккаунт
  получает короткий unread-бейдж; раскрытие секции аккаунтов хранится в
  `OEDrawerAccountsExpanded` (default `true`) и переживает перезапуск.
  Settings Information сохраняет прежнее поведение: при одном аккаунте
  строку не показывает и не включает бейдж активного аккаунта; бейджи
  неактивных аккаунтов сохраняются.
- Pill Stack: выбор видимых пилюль через `OEPillStackPills` и чекбоксы в
  настройках Exteraless.
- Per-chat исключения Ghost Mode: подменю в контекстном меню чата
  (Default / Exclude read receipts / Exclude typing), отдельные ключи
  `OEGhostReadExclusions` и `OEGhostTypingExclusions`.
- Исключения применяются к read history, read contents, discussion,
  saved history, stories и typing через короткоживущие allowance-флаги.

## 2026-11-01

- Проверен Pill Stack в Debug-сборке под Xvfb: CPU, RAM, сеть, DC ping,
  proxy, cache, Ghost и last seen отображаются и обновляются.
- Nix Release-пакет пересобран с Pill Stack и проверен под Xvfb.
- `nix/patches/exteraless.patch` перегенерирован и применяется к чистому
  tdesktop 7.3.1; `nix flake check --no-build` проходит.

## 2026-10-31

- Pill Stack: полоса пилюль под поиском в списке чатов (CPU, RAM, сеть,
  DC ping, прокси, кэш, Ghost, last seen), включение в настройках и главном
  меню. Для DC ping добавлен `MTP::Instance::pingTime()`.
- Рефакторинг условий авто-отключения прокси: `ProxyCondition` enum и
  helpers `HasProxyCondition` / `WithProxyCondition` вместо магических
  `1/2/4`.
- Добавлены `Agents/README.md` и этот changelog.

## 2026-10-30

- M0: форк tdesktop 7.3.1, брендинг, Linux-сборка, flake, Nix-пакет и
  NixOS-модуль.
- M1: раздел Exteraless в настройках, `Relative last seen`, запятая после
  упоминания, авто-отключение прокси, импорт/экспорт `.extera`.
- M2: Ghost Mode (read/stories/typing/online), сохранение удалённых
  сообщений в JSON-sidecar, пункт меню и бокс просмотра.
