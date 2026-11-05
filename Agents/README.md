# Exteraless Desktop — карта проекта для агентов

`exteraless-desktop` — форк Telegram Desktop 7.3.1, в который переносятся
функции Android-клиента exteraless. Android-репозиторий лежит рядом, отдельным
sibling-чекаутом: `../exteraless`.

## Структура

- `Telegram/` — upstream tdesktop; правки минимальные, точка входа `Telegram/SourceFiles`.
- `exteraless/` — наши модули без зависимости от UI, где возможно:
  - `config.*` — JSON-настройки `tdata/exteraless.json`, включая
    `OEPluginStates` (включённость плагинов, default `false`);
  - `proxy_rules.*` и `proxy_rules_state.*` — авто-отключение прокси по типу сети
    (состояние в памяти, один раз на переход);
  - `ghost_mode.*` — фильтр исходящих MTProto-запросов;
  - `deleted_entry.*` и `deleted_messages.*` — JSON-sidecar удалённых
    сообщений (схема v2: реакции, reply, forward, entities, правка, read);
  - `backup.*` — формат `.extera`;
  - `network.*` — тип сети через Qt NetworkInformation + fallback по интерфейсам;
  - `plugins/scanner.*` — запуск `python3 -I -B` и разбор JSON манифестов;
  - `chats.*` — `ChatsSettings`, правило скрытия реакций и unread capsule;
  - `links.*` — `StripTracking` для http/https-ссылок.
- `Telegram/SourceFiles/exteraless/plugin_runtime.*` — singleton `Runtime`:
  один `QProcess` на приложение (bwrap + Python host), JSON-lines IPC,
  проверка путей, audit-лог, load/unload и остановка host при выходе.
- `Telegram/SourceFiles/exteraless/pill_stack.*` — полоса Pill Stack в списке чатов.
- `Telegram/SourceFiles/exteraless/plugins_box.*` — бокс «Plugins»: список,
  ошибки, пустое состояние, тумблеры и статус host.
- `exteraless/tests/plugins/` — CLI `exteraless-plugins-scan` (stage 1),
  CLI `exteraless-plugin-host-cli` (stage 2), фикстуры, `run_checks.py`
  (32 проверки) и `run_host_checks.py` (213 проверок) с фикстурами
  `fixtures/runtime/`; в приложение не входят.
- `exteraless/tests/chats/` — CLI `exteraless-chats-cli`, фикстуры
  `fixtures/strip.tsv` и `fixtures/reactions.tsv`, `run_checks.py`
  (339 проверок: strip, reactions, CLI, config, general, deleted, deleted-entry, reset, workdir, menu, doubletap, stickertime, pillorder, ghost, deletedmark, vpn, proxy-rules, pangu, backup);
  в приложение не входят.
- `Telegram/SourceFiles/settings/sections/settings_exteraless.cpp` — раздел настроек.
- `Telegram/SourceFiles/boxes/deleted_messages_box.cpp` — бокс удалённых
  сообщений, показывает реакции и метки reply/forward/edited, а также
  значок/кастомную метку, прозрачность и цвет (NB-20); inline-показ в чате —
  NB-92.
- `nix/upstream-base` — закреплённая база патча: локальный коммит, tree-хеш и
  upstream-коммит; `nix/update-patch.sh --check` проверяет применимость.
- `Telegram/SourceFiles/mtproto/mtproto_request_filter.h` — интерфейс фильтра запросов.
- `Telegram/SourceFiles/window/window_main_menu.cpp` — пункты Exteraless в главном меню.
- Плагины этапа 1: каталог `cWorkingDir()/tdata/plugins`, манифесты
  `.plugin`/`.py`, `.elyx`/`.eaf` показываются как unsupported. Сканер
  читает метаданные через AST и не исполняет код плагина.
- Плагины этапа 2: один host на приложение — `bwrap` + `python3 -I -B -c`
  с JSON-lines (`seq`/`ok`, `load`/`unload`). bwrap даёт read-only `/` и
  `/plugins`, скрытый HOME, минимальный environment, `/tmp`, без сети и со
  сброшенными capabilities; host best-effort входит в отдельный session
  keyring. Audit hook — best-effort policy layer, а не security boundary:
  пишет отказы в `tdata/plugins-audit.log`, но плагин может выключить его
  через `import __main__; __main__._IN_AUDIT[0] = True`, а потоки
  (`threading.Thread` в Python 3.14) сейчас разрешены — deny-запись
  `threading.Thread` мёртвая, запрещён только прямой
  `_thread.start_new_thread`. bwrap доступен только на Linux и без него host
  fail closed. Включённые плагины (`OEPluginStates`, default `false`)
  загружаются только при открытии бокса Plugins, закрытие бокса host не
  останавливает, выход из приложения — останавливает; при старте приложения
  код плагинов не исполняется. Manifest permissions информационные и прав не
  дают.
- Pill Stack выключен по умолчанию (`OEPillStackEnabled`), включается в
  настройках Exteraless или в главном меню. Состав пилюль — `OEPillStackPills`
  (битовая маска, по умолчанию все).
- Per-chat Ghost exclusions хранятся в `OEGhostReadExclusions` и
  `OEGhostTypingExclusions`; меню — «Ghost Mode exclusions» в контекстном
  меню чата.
- Подсекция `Chats` в настройках Exteraless: `OEChatsRemoveMessageTail`
  (default `true`), `OEChatsHideReactionsChannels`/`Groups`/`Private`
  (default `false`), `OEChatsStripTrackingOnOpen` (default `true`),
  `OEChatsStripTrackingOnPaste` (default `true`) и `OEChatsUnreadCapsule`
  (default `false`). Tail и reactions читаются при построении чата, поэтому
  применяются после его повторного открытия, а реакции в pinned bar могут
  требовать перезапуска. Strip tracking срабатывает на клик по http/https и
  на plain-text/URL paste без HTML; allowlist
  консервативный (`utm_*` плюс известные click-id), `ref`/`source`/`s`/`si`
  и прочие неоднозначные параметры сохраняются, percent-encoded имена не
  декодируются. Backup пишет все 54 desktop-ключа в секциях `exteraconfig`,
  `pillstackconfig` и `mainconfig`; `OEProxyAutoDisabled` при импорте
  сбрасывается, потому что это производное сетевое состояние.
- Двойной тап по сообщению: `OEChatsDoubleTapIn`/`OEChatsDoubleTapOut`
  (-1 — как в настройках Telegram, 0 — ничего, 1/2 — любимая реакция, 4 —
  ответ); резолвер — `exteraless/doubletap.{h,cpp}`.
- Время на стикерах: `OEChatsStickerTimeSide` (сбоку) и
  `OEChatsHideStickerTime` (скрыто), отрисовка и hit-test в `UnwrappedMedia`
  через `Element::drawSideInfo`/`sideInfoRect`/`sideInfoTextState`.
- Порядок пилюль: drag на полосе `PillStack`, сохраняется в `OEPillStackOrder`
  (CSV из `cpu,ram,net,dc,proxy,cache,ghost,lastseen`).
- Ghost: пункт главного меню включает/выключает все четыре тумблера и
  показывает `On`/`Off`/`Partial` при `OEGhostShowStatus`; статусные функции —
  `exteraless/ghost_status.{h,cpp}`. Пять `OEGhostSend*Locked` защищают
  отдельные тумблеры от пакетного переключения и исключаются из статуса.
- Порядок пунктов Exteraless в главном меню — `OEMainMenuOrder` (CSV,
  по умолчанию `exteraless,ghost,pillstack,proxy,storage`); drag-and-drop
  сохраняет новый порядок в конфиг.
- Drawer — это главное меню tdesktop: единственный аккаунт в нём тоже виден,
  активный аккаунт получает короткий unread badge
  (`unreadWithMentionsBadge`); раскрытие секции аккаунтов хранится в
  `OEDrawerAccountsExpanded` (default `true`) — это фактический источник
  состояния для drawer. Upstream `mainMenuAccountsShown` пишется и из
  главного меню, и из `Main::Domain::add` при добавлении второго аккаунта;
  известное ограничение: upstream auto-expand и синхронизация уже открытых
  главных меню в drawer пока не отражаются. Settings Information использует
  дефолтные `AccountsOptions`.
- `nix/` — flake-пакет, NixOS-модуль и патч upstream.
- `STATUS.md` — что готово и что дальше.
- `CHANGELOG.md` — история изменений.
- `docs/nagram-feature-audit.md` — полная карта функций exteraless/NagramX и
  степени их переноса.
- `docs/nagram-porting-backlog.md` — приоритизированный P0/P1/P2 бэклог
  переноса.
- `docs/nagram-master-plan.md` — краткий порядок исполнения и точка
  возобновления после сжатия контекста.
- `ACKNOWLEDGEMENTS.md` — происхождение цепочки форков, благодарности и границы заимствований.

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

`nix/package.nix` сохраняет upstream `postFixup` как есть и добавляет только
`wrapProgram`, который кладёт `bubblewrap` и `python3` в `PATH`; upstream
D-Bus `substituteInPlace` не дублируется (иначе `--replace-fail` падает на
повторной подмене). devShell в `flake.nix` содержит те же инструменты для
запуска host и тестов. Полная сборка пакета после этапа 2 и батча Chats ещё
не выполнялась: проверены `postFixup` через `nix eval`, `bash -n`/shellcheck,
`nix flake check --no-build` и применение патча. Изменения
`flake.nix`/`nix/package.nix` не входят в `nix/patches/exteraless.patch` —
патч применяется к upstream-исходникам, а wrapper и devShell живут в flake.

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
