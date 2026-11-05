# Статус порта Exteraless Desktop

Обновлено: 2026-11-05.

## Готово

### M0. База

- Форк `telegramdesktop/tdesktop` 7.3.1 в `~/exteraless-desktop`.
- Брендинг: `Exteraless Desktop`, отдельное имя приложения и data-каталог.
- Сборка под Linux (Debug и Release).
- Smoke-тест под Xvfb: окно, QRhi/OpenGL, подключение к DC.
- Flake, пакет `exteraless-desktop`, NixOS-модуль, CI-workflow.

### M1. Настройки, прокси, backup

- Раздел **Exteraless** в настройках tdesktop.
- `Relative last seen`: абсолютное/относительное время последнего визита.
- `Comma after mention`: запятая после упоминания в автодополнении и контекстном меню.
- Авто-отключение прокси по VPN / мобильной сети / Wi-Fi с восстановлением после смены сети.
- Импорт/экспорт `.extera` с Android-совместимым кодированием невидимыми символами.
- Конфиг: `<рабочий каталог>/tdata/exteraless.json`; без `-workdir` это
  `~/.local/share/ExteralessDesktop/tdata/exteraless.json`.
- Nix-пакет собирает upstream tdesktop и применяет `nix/patches/exteraless.patch`.

### Проверки

- `nix flake check --no-build` — проходит.
- `nix build .#exteraless-desktop` — собрано, включая Pill Stack.
- Release smoke-тест: `result/bin/Telegram` запускается под Xvfb.
- Debug smoke-тест: `out/bin/Telegram` запускается под Xvfb.
- Pill Stack smoke-тест: `out/bin/Telegram` и `result/bin/Telegram` с `OEPillStackEnabled=true` под Xvfb, скриншот 1280x1024.
- Pill Stack selection: с `OEPillStackPills=3` показываются только CPU и RAM.
- Per-chat Ghost exclusions: подменю и запись в `OEGhostReadExclusions` проверены под Xvfb; сетевой эффект отдельно не измерялся.
- Drawer order: под Xvfb задан порядок `ghost,exteraless,pillstack,proxy,storage`, drag даёт `exteraless,pillstack,proxy,ghost,storage`; запись в `exteraless.json` и сохранение после перезапуска подтверждены.
- Drawer accounts — проверено под Xvfb на итоговом бинарнике:
  - свежий профиль без ключа: секция стартует раскрытой, строка единственного
    аккаунта видна, активный аккаунт получает короткий unread badge `128.4K`;
  - Settings Information при одном аккаунте строку не показывает и бейдж
    активного аккаунта не включает;
  - collapse/expand пишет в `exteraless.json` `false`/`true` и переживает
    restart.
- Финальная полная Ninja-сборка завершила компиляцию, финальный линк выполнен
  LLD из-за debug-info overflow у GNU ld; `nix flake check --no-build`
  проходит.
- Drawer accounts, поведение по коду (в профиле smoke один аккаунт): бейджи
  неактивных аккаунтов сохраняются — `badgeActive` лишь добавляет бейдж
  активному и не снимает остальные.
- Плагины, этап 1, backend: `exteraless/tests/plugins/run_checks.py` —
  32 проверки PASS на текущем CLI; покрыты фикстуры, рекурсия, лимит 2 MiB,
  отсутствие `python3`, невалидный JSON, ненулевой exit, таймаут, один
  процесс на каталог, отсутствие исполнения и пропуск symlink/каталогов/`.txt`.
- Плагины, этап 1, UI под Xvfb: непустой каталог — `Plugins (6)`, метаданные
  и per-file ошибки; пустой — `Plugins (0)` и `No plugins in ...`; файл с
  исполняемым кодом показан как `Exec Check`, маркер исполнения не создан.
- Плагины, этап 1, сборка: изменённые TU компилируются, бинарник слинкован
  LLD 21.1.8 (GNU ld упирается в debug-info overflow), `out/bin/Telegram`
  sha256 `7559fd57…`, `ninja -C out -n Telegram` не показывает compile/link
  edges кроме glob/CMake re-run.
- Плагины, этап 2, backend: `exteraless/tests/plugins/run_host_checks.py` —
  213 проверок PASS на текущем CLI; покрыты bwrap-профиль (read-only `/` и
  `/plugins`, скрытый HOME, `/tmp`, минимальный environment, нет сети,
  capabilities сброшены), best-effort join session keyring,
  path/symlink/canonical root, protocol `seq`/`ok` и violation, timeout около
  10 с, single host, state persistence, sandbox unavailable/override/failure и
  отсутствие host при старте приложения.
- Плагины, этап 2, UI под Xvfb: при старте с включёнными в `OEPluginStates`
  плагинами host и маркер отсутствуют; открытие Plugins загружает включённые
  плагины (`Plugin host: running`, `Enabled (running)`), toggle OFF/ON,
  close/reopen с тем же host; timeout показывает
  `Enabled (not running): Plugin host timed out.`; выход из приложения
  останавливает host.
- Плагины, этап 2, сборка: итоговый `out/bin/Telegram` sha256 `a7d1c6d9…`,
  mtime 2026-11-04 14:57; `ninja -C out -n Telegram` не показывает
  compile/link edges (только glob/CMake re-run).
- `nix/patches/exteraless.patch` применяется к чистому tdesktop 7.3.1.
- Nix-пакет, `postFixup`: upstream D-Bus `substituteInPlace` не дублируется,
  `nix eval --raw .#packages.x86_64-linux.default.postFixup` содержит один
  вызов substitution и один `wrapProgram`; `bash -n` и shellcheck проходят
  (shellcheck ругается только на переменные upstream-сниппета). Симуляция:
  подмена D-Bus вызывается один раз, `wrapProgram` — один раз; прежний
  дублированный вариант падал на `--replace-fail`.
- Chats: `exteraless/tests/chats/run_checks.py` — 261 проверка PASS
  (44 strip, 10 reactions, 9 CLI, 7 config, 22 general, 27 deleted, 7 reset,
  8 workdir, 8 menu, 22 doubletap, 14 stickertime, 9 pillorder, 74 backup);
  плагины без изменений — 32 и 213 проверок.
- Pill Stack order (NB-16): пилюли перетаскиваются прямо на полосе над
  списком чатов; порядок хранится в `OEPillStackOrder` (CSV из
  `cpu,ram,net,dc,proxy,cache,ghost,lastseen`), санитизируется (уникальность,
  известные ключи, недостающие дописываются), экспортируется в
  `pillstackconfig` `.extera` (суммарно 43 ключа) и применяется при старте.
  CLI `pillorder` (9 проверок) проверяет дефолт, drag-эквивалент, санитайз,
  `fromJson` и backup. Xvfb smoke: перетаскивание меняет порядок, после
  рестарта порядок сохраняется.
- Sticker time (NB-14): селектор `Sticker time` (`As usual`, `Beside sticker`,
  `Hidden`) хранится в `OEChatsStickerTimeSide`/`OEChatsHideStickerTime`,
  отрисовка и hit-test времени вынесены в
  `Element::drawSideInfo`/`sideInfoRect`/`sideInfoTextState` и вызываются из
  `UnwrappedMedia`; затронуты только стикеры, текст и медиа не меняются.
  Android-ключи `stickerTimeMode` и `hideStickerTime` поддержаны в `.extera`
  (суммарно 42 ключа). CLI `stickertime` (14 проверок) проверяет дефолты,
  сеттеры, `fromJson`, алиасы и режимы. Xvfb smoke: у исходящего стикера
  время рисуется слева от стикера и остаётся кликабельным, режим `Hidden`
  убирает время полностью, обычные текстовые сообщения не изменились.
- Double tap action (NB-12): настройки `OEChatsDoubleTapIn`/`OEChatsDoubleTapOut`
  (-1 — как в настройках Telegram, 0 — ничего, 1/2 — любимая реакция, 4 —
  ответ) и резолвер `exteraless/doubletap.{h,cpp}`, применённые в
  `HistoryInner::mouseDoubleClickEvent` и `ListWidget::mouseDoubleClickEvent`.
  Два селектора в подсекции Chats обновляют подпись на месте. Ключи
  поддержаны в `.extera` как `doubleTapAction`/`doubleTapActionOutOwner`
  (суммарно 40 ключей), неподдерживаемые Android-действия 3,5–12 дают
  «ничего» (расширение — NB-91). CLI `doubletap` (22 проверки) проверяет
  дефолты, сеттеры, диапазон, `fromJson`, алиасы и все значения резолвера.
  Xvfb smoke: двойной тап по входящему/исходящему даёт reply, «Do nothing»
  не делает ничего, «Send reaction» ставит любимую реакцию.
- Hide phone number in profile (NB-37): настройка `OEGeneralHidePhone`
  (default `false`) скрывает строку мобильного телефона в профиле контакта
  и своём профиле через `PhoneValue` и `PhoneOrHiddenValue`
  (`info_profile_values.cpp`). Ключ поддержан в `.extera` как
  `hidePhoneNumber` (суммарно 38 ключей). CLI `general` проверяет дефолт,
  сеттер и `fromJson`. Xvfb smoke: включение тумблера пишет
  `OEGeneralHidePhone: true`, в профиле пользователя строка номера телефона
  полностью скрыта.
- Show seconds in timestamps (NB-07): настройка `OEGeneralShowSeconds`
  (default `false`) и `Exteraless::FormatClock` форматируют время с секундами
  в сообщениях (`BottomInfo::layoutDateText`, `FormatEditedDate`) и списке
  чатов (`Ui::FormatDialogsDate`) с сохранением локального формата
  (12/24-часовой, AM/PM). Ключ поддержан в `.extera` как
  `formatTimeWithSeconds` (суммарно 37 ключей). CLI `clock` и `general`
  (17 проверок) проверяют дефолты, C/ru/en локали, полночь и roundtrip.
  Xvfb smoke: включение тумблера пишет `OEGeneralShowSeconds: true`, время в
  сообщениях и в списке чатов отображает секунды (`10:07:03 PM`, `9:35:16 PM`).
- Visible message menu items (NB-11): подсекция «Visible message menu items»
  в Chats с 11 чекбоксами `OEChatsMenu*` управляет видимостью пунктов
  сообщения в обоих виджетах истории (`history_inner_widget.cpp` и
  `history_view_context_menu.cpp`). Ключи экспортируются в `.extera`
  (суммарно 36 ключей), импортируются с поддержкой Android-алиасов.
  CLI-группа `menu` (8 проверок) проверяет дефолты (все `true`), сеттеры,
  `fromJson` и алиасы. Xvfb smoke: снятие чекбокса `Pin / Unpin` пишет
  `OEChatsMenuPin: false` в `exteraless.json`, контекстное меню сообщения в
  чате скрывает пункт `Pin`; горячие клавиши не ломаются.
- Exteraless settings reset: кнопка «Reset Exteraless settings» в подсекции
  Reset показывает подтверждение, очищает `_values` и перезаписывает
  `exteraless.json` пустым `exteraconfig`, возвращая дефолты; после
  подтверждения секция пересоздаётся и вызывает `ApplyProxyRules()`.
  CLI-группа `reset` проверяет очистку, дефолты, persisted-файл и отсутствие
  OE-ключей. Xvfb smoke на профиле с аккаунтом: кнопка видна, подтверждение
  открывается, Reset очищает файл до `{}` и секция показывает дефолты.
- `-workdir`: `Application` передаёт `cWorkingDir()` в
  `Config::setWorkingDir()`, поэтому `exteraless.json` читается и пишется в
  `<workdir>/tdata/exteraless.json`. При первом запуске, если нового файла
  нет, старый файл из `QStandardPaths::AppLocalDataLocation` копируется.
  CLI использует `EXTERALESS_WORKDIR` (8 проверок). Xvfb smoke на копии
  профиля с `-workdir`: старый конфиг скопирован, применён (Show exact
  counters и Compact unread messages label включены), старый файл не изменён.
- Backup `.extera`: экспортируются все 25 desktop-ключей в трёх секциях —
  `exteraconfig` (21), `pillstackconfig` (2), `mainconfig` (2). Импорт
  проверяет тип, целочисленность и диапазон, санитизирует Ghost-exclusions и
  plugin states, сбрасывает производный `OEProxyAutoDisabled`, принимает
  старый плоский формат и репортит неизвестное с префиксом секции.
  Round-trip «export → import → export» совпадает; импорт полного набора
  применяет 25 ключей, импорт неизвестного набора — 2 известных и 5
  неизвестных. Xvfb boot smoke 35 с PASS с невалидными exclusions/plugins
  в конфиге.
- Chats/Deleted messages: `OESaveDeletedMessages` (default `false`) теперь
  реально управляет записью: при выключенном ключе ничего не сохраняется,
  архив не урезается при загрузке, тумблер «Save deleted messages» добавлен
  в подсекцию Chats. Sidecar ограничен 1000 записями и 100 на чат; при
  добавлении и при загрузке с включённым ключом старые записи вытесняются,
  урезанный файл перезаписывается. CLI-режимы `deleted`, `deleted-set`,
  `deleted-trim` покрывают дефолт, персистентность, `fromJson`, дефолтные
  лимиты и ротацию. Xvfb smoke на профиле с аккаунтом: тумблер
  «Save deleted messages» виден в подсекции Chats, клик включает и выключает
  его и пишет `OESaveDeletedMessages` в `exteraless.json`; приложение не
  падает.
- Chats, Xvfb smoke: подсекция `Chats` с шестью тумблерами, tail on/off в
  личном чате, реакции скрыты в канале и группе, тумблер unread capsule
  переключается и пишет `OEChatsUnreadCapsule`; сам unread-разделитель в
  smoke-профиле не появился (чаты автоматически помечались прочитанными),
  поэтому отрисовка капсулы подтверждена сборкой и кодом, но не скриншотом.
  Личного чата с реакциями в профиле не было, поэтому реакции в личных чатах
  проверены только CLI. После включения настройки уже открытый чат сохраняет
  старые tail/reactions, реакции в pinned bar обновляются только после
  перезапуска; unread capsule читается на каждой перерисовке.
- Chats/General, сборка: итоговый `out/bin/Telegram` sha256 `36cb6a3e…`,
  mtime 2026-11-05 05:24; GNU ld по-прежнему упирается в debug-info overflow,
  поэтому затронутые TU собраны точечно и финальный линк выполнен LLD.
  `ninja -C out -n Telegram` после этого не показывает compile/link edges
  (только glob/CMake re-run).
- Локализация секции Exteraless: все заголовки, чекбоксы, тумблеры и
  сообщения боксов переведены на `tr::lng_settings_exteraless_*` (30 новых
  ключей в `Telegram/Resources/langs/lang.strings`, суммарно 41). Сгенерирован
  `td_lang`, пересобран `lang_instance.cpp` под обновлённый `kKeysCount`.
  Xvfb smoke на профиле с кэшированным языковым пакетом: секция открывается
  без падений, верхняя и нижняя части отображают локализованные подписи.
- Chats, патч: `nix/patches/exteraless.patch` перегенерирован, `apply
  --check` и apply на чистом v7.3.1 проходят; `nix flake check --no-build`
  проходит, полная Nix-сборка не запускалась.
- General: `OEGeneralDisableNumberRounding` (default `false`) показывает
  точные счётчики вместо `K/M/B`; backup-ключ `disableNumberRounding`
  импортируется/экспортируется, тумблер находится в General.
- Chats paste: `OEChatsStripTrackingOnPaste` (default `true`) очищает
  tracking-параметры в plain-text/URL вставке; rich-text/HTML не меняется,
  ключ входит в `.extera` как `OEChatsStripTrackingOnPaste`, CLI покрывает
  текст с несколькими ссылками.

### Артефакты

- История разработки — в git репозитория.
- Nix-пакет не пересобран после per-chat Ghost exclusions, выбора пилюль, drawer order, drawer accounts, плагинов этапа 2 и батча Chats; `nix flake check --no-build` и `postFixup` проверены без полной сборки.
- Nix-пакет: локальная сборка flake (`./result`), store-путь не фиксируется.
- `nix/package.nix` и `flake.nix` не входят в `nix/patches/exteraless.patch` by design: пакет применяет патч к upstream tdesktop, а wrapper/devShell живут в flake.
- Локальная Debug-сборка: `out/bin/Telegram`.

## В работе / дальше

### MVP2

- [x] Ghost Mode: read receipts, stories, online/offline, typing/upload progress.
- [x] Сохранение удалённых сообщений: JSON-sidecar, контекстное меню и бокс.
- [x] Per-chat исключения Ghost Mode: read receipts и typing через контекстное меню.
- [x] Pill Stack: CPU/RAM, сеть, DC ping, proxy, cache, ghost, last seen; выбор видимых пилюль.
- [x] Drawer: пункты Exteraless в главном меню (Ghost, Pill Stack, proxy, cache).
- [x] Drawer: порядок пунктов в главном меню (`OEMainMenuOrder`, drag-and-drop).
- [x] Drawer: аккаунты и unread-бейджи.
- [x] Chats: хвост пузыря, скрытие реакций в каналах/группах/личных чатах,
  вырезание tracking-параметров при открытии http/https-ссылок и компактная
  unread capsule; настройки применяются после повторного открытия чата,
  реакции в pinned bar — после перезапуска.

### MVP3

- [ ] Python-плагины: установка/удаление, репозиторий и обновления.
- [x] Плагины, этап 1: манифест и просмотр (`tdata/plugins`, AST без исполнения, разрешения, ошибки, бокс).
- [x] Плагины, этап 2: контролируемый host-процесс `bwrap` + `python3 -I -B -c`,
  JSON-lines IPC с `seq`/`ok`, load/unload, audit-лог
  `tdata/plugins-audit.log`, persistent `OEPluginStates`, тумблеры и
  автозагрузка при открытии Plugins; запуск приложения код плагинов не
  исполняет, выход из приложения останавливает host.
- [ ] Просмотр audit-лога в UI.
- [ ] Совместимость с частью Android-плагинов через shim.

### После MVP

- [ ] AI-клиент.
- [ ] Feed.
- [ ] Плеер и now playing/MPRIS.
- [ ] Иконпаки.
- [ ] Vosk/распознавание речи.
- [ ] Flatpak/AppImage.

## Ограничения

- Пакет по умолчанию использует API-ключи Snap-сборки tdesktop; для своего
  дистрибутива нужны собственные `apiId`/`apiHash`.
- `exteraless.json` лежит в `<рабочий каталог>/tdata` и следует `-workdir`.
  При первом запуске с другим workdir старый файл из
  `QStandardPaths::AppLocalDataLocation` копируется, если нового файла ещё
  нет; старый файл остаётся на месте.
- Плагины Android-версии несовместимы: нужен новый desktop API.
- Плагины этапа 2, известные ограничения:
  - `bwrap` доступен только на Linux; без него host fail closed
    (`Plugin sandbox is unavailable.`), плагины не запускаются. Это
    единственная фактическая граница этапа.
  - Python audit hook — best-effort policy layer, а не security boundary.
    Плагин может отключить его целиком: скрипт host исполняется в
    `__main__`, и `import __main__; __main__._IN_AUDIT[0] = True` выключает
    все проверки (`socket()` после этого создаётся). Deny-by-default —
    только про набор известных audit-событий, не про изоляцию.
  - Потоки сейчас разрешены: `threading.Thread` в Python 3.14 стартует через
    `_thread.start_joinable_thread`, которого нет в deny-наборе; запись
    `threading.Thread` в deny-наборе — мёртвая (такого audit-события нет).
    Запрещён только прямой `_thread.start_new_thread`.
  - Нет seccomp/landlock: bwrap ограничивает namespaces, mounts, network и
    capabilities, но не syscall-поверхность; `/nix` доступен read-only.
  - Session keyring отделяется best-effort и не изолируется полностью.
  - Один host на приложение: timeout или crash убивает его и выгружает все
    плагины; загрузка из UI синхронная (до 10 с).
  - Nix-пакет добавляет `bubblewrap` и `python3` в `PATH` через
    `wrapProgram`; полная сборка пакета после этапа 2 ещё не выполнялась
    (проверены только `postFixup`, `nix flake check --no-build` и
    применение патча).

## Сборка с ограничением ОЗУ

На машине 16 ГБ RAM без свапа:

```bash
nix develop
cmake -S . -B out -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DTDESKTOP_API_ID=YOUR_API_ID \
  -DTDESKTOP_API_HASH=YOUR_API_HASH
cmake --build out --target Telegram -j3

nix build .#exteraless-desktop --cores 4 --max-jobs 1
```

Для длинных сборок используется systemd-юнит с `MemoryMax`, чтобы не
словить OOM.

## Запуск

```bash
cd ~/exteraless-desktop
./result/bin/Telegram
# или
nix run .#exteraless-desktop
```

Из Android-репозитория `~/exteraless` запускать нельзя: flake лежит в
`~/exteraless-desktop`.
