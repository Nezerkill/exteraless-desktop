# Статус порта Exteraless Desktop

Обновлено: 2026-11-07.

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
- Config: неиспользуемый `Config::load()` удалён, поведение ленивой загрузки
  не изменилось.
- Патч: база закреплена `nix/upstream-base` (tree-хеш `4cf9e0e1…`, upstream
  `6fed91f`, локальный коммит `aa09910`); `./nix/update-patch.sh --check`
  применяет патч к чистому worktree и печатает sha256 — PASS.
- Сборка: `bash nix/smoke-test.sh` PASS (приложение живёт 30 с под xvfb-run);
  путь к бинарнику и `LD_LIBRARY_PATH` исправлены.
- Plugins audit log (NB-28): кнопка «Audit log» в боксе плагинов показывает
  последние 24 строки `tdata/plugins-audit.log`, «Export» открывает файловый
  диалог с `plugins-audit.log`; Xvfb подтвердил.
- Nix (NB-40): устаревший `result/` (symlink на сборку от 1 ноября) удалён;
  `nix build` создаёт новый результат.
- Docs (NB-39): `docs/feature-mapping.md` сверен с кодом, убрано
  несуществующее утверждение о `pinnedReactions`; Deleted messages описывает
  схему v2 и метку в боксе (inline — NB-92).
- Ghost offline-ключ (NB-38): мёртвые `OEGhostSendOfflineAfterOnline` и
  `OEGhostSendOfflineAfterOnlineLocked` удалены из config, `.extera` и UI;
  в `.extera` осталось 86 ключей. CLI-тесты PASS (363), Xvfb не требовался —
  изменение только удаляет неиспользуемую настройку.
- Chats: `exteraless/tests/chats/run_checks.py` — 1058 проверок PASS
  (44 strip, 10 reactions, 63 CLI, 7 config, 22 general, 9 bot-privacy,
  27 deleted, 7 deleted-entry, 7 reset, 8 workdir, 8 menu, 24 doubletap,
  14 stickertime, 11 pillorder, 19 ghost, 10 deletedmark, 10 vpn,
  6 proxy-rules, 15 pangu, 17 zalgo, 5 silent, 5 sections, 5 ai-editor,
  19 pluginops, 5 send-as, 5 profile-buttons, 31 inline-math,
  15 save-subfolder, 12 ghost-schedule, 8 seek-duration, 21 sticker-size,
  6 edited-mark, 5 forward-date, 5 sender-mini-avatars, 5 media-menu,
  5 chat-menu, 5 disable-link-preview, 5 hide-all-chats, 13 hide-archive-sort,
  5 always-show-download, 7 show-results-before-voting, 5 false-bottom,
  5 decoration, 5 shortcuts, 5 message-menu-style, 7 reply-style,
  5 wide-posts, 15 chat-misc, 15 persian, 9 autodownload-rules, 19 media-quality,
  19 update-channel, 32 translation, 23 ai, 13 ai-stream, 6 recognition,
  17 network, 10 download-boost, 48 subtitles, 7 avatar-corners,
  8 header-style, 7 spoilers-direct, 7 formatting-buttons, 11 section-radius,
  27 regex-filters, 32 lyrics, 27 lrclib, 39 extra-pills, 105 backup);
  плагины без изменений — 32 и 252 проверок.
- Пилюли погоды, курсов и золота (NB-56): в `PillStack` шесть новых
  `PillStackPill` — `Weather` (256), `Btc` (512), `Eth` (1024), `Usd` (2048),
  `Eur` (4096) и `Gold` (8192), — поэтому `kPillStackAllPills` вырос с
  `0xFF` до `0x3FFF`. Сеть и разбор — Qt-only `exteraless/extra_pills.{h,cpp}`:
  `WeatherApiUrl` (Open-Meteo, `current`), `CoinbaseSpotUrl` (эндпоинт
  `prices/<код>-USD/spot`), `EcbDailyUrl`, `GoldApiUrl`; парсеры `ParseWeather`,
  `ParseCoinbase`, `ParseEcbXml` + `FormatFiatRate` (рубль из куба RUB, USD —
  кросс-курсом `rubPerEur / usdPerEur`) и `ParseGoldApi`. `ExtraPills::Refresh`
  обновляет слоты не чаще раза в 30 минут, держит значения в памяти и отдаёт
  прочерк `—` на пустом ответе или ошибке сети, поэтому сбой провайдера не
  ломает полосу. Шесть тумблеров и шесть lang-ключей
  `lng_settings_exteraless_pill_{weather,btc,eth,usd,eur,gold}`; CLI
  `extra-pills-parse <domain> <file> [code]`, семь фикстур, `check_extra_pills` —
  39 проверок; chats-раннер 1045 (1006 + 39), плагины 32 и host-раннер 252
  без изменений, backup 105; `kKeysCount` 11241 (+6), `kTagsCount` 185;
  sha256 патча `0156a54c…`, `nix flake check` PASS. Не сделано: выбор города —
  координаты Москвы захардкожены. Ограничения: ECB больше не публикует RUB,
  поэтому fiat-пилюли на живом endpoint показывают прочерк (follow-up — XML
  ЦБ РФ); кэш только в памяти, после рестарта прочерк до первого fetch;
  порядок пилюль санитизируется по 14 ключам (`SanitizePillStackOrder` расширен);
  Xvfb-смоук не выполнялся.
- MD3-плеер, фаза 2 — онлайн-тексты lrclib.net (NB-50): Qt-only
  `exteraless/lrclib.{h,cpp}` — `BuildSearchUrl`/`BuildGetUrl` и разбор
  `ParseSearch`/`ParseGet` (id, track, artist, `hasSynced`; у текста
  `ok`/`synced`/`instrumental`/`text`, битый JSON и пустая выдача дают
  `ok: false`). Оверлей текста `exteraless/media_lyrics.cpp` получил
  `fetchOnline`: гейт `OELyricsAllowNetwork` (bool, default `false`, алиас
  `lyricsAllowNetwork`), один `QNetworkAccessManager` на процесс,
  диск-кэш `tdata/lrclib/<docId>.json` на 30 дней, псевдо-путь
  `lrclib:<docId>` в `OELyricsFiles` с подписью `lrclib.net`,
  `abortPending` при disable и повторном поиске. В меню медиавьюера
  «Find Lyrics Online» только при включённом гейте; в настройках тумблер
  «Allow online lyrics search (lrclib.net)», 3 новых lang-ключа. CLI
  `lyrics-search-parse`/`lyrics-get-parse` (всего четыре режима
  `lyrics-*` с фазой 1), пять фикстур, `check_lrclib` — 27 проверок;
  chats-раннер 1006 (= 978 + 27 lrclib + 1 backup), backup 105, плагины 32
  и host-раннер 252 без изменений; `kKeysCount` 11235 (+3), sha256 патча
  `cb95c305…`, `nix flake check` PASS. Не сделано (follow-up): MD3-ребилд
  плеера, мини-плеер, очередь и drag, auto-fetch по совпадению, server-side
  транскрипция (= NB-51). Ограничения: только ручной поиск; Xvfb-смоук не
  выполнялся.
- Ответ на удалённое цитатой (NB-33, закрывает split NB-20): пункт Reply у
  плейсхолдера удалённого (`AddReplyToMessageAction`,
  `history/view/history_view_context_menu.cpp`) идёт через
  `ListWidget::quoteDeletedMessageRequested()` и
  `quoteDeletedMessageRequestNotify()` (event stream
  `_requestedToQuoteDeletedMessage`) вместо `replyToMessageRequestNotify`.
  Подписка `ChatWidget` берёт запись
  `DeletedMessages::entryForPlaceholder(FullMsgId)` и строит префикс
  `BuildQuotePrefix(entry, fromName)` — жирный автор, текст с entities
  (сдвинуты на длину автора) и пустая строка, — помечает его
  `Ui::InputField::kTagBlockquote` и ставит перед текущим compose
  (`ComposeControls::setText` + фокус), сдвигая entities черновика.
  Это цитата, а не ответ: `_replyTo` не трогается, `reply_to_msg_id` не
  отправляется, postponed-очереди не затрагиваются, `FullMsgId` остаётся
  локальным. Удаление плейсхолдера снимает регистрацию, sidecar-запись
  остаётся и пере-вставляется при перестройке истории. Chats-раннер — 978
  проверок, плагины 32 и host-раннер 252 — без изменений (новых ключей и
  CLI у NB-33 нет); `kKeysCount` 11232, `.extera` не тронут; sha256 патча
  `18b582f9…`, `nix flake check` PASS. Не сделано: inline-медиа и реакции
  в плейсхолдере (наследие NB-92); Xvfb-смоук не выполнялся.
- Inline-показ удалённых (NB-92, продолжение NB-20):
  `History::addLocalMessageInPlace` (`history/history.cpp`) вставляет запись
  sidecar в историю на её место — порядок по паре `(date, entry.msgId)` — и
  регистрирует элемент через `registerClientSideMessage`; реестр
  `FullMsgId → MsgId` в `DeletedMessages` (`_inserted`/`_placeholders`)
  различает вставки (`isPlaceholder`) и снимает их
  (`insertFor`/`removeFrom`/`refresh`). Вставка идёт только при
  `loadedAtTop()` и `loadedAtBottom()`; метка дописывается текстом (🗑 из
  `OEChatsUseDeletedIcon` или `OEChatsCustomDeletedMark`), прозрачность 0.6
  рисует `Message::draw` при `OEChatsTranslucentDeleted`; тумблеры save и
  translucent вызывают `refresh()`, Reply и Delete плейсхолдера закрыты
  guard'ом в контекстном меню. Chats-раннер — 978 проверок, без изменений
  (новых ключей и CLI у NB-92 нет), плагины 32 и host-раннер 252 без
  изменений; `kKeysCount` 11232, `.extera` не тронут; sha256 патча
  `8246f873…`, `nix flake check` PASS. Не сделано: inline-медиа и реакции,
  цветная метка — подходящего `EntityType` нет, `kMaxEntityType` не менялся
  (реальный максимум 27 покрывают текущие 28). Ограничения: вставки
  появляются после перестройки истории; удаление плейсхолдера снимает только
  регистрацию — запись sidecar остаётся; Xvfb-смоук не выполнялся.
- MD3-плеер, фаза 1 — LRC и тексты (NB-50): Qt-only парсер
  `exteraless/lyrics.{h,cpp}` (`ParseLyrics`/`ActiveIndex`) читает один LRC
  (SRT не принимается): `[offset:±ms]` сдвигает таймстемпы, мультитаймстемпы
  `[mm:ss.xx]` дублируют строку, word-time `<mm:ss.xx>` внутри текста
  вырезается, BOM UTF-8/UTF-16 LE/BE распознаётся, прочие теги в `[...]`
  отбрасываются; файл без таймстемпов — unsynced (одна строка от нуля,
  `synced: false`), без текста — instrumental. Память per-video —
  `Telegram/SourceFiles/exteraless/media_lyrics.{h,cpp}`: JSON-карта в
  `OELyricsFiles` (алиас `lyricsFiles`, санитайзер в импорте `.extera`),
  `activeText` по позиции. UI: «Choose Lyrics File…» (`FileDialog` `*.lrc`)
  и «Disable Lyrics» в меню медиавьюера под гейтом аудио; в overview
  аудио-блок показывает активную строку акцентом (`songLyricsActiveFg`),
  сам подкручивает к ней и держит ≤ 3 строк (`songLyricsMaxLines`,
  `songLyricsTop`). CLI `lyrics-parse`/`lyrics-active`, пять фикстур,
  `check_lyrics` — 32 проверки; chats-раннер 978 (= 943 + 32 + 2 CLI +
  1 backup), backup 104, плагины 32 и host-раннер 252 без изменений;
  `kKeysCount` 11232 — lang не менялся, пункты меню — литералы; sha256
  патча `3b570173…`, `nix flake check` PASS. Не сделано: lrclib.net с кэшем
  и consent'ом, MD3-ребилд плеера, мини-плеер, очередь/drag, server-side
  транскрипция (= NB-51). Ограничения: выбор файла виден после переоткрытия
  вкладки Media; Xvfb-смоук не выполнялся.
- Плагины, этап 3b — меню плагина (NB-30): `MenuItemType` `main`/`message`/
  `chat` (PROFILE не перенесён), `add_menu_item`/`remove_menu_item`, снимок
  `menu` в ответе `loaded`, `cmd: menu` → `clicked`/`error`; валидация — id
  по `^[A-Za-z][A-Za-z0-9_-]{1,31}$`, текст 1..128 без управляющих, subtext
  ≤ 128, priority ±1000, ≤ 64 на плагин и ≤ 256 всего; клик — таймаут 2 с,
  re-entrancy-гард, host сам не стартует. UI: отдельный `VerticalLayout` в
  главном меню вне reorder, пункты в конце контекстного меню сообщения,
  счётчик `menu items: N` в боксе; CLI `menu-list`/`menu-click`, фикстура
  `valid-menu.plugin`; `check_plugin_menu` — 24 проверки, host-раннер 252
  (228 → 252), chats 943 и плагины-сканер 32 без изменений; `.extera` —
  138 ключей (не тронут); `kKeysCount` не менялся; sha256 патча `e32d103e…`,
  `nix flake check` PASS. Ограничения: синхронный клик на UI-потоке (~2 с),
  снимок только при `load`, контекст `chat` без UI, Xvfb-смоук меню не
  выполнялся (только CLI + IPC).
- Плагины, этап 3a — настройки плагинов (NB-79.3a): `OEPluginSettings` —
  JSON `id → {key: value}` с алиасом `pluginSettings`; лимиты: id по
  `^[A-Za-z][A-Za-z0-9_-]{1,31}$`, ключ ≤ 64, ≤ 128 ключей, ≤ 64 KiB,
  глубина ≤ 4; в `.extera` ключ не экспортируется, импорт — миграция.
  Host: `_SETTINGS` и `get_setting`/`set_setting`/`export_settings`/
  `import_settings`, снимок в ответе `load`, dirty-персист (`saveSetting`
  пишет сразу); бокс `ShowPluginSettingsBox` (Switch/Selector/Input/
  Header/Divider, `Custom` — заглушка), CLI `settings-get`/`settings-set`,
  фикстура `valid-settings.plugin`; `check_plugin_settings` — 15 проверок;
  host-раннер 228 (213 → 228), chats 943 и плагины-сканер 32 без
  изменений; `.extera` — 138 ключей (не тронут); `kKeysCount` 11232
  (`kTagsCount` 185) — lang не менялся; sha256 патча `ab0d18e9…`,
  `nix flake check` PASS. Не сделано: intents и trust-уровни (этап 3c),
  пикер alwaysAsk (= NB-55). Ограничения: правки
  бокса видны при следующем enable плагина; Xvfb-смоук не выполнялся.
- IPv6 и DNS over HTTPS (NB-77): `OENetworkUseIPv6` (bool, default `true` вне
  Windows), `OENetworkDnsType` (int `0..1`, default `0`: `0` — built-in DoH,
  `1` — system) и `OENetworkCustomDoH` (строка, пусто — выключено), алиасы
  `useIPv6`/`dnsType`/`customDoH`. Резолвер
  `mtproto/details/mtproto_domain_resolver.{h,cpp}` при `dnsType == 1`
  добавляет первой попытку `Type::System` (`QHostInfo::lookupHost`, TTL 5
  минут), а непустой customDoH — попытку с URL из конфига перед встроенными
  хостами (встроенный путь остаётся fallback'ом); диалект по хосту:
  cloudflare → JSON `/dns-query`, иначе Google `/resolve`, явный путь в URL
  сохраняется. IPv6-флаг засевается в `Application::startLocalStorage`
  через `settings().proxy().setTryIPv6()` после `Local::start()` — не в
  конструкторе, потому что upstream-настройки читаются с диска позже.
  В подсекции Network — тумблер «Try IPv6 connections», селектор «DNS
  provider» (Built-in DoH/System) и поле «Custom DoH URL», шесть lang-ключей;
  CLI 4 режима (`network`, `network-set-ipv6`, `network-set-dns`,
  `network-set-doh`, `-` очищает URL), `dnsType` клампится в `0..1`;
  `check_network` — 17 проверок (счётчик 943, backup 103, плагины 32 и 213
  без изменений); `.extera` — 138 ключей (133 + 3 + 2, сверено с
  legacy-кортежем `run_checks`); `kKeysCount` 11232 (`kTagsCount` 185);
  sha256 патча `da3b860a…`, `nix flake check` PASS. Сборке нужен `exteraless`
  в `PRIVATE`-зависимостях `td_mtproto`
  (`Telegram/cmake/td_mtproto.cmake`). Ограничения: IPv6 реально работает
  только в сетях с IPv6; смоук «подключение через DoH» переписан — DC задан
  IP, DoH виден только на прокси с hostname; customDoH идёт NoProxy; конфиг
  форка — single source of truth, поэтому тумблер upstream Proxies
  перезатирается при рестарте; Xvfb-смоук не выполнялся.
- Regex-фильтры сообщений (NB-22): движок `exteraless/regex_filters.{h,cpp}`
  (`Filter{regex, caseInsensitive, reversed, enabled}`, `CompileFilters` с
  `optimize()`, `MatchesText` — совпадение `find()` инвертируется флагом
  `reversed`, `Matcher` с кэшем на 4096 записей и generation-инвалидацией);
  пять ключей `OEChatsRegexFilters*` (`Enabled`, `Data`, `EnableInChats`,
  `ShowHidden`, `ExcludedDialogs`) и алиасы `regexFiltersEnabled`/
  `regexFiltersData`/`enableInChats`/`excludedDialogs`; гейт в `pushItem`
  (`history/view/history_view_list_widget.cpp`) не пускает скрытое сообщение
  в вид, исключённые диалоги пропускаются по bare id, смена настроек
  подхватывается естественным refresh (превью, read-till и бары не тронуты).
  Три тумблера в Chats, редактора списка нет (CLI и импорт `.extera`), четыре
  lang-ключа, CLI 5 команд; формат данных — Android `FilterModel` (`regex`,
  `caseInsensitive`, `reversed`, `enabled`, сверено по `AyuFilter.java` и
  `NaConfig.kt`); `check_regex_filters` — 27 проверок (группа `cli` не
  росла — 61; счётчик выше 925, backup 102, плагины 32 и 213 без изменений);
  `.extera` — 135 ключей (130 + 3 + 2, сверено с legacy-кортежем `run_checks`);
  `kKeysCount` 11226 (`kTagsCount` 185); sha256 патча `dfee564c…`,
  `nix flake check` PASS. Ограничения: бара «N скрыто» нет, `showHidden` —
  первая итерация, `excludedDialogs` без UI, превью-фильтра нет,
  ReDoS-защита прикладная (валидация, `optimize()`, кэш), RE2 не подключён,
  `MultilineOption` не включён — `^`/`$` матчат весь текст. Xvfb-смоук не
  выполнялся.
- Речь / Vosk-заглушка (NB-51): `OEGeneralRecognitionLanguage` (строка,
  default `ru`; пустое значение → `ru`) и алиас `recognitionLanguage` хранят
  язык распознавания — санитайзер подставляет `ru` только для пустой строки,
  список языков не проверяется. В Exteraless Chats пункт «Speech recognition
  language» открывает `Ui::Radioenum` из шести языков (ru, en, de, fr, es,
  it) и пишет код в конфиг; в контекстном меню голосовых пункт «Transcribe
  offline» показывает тост `lng_media_view_transcribe_offline_na` —
  распознавания нет, это заглушка. Два lang-ключа
  (`lng_settings_exteraless_recognition_language`,
  `lng_media_view_transcribe_offline_na`); CLI 2 режима
  (`recognition-language`, `recognition-language-set <code>`), код при
  установке тримится и приводится к нижнему регистру, пустой даёт usage;
  `check_recognition` — 6 проверок (5 + 1 на неверный вызов CLI, группа
  `cli` не росла — 61; счётчик выше 897, backup 101, плагины 32 и 213 без
  изменений); `.extera` — 130 ключей (125 + 3 + 2, сверено с
  legacy-кортежем `run_checks`); `kKeysCount` 11222 (`kTagsCount` 185);
  sha256 патча `90e76178…`, `nix flake check` PASS. Ограничения: самого Vosk
  нет — в nixpkgs его нет, нужна деривация (или whisper/kaldi), поэтому
  офлайн-распознавания не существует; Xvfb-смоук не выполнялся. Follow-up:
  подключить Vosk/whisper, скачивание моделей, серверная транскрипция как
  альтернатива и валидация кода языка в CLI (сейчас принимается любая
  непустая строка).
- AI-ассистент, MVP (NB-48): `OEAiEndpoint` (строка, default
  `https://api.openai.com/v1`), `OEAiModel` (строка, default `gpt-4o-mini`) и
  `OEAiApiKey` (строка, default пусто; секрет — в `.extera`/бэкап не
  экспортируется, импорт принимает и `aiApiKey`) задают провайдера, алиасы
  `aiEndpoint`/`aiModel`/`aiApiKey`. Запрос собирает Qt-only модуль
  `exteraless/ai_client.{h,cpp}`: `ChatCompletionsUrl` дописывает
  `/chat/completions` (пустой endpoint → OpenAI-адрес), `BuildRequestJson`
  даёт OpenAI-совместимое тело (`model`, `messages` с `user` и необязательным
  `system`, `stream: false`), `ParseResponse` берёт первый непустой
  `choices[].message.content` и текст ошибки из `error.message`/`error.type`;
  сети модуль не делает. В Exteraless Chats пункт «AI assistant» открывает
  бокс с полями Endpoint, Model и маскированным полем ключа
  (`CloudPassword::AddPasswordField`), четыре lang-ключа
  `lng_settings_exteraless_ai*`; в контекстном меню текстовых сообщений пункт
  «Ask AI» открывает `Ui::AiRequestBox` (`boxes/ai_request_box.{h,cpp}`) —
  поле запроса, строка ответа и кнопки Send/Copy/Close, ключ читается при
  каждой отправке (пустой → тост, запрос не уходит), сеть только здесь (свой
  `QNetworkAccessManager`). CLI 5 режимов (`ai-config`,
  `ai-config-set-endpoint`, `ai-config-set-model`, `ai-config-set-key`,
  `ai-request-json`), ключ в выводе не печатается (только `hasKey`);
  `check_ai` — 23 проверки, группа `cli` не росла (61; счётчик выше 891,
  backup 101, плагины 32 и 213 без изменений); `.extera` — 129 ключей
  (124 + 3 + 2, сверено с legacy-кортежем `run_checks`); `kKeysCount` 11220
  (`kTagsCount` 185); sha256 патча `8e87990a…`, `nix flake check` PASS.
  Ограничения: ключ лежит открытым текстом в `tdata/exteraless.json` —
  только маскировка в UI/CLI и исключение из экспорта, OS-keychain нет; текст
  сообщения уходит на произвольный endpoint без согласия в UI; запрос идёт
  мимо прокси Telegram (свой `QNetworkAccessManager`); Xvfb-смоук не
  выполнялся. Follow-up: стриминг, роли/промпты, локальная история, вставка
  ответа цитатой, замена редактора и саммари не сделаны.
- Стриминг AI-ассистента (NB-48, follow-up MVP): разбор SSE живёт в Qt-only
  `exteraless/ai_client.{h,cpp}` — `SseParser::feed` копит чанки в буфер и
  режет по `\n` (терпит `\r` и разрыв строки между чанками), `isDone()`
  ставит флаг на `data: [DONE]`, `reset()` очищает состояние;
  `ParseStreamEvent` отдаёт `delta.content` из `choices[]`, а строки без
  `data:`, битый JSON и `error`-события молча отбрасывает. В
  `boxes/ai_request_box.cpp` тело собирается локально с `stream: true` (мимо
  `BuildRequestJson`), ответ дописывается по `readyRead`, `[DONE]` обрывает
  запрос и финализирует текст, `ParseResponse` работает fallback'ом для
  нестримового ответа, а закрытие бокса (Close, Esc, клик вне) через
  деструктор `State` делает `cancel()` и гасит запрос. CLI — два режима
  (`ai-stream-parse`, `ai-stream-chunk`), три фикстуры, `check_ai_stream` —
  13 проверок; chats-раннер 1058 (1045 + 13), плагины 32 и host-раннер 252
  без изменений, backup 105; `kKeysCount` 11241 без изменений (новых
  lang-ключей нет), sha256 патча `0d45e9dc…`, `nix flake check` PASS.
  Ограничения: runtime SSE на живом endpoint не проверялся (только синтаксис
  и CLI); ошибка в середине потока не видна — у парсера нет error-канала;
  Xvfb-смоук не выполнялся. Follow-up: роли/промпты, локальная история,
  замена редактора, саммари и параметр `stream` в `BuildRequestJson`.
- Провайдеры перевода (NB-49): `OETranslationProvider` (int `0..3`, default
  `0`: `0` — auto/upstream, `1` — Google, `2` — DeepL, `3` зарезервирован под
  LLM из NB-48) выбирает провайдера, `OETranslationGoogleCloudKey` и
  `OETranslationDeepLKey` (строки, секреты — в `.extera`/бэкап не
  экспортируются, импорт старых бэкапов принимает) хранят ключи, а
  `OETranslationDeepLFormality` (int `0..2`, default `0`; `1` — more, `2` —
  less) — формальность DeepL; алиасы `translationProvider`/
  `googleCloudTranslateKey`/`deepLTranslateKey`/`deepLFormality`. Диспетчер
  `CreateTranslateProvider` (`lang/translate_provider.cpp`) возвращает
  выбранного провайдера до существующей цепочки (URL-шаблон → MTProto), а
  `0` оставляет upstream как есть. Google — `GET` на
  `translation.googleapis.com/language/translate/v2` с `format=text`, разбор
  `data.translations[].translatedText` и unescape HTML-entities (`&quot;`,
  `&#39;`, `&lt;`, `&gt;`, `&amp;`); DeepL — `POST` на
  `api-free.deepl.com/v2/translate` с `Authorization: DeepL-Auth-Key`,
  `formality=more|less` (при `0` параметра нет), Pro-эндпоинт вне scope.
  В Exteraless Chats пункт «Translation provider» (Auto/Google/DeepL),
  «DeepL formality» и «Translation API keys» с двумя маскированными полями
  (`CloudPassword::AddPasswordField`), шесть lang-ключей
  `lng_settings_exteraless_translation_*`; CLI 6 режимов
  (`translation-provider`/`-set`, `translation-keys`/`-set`,
  `translation-formality`/`-set`), ключи в выводе — первые 4 символа и
  `****`; `check_translation` — 32 проверки (27 + 5 на неверный вызов CLI,
  группа `cli` не росла — 61; счётчик выше 868, backup 101, плагины 32 и 213
  без изменений); `.extera` — 127 ключей (122 + 3 + 2, сверено с
  legacy-кортежем `run_checks`); `kKeysCount` 11216 (`kTagsCount` 185);
  sha256 патча `897e8192…`, `nix flake check` PASS. Ограничения: секреты
  лежат plaintext в `tdata/exteraless.json` — только маскировка в CLI и
  исключение из экспорта, OS-keychain нет; текст уходит на сторонние серверы
  Google и DeepL, согласия в UI не спрашивают; ошибки дают общий тост
  (`TranslateProviderError` не расширялся); LLM-провайдер (`3`) — NB-48;
  DeepL Pro-эндпоинт не поддерживается; `boxes/translate_box.cpp` не
  менялся; Xvfb-смоук не выполнялся.
- Радиус секций и отдельные заголовки (NB-64): `OEChatsSectionRadius`
  (int `-1..16`, default `-1`) задаёт радиус подложки секции настроек, а
  `OEChatsSectionSeparateHeaders` (bool, default `false`) выносит заголовки
  наружу. `SectionBuilder` строит `SectionCard` (`Ui::OverrideMargins`,
  `QMargins(radius, 0, radius, 0)`, заливка `st::windowBgOver` в
  `paintEvent`), поэтому содержимое секции лежит на скруглённой подложке;
  карточка создаётся лениво в `body()` при первом контроле, а при
  `separateHeaders` `addSubsectionTitle` кладёт заголовок в `_outer` вне
  карточки и сбрасывает `_body`, так что следующая секция получает свою
  подложку. Путь поиска (`SearchContext`) карточек не создаёт. Алиасы
  `sectionRadius`/`SectionRadius` (double, `lround` и кламп `-1..16`) и
  `separateHeaders`/`SeparateHeaders` (bool); в Chats пункт «Section radius» с
  `Ui::Radioenum` Upstream/Square/Rounded 4/8/12 и тумблер «Separate headers»,
  четыре lang-ключа; CLI `section-radius`, `section-radius-set`,
  `separate-headers`, `separate-headers-set`, `check_section_radius` —
  11 проверок (счётчик выше 836, CLI 61, backup 101, плагины 32 и 213 без
  изменений); `.extera` — 125 ключей (120 + 3 + 2, сверено с legacy-кортежем
  `run_checks`); `kKeysCount` 11210 (`kTagsCount` 185); sha256 патча
  `90c40747…`, `nix flake check` PASS. Ограничения: вертикальные отступы
  карточки нулевые, поэтому hover-подсветка строк касается её краёв; радиус
  боксов (`lib_ui`) не затронут — это вне Nix-патча; 51 прямой вызов
  `AddSubsectionTitle` вне билдера остаётся внутри карточки; режим применяется
  после перестройки страницы; Xvfb-смоук не выполнялся.
- Тулбар форматирования (NB-89, вторая половина):
  `OEChatsShowFormattingButtons` (bool, default `false`, алиас
  `showFormattingButtons`) показывает над полем ввода ряд из восьми
  `Ui::LinkButton` — B, I, U, S, `</>`, `⊘`, `❝` и ссылка. Теговые кнопки
  вызывают `Ui::InputField::toggleCurrentMarkdownTag` только при
  `markdownEnabledState().enabledForTag(tag)`, последняя —
  `editCurrentMarkdownLink`; у кнопок `Qt::NoFocus`, accessible name из
  `lng_menu_formatting_*`, шаг — `st::defaultLinkButton.font->height / 2`.
  Видимость пересчитывает `updateFormattingButtons` (вызовы из
  `initKeyHandler` и `updateControlsVisibility`), высота ряда входит в
  `updateHeight`, а `updateControlsGeometry` поднимает шапку на его высоту.
  Тумблер «Show formatting buttons» в Chats, один lang-ключ
  `lng_settings_exteraless_chats_show_formatting_buttons`, CLI
  `formatting-buttons`/`formatting-buttons-set`, `check_formatting_buttons` —
  7 проверок, две из них на неверный вызов CLI (счётчик выше 825, CLI 61,
  backup 101, плагины 32 и 213 без изменений); `.extera` — 123 ключа
  (118 + 3 + 2, сверено с legacy-кортежем `run_checks`), экспортируется как
  `showFormattingButtons`, импорт принимает и
  `OEChatsShowFormattingButtons`; `kKeysCount` 11206 (`kTagsCount` 185);
  sha256 патча `e81c0789…`, `nix flake check` PASS. Ограничения: ряд
  появляется при следующем обновлении видимости контролов, перезапуск не
  нужен; глифы `⊘` и `❝` — компромисс `LinkButton` (иконок нет);
  markdown-парсер и подсветка синтаксиса не делались; Xvfb-смоук не
  выполнялся.
- Стили шапки чата (NB-63): `OEChatsChatHeaderStyle` (int `0..2`, default `0`;
  `0` — upstream, `1` — iOS-центровка, `2` — M3) центрирует имя и статус шапки
  внутри доступной ширины (`headerAvail` — `width()` минус `_leftTaken` и
  `_rightTaken`): `nameleft` считается от `headerAvail`, а `statusleft` и
  `statuswidth` выставляет `centerStatus`, поэтому `paintConnectingState`,
  `paintSendAction` и `paintStatus` получают сдвинутые координаты.
  `getMembersShowAreaGeometry` повторяет расчёт для клика по статусу. Режим
  `2` берёт `M3TitleStyle()` — static-копию `st::msgNameStyle` с шрифтом
  `st::topBarM3TitleFont` (`font(17px semibold)` в `info/info.style`), тот же
  шрифт напрямую видит заголовок папки/replies, а смена стиля перестраивает
  `_title` по `_title.style()`. Узкий режим и высота шапки не тронуты.
  Алиасы `iosChatHeader` (bool → `1`/`0`) и `newChatHeaderStyle`
  (int `0..2`, bool → `2`/`0`); в Chats пункт «Chat header style» с
  `Ui::Radioenum` Upstream/iOS/M3 и четыре lang-ключа
  `lng_settings_exteraless_chats_header_style*`; CLI `header-style` и
  `header-style-set`, `check_header_style` — 8 проверок плюс одна на неверный
  вызов CLI (счётчик выше 811, CLI 61, backup 101, плагины 32 и 213 без
  изменений); `.extera` — 121 ключ (116 + 3 + 2, сверено с legacy-кортежем
  `run_checks`), `kKeysCount` 11204 (`kTagsCount` 185) — в счётчике остаётся
  retired-слот, чистая генерация дала бы 11203; sha256 патча `0dd9f6f7…`,
  `nix flake check` PASS. Ограничения: статус typing/connecting может слегка
  съезжать при центровке; в replies-чате бара нет, поэтому режим там не
  проявляется; Xvfb-смоук не выполнялся.
- Спойлеры сразу (NB-89, первая половина): `OEChatsShowSpoilersDirectly` (bool,
  default `false`, алиас `showSpoilersDirectly`) раскрывает текстовые
  спойлеры без нажатия. В `InitElementTextPart`
  (`history/view/history_view_text_helper.cpp`) режим добавляет
  `setSpoilerRevealed(true, anim::type::instant)` и пропускает установку
  `setSpoilerLinkFilter`, а превью в списке чатов делает то же для
  `_textCache` в `MessageView::prepare` (`dialogs/ui/dialogs_message_view.cpp`).
  Медиа гейтит `EffectiveSpoiler` (`data/data_media_types.cpp`) на входе
  `MediaPhoto::createView` и `MediaFile::createView`: спойлер снимается только
  у обычного пользовательского медиа, а TTL, `isMediaSensitive()` и платное
  медиа (`invoice()->isPaidMedia`) его сохраняют, `Media::hasSpoiler()` не
  тронут (меняется лишь `hasSpoilerForPreview()`). Тумблер «Show spoilers
  without tapping» в Chats, один lang-ключ
  `lng_settings_exteraless_chats_show_spoilers_directly`, CLI `spoilers-direct`
  и `spoilers-direct-set`, `check_spoilers_direct` — 7 проверок, две из них на
  неверный вызов CLI (счётчик выше 818, CLI 61, backup 101, плагины 32 и 213
  без изменений); `.extera` — 122 ключа (117 + 3 + 2, сверено с legacy-кортежем
  `run_checks`), экспортируется как `showSpoilersDirectly`, импорт принимает и
  `OEChatsShowSpoilersDirectly`; `kKeysCount` 11205 (`kTagsCount` 185);
  sha256 патча `cfa7a617…`, `nix flake check` PASS. Ограничения: спойлер
  раскрывается после перестройки текста, а не мгновенно — открытый чат
  обновится на ближайшем событии; счётчики и превью чувствительного, платного
  и TTL-медиа остаются как в upstream; Xvfb-смоук не выполнялся..
- Мини-аватарки отправителей (NB-62): `OEChatsSenderMiniAvatars` (bool,
  default `false`, алиас `senderMiniAvatars`) рисует мини-аватарку отправителя
  в превью диалога. Пейнт — в `PaintRow` (`dialogs/ui/dialogs_layout.cpp`) перед
  `paintItemCallback`: `sender` берётся как `item->displayFrom()` при предикате
  (превью не скрыто, автор поста не спрятан, отправитель не обычный
  пользователь, кроме гостевого бота), а отрисовка требует места под аватарку —
  `itemWidth` не меньше `skip` плюс `st::dialogsTextWidthMin`; затем сдвигаются
  `itemLeft` и `itemWidth`. Кэш `PeerUserpicView` держит `BasicRow`
  (`dialogs/dialogs_row.h`), сброс — при смене пира. Style-ключи:
  `dialogsSenderMiniAvatar` (16 px), `...Skip` (2 px), `...Top` (1 px) и
  `...Right` — объявлен, не используется (аватарка привязана к левому краю
  имени). Тумблер «Show mini avatars next to sender names» в Chats, один
  lang-ключ `lng_settings_exteraless_chats_sender_mini_avatars`, CLI
  `sender-mini-avatars`/`sender-mini-avatars-set`, `check_sender_mini_avatars` —
  5 проверок плюс одна на неверный вызов CLI (счётчик выше 802, CLI 60, backup
  101, плагины 32 и 213 без изменений); `.extera` — 120 ключей (115 + 3 + 2,
  сверено с legacy-кортежем `run_checks`), `kKeysCount` 11200 (`kTagsCount`
  185), sha256 патча `105a17c7…`, `nix flake check` PASS. Ограничения:
  применяется при следующей отрисовке строки — принудительного репейнта нет;
  фото отправителя подтягивается при первом показе, до этого рисуются инициалы;
  у постов со скрытым автором и при `hideSender` аватарки нет; Xvfb-смоук не
  выполнялся.
- Канал обновлений (NB-54): `OEGeneralAutoUpdateChannel` (int `0..2`, default
  `1`; `0` — off, `1` — release, `2` — beta, значение клампится) задаёт канал
  релизов, алиас `autoUpdateChannel`. Разбор списка релизов — Qt-only модуль
  `exteraless/update_check.{h,cpp}`:
  `Exteraless::ParseReleasesJson(data, current, includePrerelease)` берёт самый
  новый подходящий тег (draft отбрасывается всегда, prerelease — только при
  `includePrerelease`, тег не новее `current` пропускается, без `html_url`
  остаётся страница releases), `ReleasesApiUrl()` возвращает адрес API
  releases; сам модуль сети не делает — загрузку оставляет вызывающему.
  В Exteraless General пункт «Update channel» с `Ui::Radioenum`
  Off/Release/Beta и четыре lang-ключа
  `lng_settings_exteraless_update_channel` (`_off`, `_release`, `_beta`).
  `.extera` — 119 ключей (114 + 3 + 2, сверено с legacy-кортежем
  `run_checks`), CLI `update-channel`, `update-channel-set`, `update-check`
  (разбор без сети), `check_update_channel` — 19 проверок (16 + 3 на неверный
  вызов CLI; счётчик выше 796, backup 101, плагины 32 и 213 без изменений);
  `kKeysCount` 11199 (`kTagsCount` 185), sha256 патча `a44a72bb…`,
  `nix flake check` PASS. Ограничения: автозагрузки и установки нет — объём
  ограничен нотификатором (проверка версии и ссылка на страницу releases),
  но потребителя в UI пока нет: парсер вызывается только из CLI, сети в
  модуле нет, релизов у форка пока 0, поэтому живой ответ API не проверялся;
  `ReleasesPage()`/`kReleasesApi` — константы, другие хосты и зеркала не
  поддерживаются; Xvfb-смоук не выполнялся.
- Persian calendar (NB-58): `OEGeneralUsePersianCalendar` и
  `OEGeneralPersianCalendarLatin` (обе bool, default `false`) переводят даты на
  Solar Hijri. Строку собирает
  `Exteraless::PersianDateText(date, latin, withYear)`
  (`exteraless/format.{h,cpp}`) через `QCalendar(QCalendar::System::Jalali)`:
  таблица 12 персидских месяцев с латинскими именами, цифры ASCII при `latin`
  и `U+06F0..U+06F9` иначе, невалидная дата даёт пустую строку. В
  `lang/lang_keys.cpp` подмена стоит в `langDayOfMonth`, `langDayOfMonthFull`,
  `langDayOfMonthShort`, `langDateTime` и `langDateTimeFull` — меняется только
  `lt_date`, время не тронуто. Алиасы `usePersianCalendar` и
  `displayPersianCalendarByLatin`. В Exteraless General два тумблера (второй
  доступен только при включённом первом) и два lang-ключа
  `lng_settings_exteraless_persian_calendar`/
  `lng_settings_exteraless_persian_calendar_latin`. `.extera` — 118 ключей
  (113 + 3 + 2, сверено с legacy-кортежем `run_checks`), CLI `persian-date`,
  `persian-calendar`, `persian-calendar-set`, `check_persian` — 15 проверок
  (счётчик выше 776, плагины 32 и 213 без изменений); `kKeysCount` 11195
  (`kTagsCount` 185), sha256 патча `daa23e68…`, `nix flake check` PASS.
  Ограничения: показывать ли год, решает григорианский год, а персидский
  меняется на Наврузе — около смены персидского года метка года может
  отсутствовать; RTL и порядок не переупорядочиваются, строка отдаётся готовой;
  вендорные `lng_month*` персидским путём не используются; смена настройки
  видна после переоткрытия чата; Xvfb-смоук не выполнялся.
- Media quality and formats (NB-88): `OEGeneralDefaultVideoQuality` (int
  `0..2`, default `0`) — стартовое HLS-качество: `0` оставляет upstream 480p,
  `1` — 720p, `2` — 1080p (`initialPlaybackVideoQuality` в
  `media/view/media_view_overlay_widget.cpp`, `Settings::videoQuality()` в
  `core/core_settings.cpp`); `OEGeneralHardwareAcceleration` (bool, default
  `true`) — алиас поверх upstream-настройки: включённое декодирование требует
  `hardwareAcceleratedVideo() && hardwareAcceleration()` в
  `restartAtSeekPosition` оверлея и PiP, а тумблер пишет оба значения;
  `OEChatsTakeGifAsVideo` (bool, default `false`) отменяет конверсию GIF в
  `video/mp4` в `FileLoadTask::process`
  (`storage/localimageloader.cpp`); `OEGeneralCustomAudioBitrate` (int
  `16/24/32/48`, default `32`, иное сводится к `32`) задаёт `bit_rate` записи
  Opus в `InitializeFFmpeg` (`media/audio/media_audio_capture.cpp`), при отказе
  `avcodec_open2` поток один раз переоткрывается на 32 kbps. Алиасы
  `defaultHlsVideoQuality`, `playerDecoder`, `takeGIFasVideo`,
  `customAudioBitrate`. В Exteraless подсекция «Media»: селектор качества
  Auto/720p/1080p, тумблер Hardware video decoding, тумблер Send GIFs as video
  и селектор 16/24/32/48 kbps; шесть lang-ключей
  `lng_settings_exteraless_media*`. `.extera` — 116 ключей (111 + 3 + 2,
  сверено с legacy-кортежем `run_checks`), CLI четыре группы
  (`media-quality`, `hw-accel`, `gif-as-video`, `audio-bitrate`),
  `check_media_quality` — 19 проверок (счётчик выше 761, плагины 32 и 213 без
  изменений); `kKeysCount` 11193 (`kTagsCount` 185), sha256 патча `c647f76d…`,
  `nix flake check` PASS. Ограничения: ручной тир отключает адаптивное
  переключение — на слабой сети просмотр может ухудшиться; повышенный битрейт
  может упасть на `avcodec_open2`, тогда запись идёт на 32 kbps без сообщения;
  GIF-как-видео теряет no-sound-спойлер; MP4-как-видео не делалось — уже дефолт
  upstream, `enhancedVideoBitrate`/`alwaysSendInHD`/`preferredOriginalQuality`
  — отдельные ключи; Xvfb-смоук и сетевой замер не выполнялись.
- Avatar corners (NB-61): `OEChatsAvatarRadius` (int `-1..32`, default `-1`;
  `-1` — круг как в upstream, `0` — квадрат, `N` — радиус в px) задаёт форму
  загруженного и пустого юзерпика. Маска выбирается в `Ui::ValidateUserpicCache`
  (`ui/userpic_view.cpp`): при `radius >= 0` — `Images::Round` и
  `EmptyUserpic::paintRounded`, иначе `Images::Circle`/`paintCircle`; радиус
  входит в ключ кэша (`PeerUserpicView::radius`), поэтому смена значения
  перегенерирует картинку. Алиасы: `avatarCorners`/`singleCornerRadius` —
  абсолютный радиус (кламп `-1..32`), `avatarSquareness` (0..1) —
  `round(32 * (1 - q))`. В Chats пункт «Avatar corners» с `Ui::Radioenum`
  Circle/Square/Rounded 4/8/12/16/24 и строкой для произвольного значения;
  четыре lang-ключа `lng_settings_exteraless_chats_avatar_corners*`
  (`_rounded` с `lt_radius` и `QString::number`). `.extera` — 112 ключей, CLI
  `avatar-corners`/`avatar-corners-set`, `check_avatar_corners` — 7 проверок
  (счётчик выше 742, CLI 58, плагины 32 и 213 без изменений). `kKeysCount`
  11187, sha256 патча `b58e4a17…`, `nix flake check` PASS (прогон из батча,
  повторно не запускался). Ограничения: форма применяется при следующей
  отрисовке, уже нарисованные аватарки не меняются мгновенно;
  `Data::Peer::GenerateUserpicImage` и `UserpicButton` (`useForumShape`)
  остались круглыми; онлайн-точка не тронута; Xvfb-смоук не выполнялся.
- Profile buttons (NB-73): `OEChatsHideGiftButton`, `OEChatsHideCallButton` и
  `OEChatsHideChannelSearchButton` (все default `false`) убирают кнопки Gift и
  Call в профиле (`info/profile/info_profile_top_bar.cpp`), Call — в шапке
  чата, Search — в шапке канала
  (`history/view/history_view_top_bar_widget.cpp`). Android-алиасы
  `hideGiftButtonInChannel`/`hideGiftButton`, `hideCallButton`
  и `hideChannelSearchButton` принимаются в `.extera` (всего 91 ключ: 86 в
  `exteraconfig`, 3 в `pillstackconfig`, 2 в `mainconfig`), три тумблера — в
  Chats, три lang-ключа в `lang.strings`, CLI `profile-buttons` и
  `profile-buttons-set`; `check_profile_buttons` — 5 проверок (счётчик выше
  582, CLI 45, плагины 32 и 213 без изменений). `kKeysCount` 11145, sha256
  патча `486d82fd…`, `nix flake check --no-build` PASS. Money и Share
  отдельных кнопок в профиле desktop не имеют — N/A (аналог `hideCameraTile`).
  Ограничения: значение применяется после переоткрытия профиля или чата,
  `Ctrl+F` и меню «⋯» продолжают работать, `_groupCall` не трогали,
  GUI-смоук под Xvfb не выполнялся.
- Zalgo filter (NB-08): `OEChatsZalgoFilter` (default `false`) вычищает
  «zalgo»-текст из сообщений; `exteraless/zalgo.{h,cpp}` режет серию из
  четырёх и более комбинирующих знаков (`Mark_NonSpacing`,
  `Mark_SpacingCombining`, `Mark_Enclosing`), сохраняет суррогатные пары и не
  трогает текст короче 4 и длиннее 2048 символов. Вставка —
  `Element::setTextWithLinks` (`history/view/history_view_element.cpp`) с
  пересчётом entities по карте pangu-стиля: обнулившиеся отбрасываются,
  остальные сдвигаются и обрезаются. Android-алиас `zalgoFilter` принимается в
  `.extera` (всего 87 ключей, экспорт пишет `zalgoFilter`), тумблер «Filter
  Zalgo text» — в Chats, CLI `zalgo`, `zalgo-set`, `zalgo-apply` и
  `zalgo-tags`; `check_zalgo` — 17 проверок (счётчик выше 582, плагины 32 и 213
  без изменений). `kKeysCount` 11141, sha256 патча `4d44da74…`,
  `nix flake check --no-build` PASS. Ограничения: матры арабского и индийских
  письменностей серией 4+ тоже вырезаются (паритет Android); в открытом чате
  фильтр применяется при переоткрытии, live-сигнала нет; GUI-смоук под Xvfb не
  выполнялся, БД фильтр не меняет — правка только на отрисовке.
- Hide Stories header (NB-10): `OEChatsHideStoriesHeader` (default `false`)
  скрывает полосу историй над списком чатов в `Widget::updateStoriesVisibility`
  (`dialogs/dialogs_widget.cpp`) тем же путём, что и поиск или свёрнутая
  панель. Android-алиасы `hideStoriesFromHeader` и `HideStoriesFromHeader`
  принимаются в `.extera` (всего 85 ключей, экспорт пишет
  `hideStoriesFromHeader`), тумблер добавлен в Chats, CLI `storiesheader` и
  `storiesheader-set` (дефолт, сеттер, fromJson, бэкап); отдельных проверок
  под этот CLI в `run_checks.py` пока нет, поэтому счётчик выше остаётся 582.
  Live-сигнала нет: ключ читается при следующем пересчёте видимости (возврат
  из настроек, показ окна, рестарт). Xvfb-смоук GUI-бинарника после
  PCH-пересборки не выполнялся: ожидается, что полоса исчезает и строки
  списка не сдвигаются; `out/bin/Telegram` от 02:15 собран до правок NB-10.
- ID and DC в профиле (NB-09): `OEGeneralShowIdDc` (default `false`) добавляет
  строку «ID and DC» (`id: 123456 · DC 2`) в профиль контакта и в свой профиль
  (Settings Information). Текст — `IdDcText`
  (`info/profile/info_profile_values.cpp`), формат — `Exteraless::FormatIdDc`
  (`exteraless/format.cpp`), вывод — `makeInfo()`
  (`info/profile/info_profile_actions.cpp`) и `SetupRows()`
  (`settings/sections/settings_information.cpp`); по клику копируется
  `bareId` без префикса чата. Строка подавлена, если включён upstream-тумблер
  «Show peer ID below about». Android-алиасы `showIdAndDc` (bool) и
  `idDcType` (число) принимаются в `.extera` (всего 86 ключей), тумблер — в
  General, CLI `id-dc`/`id-dc-set`/`id-dc-format` (без отдельных проверок в
  `run_checks.py`, поэтому счётчик выше остаётся 582). Значение не реактивно —
  строка появляется при следующем открытии профиля; DC — текущего аккаунта,
  а не собеседника. Xvfb-план: профиль контакта и свой, строка видна,
  `bareId` копируется; GUI-смоук не выполнялся, `out/bin/Telegram` от 02:15
  собран раньше правок NB-09.
- Sticker size (NB-69, только размер): `OEChatsStickerSize` (int, 4..20,
  default `14`) меняет размер стикеров в `Sticker::Size()`
  (`history/view/media/history_view_sticker.cpp`): множитель
  `1 + (v - 14) / 15` от базы 224 px даёт 75 px при `4` и 314 px при `20`,
  результат клампится в 50..336 px. При `14` работает upstream-путь с
  `OptionStickerSize`. Android-алиас `stickerSize` (дробное округляется)
  принимается в `.extera` (всего 88 ключей, экспорт пишет
  `OEChatsStickerSize`), слайдер «Sticker size» 4..20 — в Chats, CLI
  `sticker-size`/`sticker-size-set`, `check_sticker_size` — 10 проверок
  (счётчик выше 582, плагины 32 и 213 без изменений). `kKeysCount` 11142,
  sha256 патча `c47276be…`, `nix flake check --no-build` PASS. Размер
  читается при открытии чата, поэтому применяется при его переоткрытии —
  live-сигнала нет; GUI-смоук под Xvfb не выполнялся, `out/bin/Telegram`
  (02:15) собран раньше правок NB-69.
- Sticker shape (NB-69): `OEChatsStickerShape` (int, `0..2`, default `0`)
  задаёт форму стикеров: `0` — квадрат, как в upstream, `1` — скругление
  4 px (`Ui::MsgFileThumbRadiusSmall`), `2` — «bubble»
  (`Ui::BubbleRadiusLarge`). Клип — `QPainterPath` + `setClipPath` в
  `Sticker::paintAnimationFrame`, `paintPixmap` и `paintPath`
  (`history/view/media/history_view_sticker.cpp`) по фактическому rect
  отрисовки; при `0` клипа нет, `customEmojiPart()`/`emojiSticker()` его
  пропускают, поэтому inline custom emoji и emoji-стикеры не трогаются.
  Android-алиас `stickerShape` (дробное округляется) принимается в
  `.extera` (всего 93 ключа), выбор «Sticker shape» (Square/Rounded/Bubble) —
  на `Ui::Radioenum` в Chats, четыре lang-ключа в `lang.strings`, CLI
  `sticker-shape`/`sticker-shape-set`, `check_sticker_size` — 21 проверка
  в группе `sticker-size` (счётчик выше 593, плагины 32 и 213 без
  изменений). `kKeysCount` 11149, sha256 патча `9b497121…`,
  `nix flake check --no-build` PASS. NB-69 закрыт полностью (размер и
  форма). Не покрыто: premium-effect и панель стикеров — там клипа нет;
  dice клипуются сознательно. GUI-смоук под Xvfb не выполнялся.
- Poll results before voting (NB-71): `OEChatsShowResultsBeforeVoting` (default
  `false`) показывает результаты опроса до голосования. Предикат
  `Poll::showResultsBeforeVote()` (`history/view/media/history_view_poll.cpp`)
  отдаёт результаты только для публичного опроса без голоса и без скрытых
  результатов, где `totalVoters > 0` и есть ответ с голосами; admin-режим,
  `Context::MediaEditor`, уже отданный голос, `Closed`, `HideResultsUntilClose`,
  `MultiChoice`, `totalVoters <= 0` и пустые `votes` предикат отсекает.
  `showVotes()` учитывает его, а `canVote()` смягчён до
  `!showVotes() || showResultsBeforeVote()`, поэтому голос по-прежнему
  сохраняется. Android-алиас `showResultsBeforeVoting` — в
  `.extera` (всего 93 ключа: 88 + 3 + 2, сверено с legacy-кортежем
  `run_checks.py`), тумблер «Show poll results before voting» — в Chats,
  lang-ключ `lng_settings_exteraless_chats_show_results_before_vote`, CLI
  `show-results-before-voting`/`show-results-before-voting-set`,
  `check_show_results_before_voting` — 7 проверок (счётчик выше 600, плагины
  32 и 213 без изменений). `kKeysCount` 11150, sha256 патча `39c78761…`,
  `nix flake check --no-build` PASS. Ограничения: multi-choice вне объёма,
  результаты применяются после переоткрытия или обновления опроса, сервер
  может не отдать счётчики (`hide_results_until_close` скрыт всегда),
  GUI-смоук под Xvfb не выполнялся, `out/bin/Telegram` (02:15) собран раньше
  правок NB-71.
- Reply style (NB-70): `OEChatsReplyColors`, `OEChatsReplyEmoji` и
  `OEChatsReplyBackground` (все default `true`) — выключатели поверх текущего
  поведения: при `false` цитата ответа рисуется как в upstream. Отрисовка —
  `history/view/history_view_reply.cpp`; `QuotePaintCache` у каждой цитаты
  свой, общий кэш не мутируется, отдельный guard для custom emoji не нужен —
  эмодзи блока свои. Legacy-алиас `messageColoredBackground` сводится к
  `replyBackground` и отключает окраску только при явном `false`; алиасы
  `replyColors`/`replyEmoji`/`replyBackground` принимаются в `.extera` (всего
  96 ключей: 91 + 3 + 2). Три тумблера — в подсекции Replies (Chats), четыре
  lang-ключа в `lang.strings`, CLI `reply-style`/`reply-style-set`,
  `check_reply_style` — 7 проверок (счётчик выше 607, плагины 32 и 213 без
  изменений). `kKeysCount` 11154, sha256 патча `27098fd7…`,
  `nix flake check --no-build` PASS. Ограничения: медиа-карточки и ручные
  цитаты остаются цветными — осознанно; применяется после переоткрытия чата
  (ширина цитаты считается от `_hasBackgroundEmoji`), GUI-смоук под Xvfb не
  выполнялся.
- Hide archive and sort by unread (NB-15): `OEChatsHideArchive` и
  `OEChatsSortByUnread` (обе default `false`) скрывают папку архива из списка
  чатов и поднимают чаты с непрочитанными наверх. `hideArchive` идёт по уже
  существующему skip-пути `_skipTopDialog` в
  `InnerWidget::refreshWithCollapsedRows` (`dialogs/dialogs_inner_widget.cpp`)
  с внешним условием `archiveInMainMenu() || hideArchive`;
  `shouldBeInChatList` не трогали — иначе счётчики папок уходят в минус.
  `sortByUnread` выставляет upstream-опцию `Dialogs::kOptionDialogsUnreadOnTop`
  (`dialogs/dialogs_entry.cpp`), конфиг засевает её в конструкторе
  `Application` (`core/application.cpp`), поэтому индексный список и
  `dialogs_indexed_list.cpp` не менялись. Android-алиасы
  `hideArchive`/`HideArchive`/`hideArchiveFolder` и
  `sortByUnread`/`SortByUnread` принимаются в `.extera` (всего 98 ключей:
  93 + 3 + 2, сверено с legacy-кортежем `run_checks.py`), два тумблера — в
  Chats, два lang-ключа в `lang.strings`, CLI
  `hide-archive`/`hide-archive-set`/`sort-unread`/`sort-unread-set`/
  `archive-skip`, `check_hide_archive_sort` — 13 проверок плюс две проверки в
  `backup` (счётчик выше 622, плагины 32 и 213 без изменений). `kKeysCount`
  11156, sha256 патча `24e1446f…`, `nix flake check --no-build` PASS.
  Ограничения: обе настройки применяются через `notifyListChanged`/пересорт,
  поэтому скрытие архива и новая сортировка видны сразу; поиск и окно Archive
  архив по-прежнему видят; при `hideArchive` и выключенном
  `archiveInMainMenu` вернуть архив можно только тумблером; GUI-смоук под
  Xvfb не выполнялся, `out/bin/Telegram` (02:15) собран раньше правок NB-15.
- Hide All Chats (NB-84): `OEChatsHideAllChatsFolder` (default `false`) скрывает
  папку «Все чаты» в левой панели фильтров при наличии пользовательских папок,
  активной становится первая доступная папка. CLI `hide-all-chats` (5 проверок).
  Xvfb: вкладка «All chats» скрыта, в сайдбаре осталась папка «IT», чаты
  отфильтрованы по «IT».
- Always show download icon (NB-75): `OEChatsAlwaysShowDownloadIcon` (default
  `false`) оставляет вкладку «Downloads» в поисковых подсказках и угловую
  иконку скачивания на уже загруженных аудиофайлах. CLI
  `always-show-download` (5 проверок). Xvfb: чип «Downloads» появляется сразу
  после включения тумблера; у играющего (загруженного) аудио иконка видна с
  включённой настройкой и исчезает после выключения; у незагруженного
  аудиофайла иконка на месте при обоих значениях.
- False bottom (NB-85): `OEPrivacyFalseBottom` (default `false`) добавляет в
  конец Exteraless скрытую секцию «Двойное дно»; при заданном локальном пароле
  она открывается только после его ввода (неверный пароль — ошибка, верный —
  перестройка страницы с «Локальный пароль»/«Заблокировать сейчас»). CLI
  `false-bottom` (5 проверок). Xvfb: без пароля секция открывается сразу и
  подсказывает установить его; с паролем — бокс ввода, неверный ввод даёт
  «Wrong passcode»; после верного секция появляется, «Заблокировать сейчас»
  сразу показывает экран блокировки; поиск «Lock now»/«Local passcode» не
  находит содержимое, пока секция закрыта, и находит после разблокировки.
- Chat decoration (NB-87): `OEAppearanceChatDecoration` (default `0`) рисует
  снежинки или фейерверк поверх шапки чата, либо выбирает снежинки по дате
  (`3`, декабрь/январь); слой прозрачен для мыши и не меняет геометрию шапки.
  CLI `decoration` (5 проверок). Xvfb: снежинки видны в шапке, «Off» убирает
  их, «Fireworks» рисует вспышки; значение сохраняется в `.extera`.
- Admin shortcuts (NB-82): `OEChatsShortcutAdministrators`,
  `OEChatsShortcutRecentActions`, `OEChatsShortcutStatistics`,
  `OEChatsShortcutPermissions`, `OEChatsShortcutMembers` (все default
  `false`) добавляют пункты в начало меню «…» профиля канала/группы для
  админов; Statistics скрыт без `CanGetStatistics`. CLI `shortcuts`
  (5 проверок). Xvfb: на тестовом канале меню показало Permissions,
  Administrators, Members, Recent actions; Recent actions открыл All actions,
  Administrators — список админов, Permissions — права участников, Members —
  подписчиков; тестовый канал удалён после проверки.
- Message menu (NB-83): `OEChatsGroupedMessageMenu` (default `false`)
  переносит Delete в конец меню сообщения, отделяя его разделителем (меню чата
  строится в `history/history_inner_widget.cpp`, а не в
  `history_view_context_menu.cpp`, — правки внесены в оба файла);
  `OEChatsDefaultDeleteMenu` (default `false`) запоминает выбор
  «удалить у всех / только у себя» автоматически. CLI `message-menu-style`
  (5 проверок). Xvfb: Delete внизу меню после «Select»; удаление с снятой
  галочкой «Also delete» привело к тому, что следующее окно удаления открылось
  с уже снятой галочкой.
- Link preview default (NB-74): `OEChatsDisableLinkPreviewByDefault` (default
  `false`) отключает автоматическое определение превью ссылок в поле ввода;
  ручное переключение остаётся по `Ctrl+R`. CLI `disable-link-preview`
  (5 проверок). Xvfb: при включённой настройке вставка ссылки не создаёт
  карточку превью над полем ввода.
- Chat menu (NB-81, частично): `OEChatsChatMenuBoost`, `OEChatsChatMenuLinked`,
  `OEChatsChatMenuViewDeleted`, `OEChatsChatMenuClearDeleted` (default `true`)
  управляют пунктами Boost, связанный чат, «Deleted messages» и «Clear deleted
  messages»; очистка теперь с подтверждением. CLI `chat-menu` (5 проверок).
  Xvfb: при выключенных Boost/Linked/ViewDeleted в меню остались остальные
  пункты и «Clear deleted messages». `ToBeginning`/`GoToMessage`/`HideTitle`/
  `DeleteOwnMessages`/закладки неприменимы — таких пунктов в desktop нет.
- Media viewer menu (NB-80): `OEMediaViewerMenuForward`,
  `OEMediaViewerMenuCopyFrame`, `OEMediaViewerMenuCopyPhoto`,
  `OEMediaViewerMenuSetProfile` (default `true`) скрывают соответствующие
  пункты меню; CLI `media-menu` (5 проверок). Xvfb: при выключенных ключах
  в меню фото остались только Go To Message, Delete, Save As..., View all
  photos. `NoQuoteForward`/`ScanQRCode` неприменимы — таких пунктов в
  desktop нет.
- Forward date (NB-67): `OEChatsShowForwardDate` (default `false`) дописывает
  `· дата время` к заголовку пересланного сообщения; CLI `forward-date`
  (5 проверок). Проверено логом: заголовок содержит
  «Forwarded from … · Oct 30 8:16:10 PM»; в узких медиа-бабблах хвост
  эллипсится, как и остальной текст заголовка.
- Edited mark (NB-68): `OEChatsUseEditedIcon` (глиф ✎) и
  `OEChatsCustomEditedMark` (до 16 символов) заменяют локализованное «edited»
  в строке сообщения; CLI `edited-mark` (6 проверок). Xvfb: метка «ed.»
  отрисовалась в сообщении канала.
- Seek duration (NB-72): `OEChatsSeekDuration` (5/10/15/30, default 5) задаёт
  шаг перемотки стрелками в медиавьюере, J/L — двойной шаг; Android-индекс
  `doubleTapSeekDuration` конвертируется в секунды. CLI `seek-duration`
  (8 проверок), Xvfb: селектор в Chats записывает 15 с.
- Wide channel posts (NB-13): `OEChatsWideChannelPosts` (default `false`)
  снимает лимит ширины баббла в broadcast-каналах — гейт в `Message` рядом с
  `UnlimitedMessageWidth` (`_bubbleWidthLimit` уходит в `0x3FFFFFF`), при
  этом megagroup отсекается через `isBroadcast()`, а медиа-сообщения не
  затрагиваются. Android-алиасы `wideChannelPosts`/`WideChannelPosts`
  принимаются в `.extera` (всего 99 ключей: 94 + 3 + 2), тумблер «Wide
  channel posts» — в Chats, lang-ключ
  `lng_settings_exteraless_chats_wide_channel_posts` в `lang.strings`, CLI
  `wide-posts`/`wide-posts-set`, `check_wide_posts` — 5 проверок (счётчик
  выше 627, плагины 32 и 213 без изменений). `kKeysCount` 11157, sha256
  патча `c5309705…`, `nix flake check --no-build` PASS. Широкие посты
  ленты — N/A: лент в desktop нет. Ограничения: ширина применяется после
  переоткрытия чата или ресайза, кап `maxWidth` общий с
  `UnlimitedMessageWidth`, GUI-смоук под Xvfb не выполнялся.
- Double tap расширенные действия (NB-91): резолвер понимает ещё шесть
  действий — translate 3, save 5, edit 8, delete 10, forward 11, download 12;
  6/7/9 дают «ничего», 2 остаётся «Send reaction» (расхождение с Android).
  Селекторы для всех шести в Chats, шесть lang-ключей `double_tap_*` в
  `lang.strings`, новых `OE`-ключей нет; CLI `doubletap-resolve` (14 кейсов),
  `doubletap` — 24 проверки, всего 629 PASS. `kKeysCount` 11163, sha256 патча
  `900ed89d…`, `nix flake check` PASS. Ограничения: клик-хендлеры
  перехватывают, delete через бокс, repeat/LLM N/A.
- Мелочи чата (NB-86): `OEChatsHideGreetingSticker` и
  `OEChatsHidePanelsOnScroll` (обе bool, default `false`) скрывают
  приветственный стикер (гейт в `GenerateChatIntro`) и закрывают панели при
  скролле (`hidePanelsAnimated` в `onScroll`); `OEChatsChannelBottomButton`
  (int 0/1/2, default `1`) — `0` скрыть, `1` mute, `2` Discuss (кнопка,
  навигация по `discussionLink`). Алиасы `disableGreetingSticker`,
  `hideKeyboardOnScroll`/`HideKeyboardOnChatScroll`, `bottomButton`/
  `BottomButton`, legacy `DisableChannelMuteButton` (true → 0); `.extera`
  102 ключа (99 + 3, сверено с legacy-кортежем `run_checks`), два тумблера и
  Radioenum Hide/Mute/Discuss, шесть lang-ключей, CLI 3 группы,
  `check_chat_misc` — 15 проверок, всего 649 PASS (backup 100). `kKeysCount`
  11169, sha256 патча `0dff2727…`, `nix flake check` PASS.
  `keepAttachButton` и `quickTransition*` — N/A (кнопки/жеста нет).
  Ограничения: panels закрывает и автокомплит (под Xvfb автоскролл не должен
  закрывать), bottom hide уносит gift/direct-иконки, применяется после
  переоткрытия, Xvfb-смоук не выполнялся.
- Правила автоскачивания (NB-60): `OEChatsDisableAutoDownloadingArchive`
  (bool, default `false`) и `OEChatsBlockWin32ExecutableFiles` (bool, default
  `true`) — архив не качается автоматически, а Windows-исполняемые считаются
  опасными. Гейты — в `DocumentMedia::automaticLoad`
  (`data/data_document_media.cpp`) и `VideoPreload::Can`
  (`data/data_media_preload.cpp`); helper `Exteraless::IsWin32Executable`
  (`exteraless/links.cpp`) — свой список из 18 расширений, поэтому глобальный
  `kExecutable` не тронут. Android-алиасы `disableAutoDownloadingArchive` и
  `Win32ExecutableFiles` принимаются в `.extera` (всего 104 ключа: 102 + 2,
  сверено с legacy-кортежем `run_checks`), два тумблера — в Chats, два
  lang-ключа в `lang.strings`, CLI `autodownload-archive`/
  `autodownload-archive-set`/`win32-exe`/`win32-exe-set`,
  `check_autodownload_rules` — 9 проверок плюс две на неверное число
  аргументов (счётчик выше 660, CLI 51, плагины 32 и 213 без изменений).
  `kKeysCount` 11171, sha256 патча `baf908c7…`,
  `nix flake check --no-build` PASS. Ограничения: правила действуют на новые
  загрузки, уже скачанное не удаляется; `.exe` под Linux теперь тоже
  блокируется (раньше качался); photo-preload без имени файла не покрыт —
  пустое имя не блокируется; Xvfb-смоук не выполнялся.
- Boost скачивания и загрузки (NB-76): `OEChatsDownloadSpeedBoost` (int
  `0..2`, default `0`) и `OEChatsUploadBoost` (bool, default `false`).
  Загрузка — пул сессий 1/2/4 и часть 128K/512K/1M через
  `BoostedDownloadPartSize()`/`StartSessionsCount()` в
  `storage/download_manager_mtproto.cpp`, окно ожидания считается в частях
  при неизменных таймаутах; отдача — часть до 512K при капе 1M в
  `Uploader::Entry::setDocSize` (`storage/file_upload.cpp`), пейсинг не
  тронут. Алиасы `downloadSpeedBoost`/`UploadBoost` и legacy
  `OEGeneralDownloadSpeedBoost`, `.extera` — 106 ключей (104 + 2, сверено с
  legacy-кортежем `run_checks`), Radioenum Off/Boost/Ultra и тумблер в Chats,
  пять lang-ключей, CLI 4 режима; `check_download_boost` — 10 проверок, всего
  672 PASS (backup 100, CLI 53). `kKeysCount` 11176, sha256 патча
  `7689cc10…`, `nix flake check` PASS. Ограничения: при boost > 0
  CDN-редиректы могут не пройти hash-проверку (слайсинг CDN остался 128K), на
  слабой сети пул может сжаться из-за неизменных порогов времени (нужен замер
  MB/s), смена boost посреди загрузки меняет шаг офсета, preload/streaming
  остались на 128K, Xvfb-смоук и сетевой замер не выполнялись.
- Bot privacy (NB-55): `OEGeneralBotPrivacyEnabled` (bool, default `false`),
  `OEGeneralBotPrivacyAccount` (int `-1..15`, default `-1`, 1-based индекс
  `orderedAccounts`) и `OEGeneralBotPrivacyAlwaysAsk` (bool, default `false`).
  Ссылку на бота маршрутизирует хук `RouteBotToPrivacyAccount` в
  `SessionNavigation::showPeerByLinkResolved`
  (`window/window_session_controller.cpp`) — он стоит перед всеми пятью
  ветками резолва, переключает аккаунт через
  `Core::App().domain().maybeActivate()` и отдаёт ссылку контроллеру целевого
  окна; `index <= 0` — Off, индекс вне `orderedAccounts` даёт тост, от
  рекурсии спасает сравнение аккаунта и контроллера. Алиасы
  `botPrivacyEnabled`, `botPrivacyUserId` и `alwaysAsk` в `.extera` — 109
  ключей (104 + 3 + 2, сверено с legacy-кортежем `run_checks`), в General
  тумблер, селектор Off/1..N и тумблер «Always ask…», четыре lang-ключа,
  CLI `bot-privacy`/`bot-privacy-set`; `check_bot_privacy` — 9 проверок плюс
  две на неверный вызов CLI, всего 683 PASS (backup 100, CLI 55).
  `kKeysCount` 11180, sha256 патча `ba0d94a7…`, `nix flake check` PASS.
  Follow-up: пикера для `alwaysAsk` нет — режим только маршрутизирует без
  вопроса; четыре тоста остаются английскими литералами без lang-ключей;
  `unblock` в текущем аккаунте при `startAutoSubmit` не сделан.
- Внешние субтитры (NB-32), фаза 1: парсер `exteraless/subtitles.{h,cpp}` —
  `ParseSubtitles`, `ActiveCueText` и `VideoKey`. Читаются SRT и WebVTT:
  тайм-код с `.` и `,`, необязательный номер реплики, пропуск заголовка
  `WEBVTT` и директив `NOTE`/`STYLE`/`REGION`, вырезание тегов `<...>` и
  `{...}`, CRLF/CR → LF, UTF-8 с BOM и без плюс UTF-16 LE/BE по BOM. Реплики
  сортируются по старту, активный интервал end-exclusive, перекрытия
  склеиваются через `\n`. Шесть фикстур (`basic.srt`, `basic.vtt`,
  `overlap.srt`, `tags.srt`, `broken.srt`, `bom-utf16.srt`), CLI
  `subtitles-parse`/`subtitles-active`/`subtitles-key`, `check_subtitles` —
  48 проверок (счётчик выше 734, CLI 58, плагины 32 и 213 без изменений).
  `.gitattributes` закрепляет `exteraless/tests/chats/fixtures/*.srt -text` —
  иначе CRLF-фикстура схлопнется при checkout. Новых `OE`-ключей нет,
  `.extera` не менялся; sha256 патча `8c587ce7…`, `nix flake check` PASS.
  Фаза 2 открыта: рендер в четырёх бэкендах, меню выбора/отключения и
  пер-видео память — follow-up, код оверлея не тронут.
- Внешние субтитры (NB-32), фаза 2: рендер, меню и память по видео.
  `media_subtitles.{h,cpp}` (`Telegram/CMakeLists.txt`) добавляют
  `Exteraless::Subtitles`: он держит путь, метку и реплики на каждое видео,
  карта `doc_<id>` → `{path, label}` хранится в `OESubtitlesFiles`
  (JSON-объект, кап 128 видео, `ValidVideoKey`); при импорте `.extera` она
  проходит через `SanitizeSubtitlesFiles` (только строковые `path`/`label`,
  записи без пути отбрасываются), а `Config::setSubtitlesFiles` не сохраняет
  не-объект. `OEMediaViewerMenuSubtitles` (bool, default `true`) — тумблер в
  «Media viewer menu»; lang-ключей три: `lng_mediaview_subtitles_choose`,
  `lng_mediaview_subtitles_disable` и
  `lng_settings_exteraless_media_menu_subtitles`. В меню медиавьюера —
  «Choose Subtitles File» (`FileDialog` `*.srt *.vtt`) и «Disable Subtitles»
  только при выбранном файле, гейт `canShowSubtitles()` — видео без webpage
  и стрима. Позиция из `UpdateVideo` и `updatePlaybackState`, перерисовка
  только при смене текста активной реплики. `paintSubtitles(QRect, float64)`
  в `Renderer`, raster, OpenGL (`_subtitlesImage`) и RHI; контент общий —
  `paintSubtitlesContent` со `mediaviewSubtitleBg`/`mediaviewSubtitleFg`
  (алиасы caption); вызов после opacity-блока, поэтому fullscreen тоже видит
  субтитры. `.extera` — 111 ключей (106 + 3 + 2, сверено с legacy-кортежем
  `run_checks` `(106, 3, 2)`); раннеры 734/32/213 PASS (backup 100),
  `kKeysCount` 11183, sha256 патча `80ade745…`. Ограничения: PiP и stories
  не покрыты, preload/streaming остались 128K, CLI и раннеры проверяют
  только парсер — проверки меню и пер-видео памяти нет, Xvfb-смоук не
  выполнялся, `nix flake check` не запускался в docs-задаче.
- Отложены: NB-21 (нужен SQLite), NB-33 (item виден, нужна sidecar-цитата),
  NB-43 (peer в MTP-фильтре).
- Ghost scheduled send (NB-23): `OEGhostScheduledSend` (default `false`) при
  активном online-ghost отправляет новые текстовые сообщения как отложенные
  на 12 секунд (`SendOptions.scheduled`), исключая self, ботов и секретные
  чаты; решение — `ShouldGhostSchedule` в `exteraless/ghost_schedule.{h,cpp}`,
  CLI `ghost-schedule`/`ghost-schedule-decide` (12 проверок). Xvfb: тумблер в
  Ghost Mode. Реальная отправка не выполнялась, чтобы не писать в чужие чаты.
- Save to chat subfolder (NB-26): `OEGeneralSaveToChatSubfolder` (default
  `false`) добавляет подпапку с именем чата к автоматическим сохранениям в
  `DocumentSaveClickHandler` (имена санитизируются, коллизии получают
  ` (2)`); «Save as...» по-прежнему спрашивает путь. CLI `save-subfolder`,
  `chat-folder`, `chat-subfolder` (15 проверок). Xvfb: тумблер в General.
  `customSavePath` отдельно не добавлялся — в desktop уже есть «Download
  path» в Advanced.
- Inline math (NB-24): `OEChatsInlineMathResult` (default `false`) вычисляет
  выражение при Space после `=`. Парсер `exteraless/math.{h,cpp}`: `+ - * / %`,
  `^`, скобки, `pi`/`e`, `sqrt`/`abs`/`sin`/`cos`/`tan`/`asin`/`acos`/`atan`/
  `ln`/`log`/`exp`/`floor`/`ceil`/`round`; деление на ноль и мусор → invalid.
  Фильтр установлен на поле ввода `HistoryWidget` и `ComposeControls`,
  CLI `math-eval`/`inline-math` (31 проверка). Xvfb: после `2+2=` и Space
  черновик стал `2+2=4`.
- Channels / Send as (NB-35, частично): `OEChatsHideSendAsPeer` (default
  `false`) убирает кнопку «Send as...» в каналах; Android-алиас
  `hideSendAsChannel`, тумблер в Chats, CLI `send-as` (5 проверок). Xvfb:
  тумблер записывает ключ. `hideCameraTile` не применим (в attach-меню
  tdesktop камеры нет), `hideShareButtonInChannel` — профиль (NB-73).
- Plugins install/delete (NB-29): `Install plugin...` копирует `.plugin`/`.py`
  в `tdata/plugins` (санитайз имени, суффикс `-2` при коллизии, лимит 2 МиБ,
  отказ для symlink и чужих расширений), `Delete plugin` удаляет файл после
  подтверждения и отключает плагин. Логика — `exteraless/plugins/installer.*`,
  CLI `plugin-install`/`plugin-delete`/`plugin-filename` (19 проверок).
  Xvfb: установка через файловый диалог появилаcь в списке, удаление убрало
  файл и перестроило бокс.
- Pill Stack reset (NB-31): кнопка «Reset pill order» сбрасывает
  `OEPillStackOrder` в дефолт и показывает тост; CLI `pillorder-reset`
  (3 проверки). Xvfb: порядок вернулся к `cpu,ram,net,dc,proxy,cache,ghost,
  lastseen`. Бесконечная прокрутка не применима к wrapped-полосе.
- AI editor (NB-27): `OEAppearanceHideAiEditor` (default false) скрывает
  кнопку AI в compose и caption-боксах; тумблер в General, CLI `ai-editor`
  (5 проверок). Xvfb: тумблер записывает ключ. Саммари IV/сообщений в этой
  версии отсутствуют — отдельные ключи не добавлялись.
- Settings sections (NB-36): `OESettingsHidePremium`/`OESettingsHideHelp`
  (default false) убирают Premium/Credits и Help из главного экрана настроек;
  тумблеры в General, CLI `settings-sections` (5 проверок). Xvfb: после
  включения секции исчезли, после выключения вернулись.
- Silent send (NB-34): `OEChatsSilentByDefault` (default `false`) выставляет
  `SendOptions.silent` в `prepareSendAction` обоих виджетов истории; тумблер в
  Chats, CLI `silent` (5 проверок). Xvfb: тумблер записывает ключ.
- Pangu (NB-25): `OEChatsPanguOnSending` (default `false`) добавляет пробелы
  между CJK и латиницей/цифрами в `HistoryWidget::sendTextWithTags` и
  `ChatWidget::sendTextWithTags`; `ApplyPanguWithMap` корректирует text-tags,
  CLI `pangu`/`pangu-apply`/`pangu-tags` (15 проверок). Xvfb: тумблер в Chats.
- Прокси-правила:
- Network: `ppp*` больше не считается VPN (`IsVpnInterfaceName`), PPPoE не
  выключает прокси; CLI `vpn-name` (10 проверок).
- Proxy rules: автовыключение прокси хранится только в памяти, срабатывает
  один раз на переход условия и не отменяет ручное включение; после
  перезапуска без условия прокси не включается сам. `OEProxyAutoDisabled`
  остаётся legacy и не пишется. Решение — `DecideProxyRule` в
  `exteraless/proxy_rules_state.{h,cpp}`, CLI `proxy-rule` (6 проверок).
- Deleted message mark (NB-20, split-required): добавлены
  `OEChatsTranslucentDeleted` (default `true`), `OEChatsUseDeletedIcon`
  (`true`), `OEChatsCustomDeletedMark` (пусто) и `OEChatsDeletedMarkColor`
  (`0`, всего 53 ключа), Android-алиасы в `.extera`, два тумблера в Chats.
  Бокс «Deleted messages» показывает 🗑/кастомную метку, прозрачность 0.6 и
  цвет; CLI `deletedmark` (10 проверок). Xvfb: с иконкой строка помечена и
  полупрозрачна, при `UseDeletedIcon = false` и пустой кастомной метке значка
  нет. Полная inline-вставка в историю чата выделена в NB-92.
- Deleted messages schema (NB-19): сайдкар переведён на версию 2 и хранит
  17 полей: реакции, `replyToMsgId`/`replyToText`, `forwardedFrom`/
  `forwardedDate`, entities, `editedDate` и флаг `read`. Сериализация вынесена
  в Qt-only `exteraless/deleted_entry.{h,cpp}`, старые v1-записи читаются с
  дефолтами, бокс показывает `[❤] [reply] [edited]`. CLI `deleted-entry`
  (7 проверок) проверяет roundtrip, фильтрацию entities и обязательные id.
  Xvfb smoke: после удаления сообщения с реакцией и ответом бокс «Deleted
  messages» показывает реакцию и метку reply; попутно исправлен use-after-free
  захвата `controller`/`peer` в `window_peer_menu.cpp` (раньше бокс мог не
  открыться или упасть).
- Ghost locked-тумблеры (NB-18): четыре настройки `OEGhostSendReadLocked`,
  `OEGhostSendStoriesLocked`, `OEGhostSendOnlineLocked`,
  `OEGhostSendUploadProgressLocked` исключают отдельные тумблеры из пакетного
  переключения
  и из расчёта `On/Off/Partial`; в настройках появилась подсекция
  «Locked Ghost settings». CLI `ghost-lock` и `ghost` (19 проверок) проверяют
  установку, пропуск locked в batch, `fromJson` и Android-алиасы
  `send*Locked`. Xvfb smoke: включённый lock read сохраняет
  `OEGhostSendReadPackets = true` при Ghost On, остальные три выключаются,
  статус в меню — `On`.
- Ghost Mode batch/status (NB-17): пункт главного меню включает или выключает
  все четыре тумблера одним кликом и показывает `On` / `Off` / `Partial`
  при включённой настройке `OEGhostShowStatus` (Android-ключ
  `showGhostModeStatus` в `.extera`, суммарно 44 ключа). `IsGhostActive`
  теперь строгий: активен только когда все четыре в ghost-режиме;
  `Partial` виден в меню. Статусные функции вынесены в
  `exteraless/ghost_status.{h,cpp}` (без зависимости от MTP), CLI `ghost`
  (13 проверок) проверяет дефолты, пакетное включение/выключение, partial,
  `fromJson` и backup-алиас. Xvfb smoke: «Ghost Mode (Off)» → клик →
  «Ghost Mode (On)», в `exteraless.json` все четыре `OEGhostSend* = false`;
  повторный клик возвращает `true`.
- Pill Stack order (NB-16): пилюли перетаскиваются прямо на полосе над
  списком чатов; порядок хранится в `OEPillStackOrder` (CSV из
  `cpu,ram,net,dc,proxy,cache,ghost,lastseen`), санитизируется (уникальность,
  известные ключи, недостающие дописываются), экспортируется в
  `pillstackconfig` `.extera` (суммарно 43 ключа) и применяется при старте.
  CLI `pillorder` (9 проверок) проверяет дефолт, drag-эквивалент, санитайз,
  `fromJson` и backup. Xvfb smoke: перетаскивание меняет порядок, после
  рестарта порядок сохраняется. При доработке исправлен порядок
  инициализации: `applySavedOrder()` вызывается в конце конструктора, а не до
  настройки виджета, иначе приложение могло падать на старте.
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
- Chats/General, сборка: итоговый `out/bin/Telegram` sha256 `75e759aa…`,
  mtime 2026-11-06 02:15; GNU ld по-прежнему упирается в debug-info overflow,
  поэтому затронутые TU собраны точечно и финальный линк выполнен LLD.
  `ninja -C out -n Telegram` после этого не показывает compile/link edges
  (только glob/CMake re-run). Перед финальным линком пересобраны TU,
  зависящие от `history_view_webpage_processor.h`: header менялся в NB-74,
  а `history_widget.cpp.o` остался от 12:02, и старый `sizeof` давал порчу
  хипа со SIGSEGV в `~WebpageProcessor` при смене чата; после пересборки
  переключение чатов стабильно.
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

- [x] AI-клиент: MVP (NB-48) — endpoint/model/key и бокс запрос→ответ;
  стриминг, роли, история, замена редактора и саммари — follow-up.
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
