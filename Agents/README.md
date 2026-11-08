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
  - `ayu_import.*` — импорт базы AyuGram (NB-21): QSQLITE только на чтение
    (`immutable=1` как fallback), отказ при схеме вне `[21,27]`, 11 колонок
    `deletedmessage` → `DeletedEntry`, дедуп `(peerId, msgId)`, запись
    `QSaveFile` в формате v2; entities, реакции, медиа, out и read не
    переносятся (задокументированная потеря);
  - `backup.*` — формат `.extera` и полный JSON-бэкап (`BuildBackup`,
    whitelist ключей); `BuildBackup(true)` заменяет секреты из `kSecretKeys`
    (`OEAiApiKey`, `OETranslationGoogleCloudKey`, `OETranslationDeepLKey`)
    на `***REMOVED***`, а импорт такое значение пропускает, поэтому локальные
    секреты не затираются; CLI — `backup-export-redacted`;
  - `network.*` — тип сети через Qt NetworkInformation + fallback по интерфейсам;
  - `plugins/scanner.*` — запуск `python3 -I -B` и разбор JSON манифестов;
  - `plugins/installer.*` — установка/удаление файлов плагинов;
  - `math.*` — оценка inline-математики (`2+2=` → `4`);
  - `save_path.*` — подпапка чата при автосохранении файлов;
  - `format.*` — `PersianDateText(date, latin, withYear)` переводит дату в Solar
    Hijri через `QCalendar` Jalali (12 персидских месяцев, цифры ASCII или
    `U+06F0`); при включённом `OEGeneralUsePersianCalendar` его вызывают
    `langDayOfMonth*`/`langDateTime*`;
  - `ghost_schedule.*` — решение об отложенной Ghost-отправке (+12 с);
  - `chats.*` — `ChatsSettings` (включая `stickerSize`: int 4..20, default
    14), правило скрытия реакций и unread capsule;
  - `links.*` — `StripTracking` для http/https-ссылок;
  - `zalgo.*` — фильтр Zalgo-текста: серии из 4+ комбинирующих знаков и карта
    смещений для пересчёта entities;
  - `subtitles.*` — парсер SRT/WebVTT: `ParseSubtitles`, `ActiveCueText` и
    `VideoKey`; UTF-8 с BOM и без, UTF-16 LE/BE по BOM, CRLF, вырезание тегов,
    сортировка по старту и склейка перекрывающихся реплик, активный интервал
    end-exclusive. Рендер и выбор файла — NB-32, фаза 2: `media_subtitles.*`
    помнит субтитры по видео (JSON-карта в `OESubtitlesFiles`), медиавьюер
    добавляет пункты «Choose Subtitles File» и «Disable Subtitles», а реплики
    рисуются в raster, OpenGL и RHI.
  - `lyrics.*` — парсер LRC: `ParseLyrics` и `ActiveIndex`. Формат только
    LRC (SRT не принимается): `[offset:±ms]` сдвигает таймстемпы, строка с
    несколькими `[mm:ss.xx]` дублируется на каждую метку, word-time
    `<mm:ss.xx>` внутри текста вырезается, BOM UTF-8/UTF-16 LE/BE
    распознаётся, прочие теги в `[...]` отбрасываются; файл без таймстемпов
    даёт `synced: false` и одну строку от нуля, файл без текста —
    `instrumental`. Тексты в аудио-экране — NB-50, фаза 1: `media_lyrics.*`
    помнит текст по видео (JSON-карта в `OELyricsFiles`, алиас
    `lyricsFiles`, `activeText` по позиции), медиавьюер добавляет «Choose
    Lyrics File…» (`*.lrc`) и «Disable Lyrics», а в overview аудио-блок
    показывает активную строку акцентом с автоскроллом и лимитом в три
    строки. Фаза 2 — онлайн-тексты lrclib.net: Qt-only `lrclib.*`
    (`BuildSearchUrl`/`BuildGetUrl`, `ParseSearch`/`ParseGet`) и
    `MediaLyrics::fetchOnline` в `media_lyrics.*` под гейтом
    `OELyricsAllowNetwork` с диск-кэшем `tdata/lrclib` на 30 дней и
    псевдо-путём `lrclib:<docId>`; пункт «Find Lyrics Online» в меню
    медиавьюера — только при включённом гейте.
  - `stt.*` — офлайн-распознавание речи (NB-51): sherpa-onnx 1.13.8 из
    pinned nixpkgs (`buildInputs` + FOD модели + `EXTERALESS_STT_MODEL`),
    WAV-декод + собственный реземплер в mono 16k, `TranscribePcm`
    (stereo 48k), recognizer per-call, async; декод `.oga` через
    `Media::FFMpegLoader` в пункте меню; тост с текстом / тост ошибки
    (без новых lang-ключей).
  - `ai_client.*` — OpenAI-совместимый сборщик запроса без сети:
    `ChatCompletionsUrl` (дописывает `/chat/completions`), `BuildRequestJson`
    (`messages`, `stream: false`) и `ParseResponse` (первый непустой
    `choices[].message.content`, текст ошибки из `error`), плюс SSE
    `SseParser` (`feed` по строкам буфера, флаг `[DONE]`, `reset`) и
    `ParseStreamEvent` (`delta.content`; битые строки молча пропускаются),
    плюс `ParseAiRoles` и перегрузка `BuildRequestJson` (system+history);
    сама сеть живёт только в боксе
    `Telegram/SourceFiles/boxes/ai_request_box.*` («Ask AI» из контекстного
    меню текстового сообщения: поле запроса, стримовый ответ по `readyRead`
    с `stream: true`, abort по `[DONE]`, отмена при закрытии, fallback
    `ParseResponse`, Send/Copy/Close), ключ `OEAiApiKey` читается при каждой
    отправке; роли `OEAiRoles`/`OEAiActiveRole` (`ParseAiRoles`) с
    радиоселектором в боксе и in-memory историей turns (cap 20, `OEAiHistory`); саммари — `exteraless/ai_summary.*` (сбор 30/6000, `<chat>`-guard) + пункт в меню пира + system-override бокса.
  - `extra_pills.*` — погода, курсы и золото для Pill Stack (`Weather`,
    `Btc`, `Eth`, `Usd`, `Eur`, `Gold`): URL-билдеры `WeatherApiUrl`
    (Open-Meteo, координаты Москвы захардкожены), `CoinbaseSpotUrl`,
    `EcbDailyUrl`, `CbrDailyUrl` и `GoldApiUrl` плюс парсеры `ParseWeather`,
    `ParseCoinbase`, `ParseEcbXml`/`ParseCbrXml`/`FormatFiatRate` и
    `ParseGoldApi`; курс рубля берётся из XML ЦБ РФ (`ParseCbrXml` —
    `CharCode`/`Nominal`/`Value`, десятичная запятая в точку, деление на
    `Nominal`), ECB-кросс остаётся fallback'ом, а прочерк `—` — последним.
    `ExtraPills::Refresh` троттлится 30 минутами, кэш только в памяти, а
    пустой ответ или ошибка сети дают прочерк `—`.
- `Telegram/SourceFiles/exteraless/plugin_runtime.*` — singleton `Runtime`:
  один `QProcess` на приложение (bwrap + Python host), JSON-lines IPC,
  проверка путей, audit-лог, load/unload и остановка host при выходе.
- `Telegram/SourceFiles/exteraless/pill_stack.*` — полоса Pill Stack в списке чатов.
- `Telegram/SourceFiles/exteraless/plugins_box.*` — бокс «Plugins»: список,
  ошибки, пустое состояние, тумблеры, статус host, просмотр последних строк
  `plugins-audit.log`, экспорт лога, установка `.plugin`/`.py` из файла
  и удаление плагина.
- `exteraless/tests/plugins/` — CLI `exteraless-plugins-scan` (stage 1),
  CLI `exteraless-plugin-host-cli` (stage 2), фикстуры, `run_checks.py`
  (32 проверки) и `run_host_checks.py` (279 проверок) с фикстурами
  `fixtures/runtime/`; в приложение не входят.
- `exteraless/tests/chats/` — CLI `exteraless-chats-cli`, фикстуры
  `fixtures/strip.tsv` и `fixtures/reactions.tsv`, `run_checks.py`
  (1167 проверок: strip, reactions, CLI, config, general, bot-privacy, deleted, deleted-entry, reset, workdir, menu, doubletap, stickertime, sticker-size, pillorder, ghost, deletedmark, vpn, proxy-rules, pangu, zalgo, spoilers-direct, formatting-buttons, silent, sections, ai-editor, pluginops, send-as, profile-buttons, inline-math, save-subfolder, ghost-schedule, seek-duration, edited-mark, forward-date, sender-mini-avatars, header-style, media-menu, chat-menu, disable-link-preview, hide-all-chats, hide-archive-sort, always-show-download, show-results-before-voting, false-bottom, decoration, shortcuts, message-menu-style, reply-style, wide-posts, chat-misc, persian, autodownload-rules, media-quality, update-channel, translation, ai, ai-stream, recognition, network, download-boost, subtitles, lyrics, lrclib, extra-pills, ai-roles, weather-city, stt, ayu-import, avatar-corners, section-radius, regex-filters, backup, backup-redacted);
  в приложение не входят.
- `Telegram/SourceFiles/settings/sections/settings_exteraless.cpp` — раздел настроек;
  пункт «Avatar corners» (`OEChatsAvatarRadius`, int `-1..32`) выбирает форму
  юзерпика через `Ui::Radioenum` Circle/Square/Rounded, а маску применяет
  `Ui::ValidateUserpicCache` в `Telegram/SourceFiles/ui/userpic_view.cpp`;
  подсекция «Media» (`OEGeneralDefaultVideoQuality` Auto/720p/1080p,
  `OEGeneralHardwareAcceleration` — алиас поверх upstream-настройки,
  `OEChatsTakeGifAsVideo` и `OEGeneralCustomAudioBitrate` 16/24/32/48 kbps)
  задаёт стартовое качество видео, аппаратное декодирование, отправку GIF
  видео и битрейт записи Opus; пункт «Update channel»
  (`OEGeneralAutoUpdateChannel`, int `0..2`, default `1`) выбирает канал
  Off/Release/Beta через `Ui::Radioenum`, а разбор списка релизов живёт
  в Qt-only модуле `exteraless/update_check.{h,cpp}`
  (`Exteraless::ParseReleasesJson`, `Exteraless::ReleasesApiUrl`) — сети он
  не делает, только парсит ответ GitHub releases; тумблер «Show mini avatars
  next to sender names» (`OEChatsSenderMiniAvatars`, bool, default `false`,
  алиас `senderMiniAvatars`) включает мини-аватарку отправителя в превью
  диалога — рисует `PaintRow` (`dialogs/ui/dialogs_layout.cpp`) перед
  `paintItemCallback`, а кэш `PeerUserpicView` держит `BasicRow`
  (`dialogs/dialogs_row.h`); пункт «Chat header style»
  (`OEChatsChatHeaderStyle`, int `0..2`, default `0`; `0` — upstream,
  `1` — iOS-центровка, `2` — M3) центрирует имя и статус шапки в
  `history/view/history_view_top_bar_widget.cpp`, а для M3 подставляет
  static-копию стиля `M3TitleStyle()` со шрифтом `st::topBarM3TitleFont`
  (`font(17px semibold)` в `info/info.style`); пункт «Section radius»
  (`OEChatsSectionRadius`, int `-1..16`, default `-1`) и тумблер «Separate
  headers» (`OEChatsSectionSeparateHeaders`, bool, default `false`) правят
  подложку секций — `SectionCard` в
  `Telegram/SourceFiles/settings/settings_builder.cpp`. Провайдеры перевода
  выбирает `OETranslationProvider` (int `0..3`, default `0` — Auto/upstream,
  Google, DeepL; `3` — резерв под LLM/NB-48), ключи лежат в
  `OETranslationGoogleCloudKey`/`OETranslationDeepLKey` и правятся двумя
  маскированными полями, формальность DeepL — в
  `OETranslationDeepLFormality` (`0..2`); диспетчер
  `Ui::CreateTranslateProvider` (`lang/translate_provider.cpp`) стоит перед
  upstream-цепочкой, провайдеры — `lang/translate_google_provider.cpp` и
  `lang/translate_deepl_provider.cpp`, в экспорт `.extera` ключи не попадают.
  Пункт «AI assistant» правит `OEAiEndpoint`/`OEAiModel` и маскированное поле
  `OEAiApiKey` (в `.extera` ключ не экспортируется). Пункт «Speech
  recognition language» хранит язык распознавания в
  `OEGeneralRecognitionLanguage` (строка, default `ru`; пустое значение →
  `ru`, алиас `recognitionLanguage`) и выбирает его из шести языков
  (ru, en, de, fr, es, it); самого распознавания нет — пункт «Transcribe
  offline» в контекстном меню голосовых показывает тост
  `lng_media_view_transcribe_offline_na` («Offline model is not installed»),
  CLI 2 режима (`recognition-language`, `recognition-language-set <code>`).
  Подсекция «Network» держит тумблер «Try IPv6 connections»
  (`OENetworkUseIPv6`, bool, default `true` вне Windows, алиас `useIPv6`),
  селектор «DNS provider» (`OENetworkDnsType`, int `0..1`, default `0` —
  Built-in DoH, `1` — System, алиас `dnsType`) и поле «Custom DoH URL»
  (`OENetworkCustomDoH`, строка, пусто — выключено, алиас `customDoH`);
  значение IPv6 засевается в `settings().proxy().setTryIPv6()` из
  `Application::startLocalStorage` (после `Local::start()`, не в
  конструкторе), а резолвер `mtproto/details/mtproto_domain_resolver.cpp`
  добавляет system-попытку через `QHostInfo` (TTL 5 минут) и prepend
  customDoH с fallback на встроенные хосты — диалект по хосту: cloudflare
  получает JSON `/dns-query`, остальные Google-форму `/resolve`.
- `Telegram/SourceFiles/boxes/deleted_messages_box.cpp` — бокс удалённых
  сообщений, показывает реакции и метки reply/forward/edited, а также
  значок/кастомную метку, прозрачность и цвет (NB-20); inline-показ в чате —
  NB-92, вставка в историю через `DeletedMessages::insertFor`.
- Inline-показ удалённых (NB-92, продолжение NB-20):
  `History::addLocalMessageInPlace` (`history/history.cpp`) вставляет запись
  sidecar в историю на её место — порядок по паре `(date, entry.msgId)` — и
  регистрирует элемент через `registerClientSideMessage`; реестр
  `FullMsgId → MsgId` (`_inserted`/`_placeholders`) в `DeletedMessages`
  различает вставки (`isPlaceholder`) и снимает их
  (`insertFor`/`removeFrom`/`refresh`). Вставка идёт только при
  `loadedAtTop()` и `loadedAtBottom()`; метка — текстом (🗑 из
  `OEChatsUseDeletedIcon` или `OEChatsCustomDeletedMark`), прозрачность 0.6
  в `Message::draw` при `OEChatsTranslucentDeleted`; тумблеры save и
  translucent вызывают `refresh()`. Delete плейсхолдера закрыт guard'ом, а Reply
  вставляет blockquote-цитату из sidecar в поле ввода (NB-33, настоящим
  ответом не становится). Не сделано: inline-медиа и реакции, цветная
  метка (подходящего `EntityType` нет, `kMaxEntityType` не менялся —
  реальный максимум 27 покрывают текущие 28). Ограничения: вставки
  появляются после перестройки истории; удаление плейсхолдера снимает только
  регистрацию — запись sidecar остаётся; Xvfb-смоук не выполнялся.
- Ответ на удалённое (NB-33): Reply у плейсхолдера больше не закрыт guard'ом,
  а вставляет цитату в compose — `DeletedMessages::BuildQuotePrefix` плюс
  blockquote-тег через `ComposeControls::setText`, при этом `_replyTo` не
  трогается и `reply_to_msg_id` не уходит; Delete плейсхолдера по-прежнему
  закрыт guard'ом.
- `nix/upstream-base` — закреплённая база патча: локальный коммит, tree-хеш и
  upstream-коммит; `nix/update-patch.sh --check` проверяет применимость.
- `Telegram/SourceFiles/mtproto/mtproto_request_filter.h` — интерфейс фильтра запросов.
- `Telegram/SourceFiles/window/window_main_menu.cpp` — пункты Exteraless в главном меню.
- `Telegram/SourceFiles/dialogs/dialogs_feed.{h,cpp}` — лента каналов (NB-47,
  v1, только чтение): `IsEligible` (broadcast без megagroup), `Collect` через
  `enumerateBroadcasts`, персист исключений в `feed_excluded` и
  `Generation`/`Invalidate`; синтетический фильтр `Dialogs::kFeedFilterId`
  (`FilterId(1000)`, `dialogs/dialogs_filters.h`) наполняет `FillFeed` в
  конструкторе `MainList`, а открывает пункт «Feed» главного меню. Список
  строится заново при пересоздании — live-update, strip-строка вкладки и
  действия над элементами остаются follow-up'ом (v2). v2: секция Feed последней в полосе фильтров (`chat_filters_tabs_strip.cpp`: маппинг индексов, unread-подписка, pinned-интервал, guard reorder/drag/dnd, keyboard-switch; бейдж — снимок). v3: live-update (`Feed::Changes` плюс `chatsListChanges`, дельта-rebuild с debounce 1 с и без сброса скролла), действия строк в ленте (`RowActionFilterId` в quick actions и контекстном меню) и guard `switchToFilter`, без которого лента не открывалась.
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
- Плагины этапа 3a — настройки: `OEPluginSettings` (JSON `id → {key: value}`,
  алиас `pluginSettings`, лимиты id/ключа/размера/глубины; в `.extera` не
  экспортируется, импорт — миграция) хранит значения плагина; host отдаёт их
  через `get_setting`/`set_setting`, снимок и `settings_ui` приходят в ответе
  `load`, а бокс `ShowPluginSettingsBox` рисует Switch/Selector/Input/
  Header/Divider (`Custom` — заглушка), изменения видны при следующем enable;
  CLI — `settings-get`/`settings-set`.
- Плагины этапа 3b — меню: `MenuItemType` (`main`/`message`/`chat`, PROFILE не
  перенесён) и `add_menu_item`/`remove_menu_item`, снимок `menu` в ответе
  `load`, `cmd: menu` → `clicked`/`error`, клик с таймаутом 2 с и
  re-entrancy-гардом без автостарта host'а; пункты `main` — отдельным
  `VerticalLayout` в главном меню, `message` — в конце контекстного меню
  сообщения, счётчик — в боксе, CLI `menu-list`/`menu-click`, а
  `check_plugin_menu` — 24 проверки.
- Плагины этапа 3c — intents и уровни доступа: `OEPluginTrustLevels` (JSON
  `id → 0|1|2`, default `1` — Gated; намеренное отклонение от Android
  ISOLATED=0, потому что манифест уже декларирует `intents` при установке,
  Isolated хранится явно, а Gated и Trusted различаются только будущим
  гейтингом) с алиасом `pluginTrustLevels` и экспортом в `.extera`; снимок
  `intents` в ответе `loaded`, `cmd:intent` → `intent-result` (таймаут 2 с,
  валидация как у меню); `Runtime::dispatchIntent(url, fromIntent)` — `false`
  это фаза before с правом cancel, `true` — after/notify — вызывается из
  хука `openLocalUrl` после passcode, а плагин уровня Isolated
  пропускается без обращения к хосту; селектор Isolated/Gated/Trusted в
  боксе, CLI `plugin-level`/`plugin-level-set`/`intent-test`, фикстура
  `valid-intents.plugin`, `check_plugin_intents` — 27 проверок. Не сделано:
  hooks/TRUSTED-капабилити и after-фаза вне notify; вызовы синхронные на
  UI-потоке (~2 с).
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
  декодируются. Список вшитый, обновления с сервера нет: к click-id добавлены
  `fb_action_ids`, `gclsrc`, `ysclid`, `pk_*`, `piwik_*`, `s_kwcid` и `sc_*`,
  а `matomo_*`/`at_*` сознательно сохранены — это фиксируют три
  false-positive строки `strip.tsv` (51 случай). Backup пишет все 138
  desktop-ключей в секциях `exteraconfig`,
  `pillstackconfig` и `mainconfig`; `OEProxyAutoDisabled` при импорте
  сбрасывается, потому что это производное сетевое состояние. Полосу историй
  над списком чатов скрывает `OEChatsHideStoriesHeader` (default `false`,
  тумблер в Chats, бэкап-алиасы `hideStoriesFromHeader`/
  `HideStoriesFromHeader`); ключ читается при следующем пересчёте видимости —
  live-сигнала нет, поэтому включение видно после возврата из настроек или
  рестарта. Zalgo-текст в сообщениях вычищает `OEChatsZalgoFilter` (default
  `false`, тумблер «Filter Zalgo text» в Chats, бэкап-алиас `zalgoFilter`):
  `exteraless/zalgo.{h,cpp}` ищет серию из четырёх и более `Mark_*` и режет её,
  вставка — в `Element::setTextWithLinks` с пересчётом entities (карта
  pangu-стиля, обнулившиеся отбрасываются), поэтому фильтр применяется при
  переоткрытии чата, а не на лету. Спойлеры без нажатия раскрывает
  `OEChatsShowSpoilersDirectly` (bool, default `false`, тумблер «Show spoilers
  without tapping» в Chats, бэкап-ключ `showSpoilersDirectly`): в
  `InitElementTextPart` он добавляет `setSpoilerRevealed(true,
  anim::type::instant)` и не ставит link-filter, а превью в списке чатов
  получает тот же флаг на `_textCache` в `MessageView::prepare`; медиа-гейт
  `EffectiveSpoiler` (`data/data_media_types.cpp`) снимает спойлер только у
  обычного пользовательского медиа, а TTL, `isMediaSensitive()` и платное
  медиа спойлер сохраняют (`hasSpoiler()` не тронут), поэтому значение
  применяется после перестройки текста, а не мгновенно. Тулбар форматирования
  включает `OEChatsShowFormattingButtons` (bool, default `false`, тумблер «Show
  formatting buttons» в Chats, бэкап-ключ `showFormattingButtons`): над полем
  ввода появляется ряд из восьми `Ui::LinkButton` (B, I, U, S, `</>`, `⊘`, `❝`
  и ссылка), теговые кнопки вызывают `toggleCurrentMarkdownTag` только при
  `markdownEnabledState().enabledForTag(tag)`, последняя —
  `editCurrentMarkdownLink`, фокус у поля не отбирается (`Qt::NoFocus`), высота
  ряда входит в `updateHeight` и поднимает шапку в `updateControlsGeometry`, а
  видимость пересчитывает `updateFormattingButtons` при обновлении видимости
  контролов, поэтому включение видно при следующем показе без рестарта. Размер
  стикеров задаёт `OEChatsStickerSize` (4..20, default 14, слайдер «Sticker
  size» в Chats):
  `Sticker::Size()` считает множитель `1 + (v - 14) / 15` от 224 px с клампом
  50..336 px; при 14 работает upstream `OptionStickerSize`, алиас
  `stickerSize` в `.extera` принимается с округлением, а размер применяется
  при переоткрытии чата. Форму задаёт `OEChatsStickerShape` (`0..2`,
  default `0`, выбор «Sticker shape» Square/Rounded/Bubble в Chats): `0` —
  квадрат, как в upstream, `1` — скругление 4 px (`MsgFileThumbRadiusSmall`),
  `2` — «bubble» (`BubbleRadiusLarge`); клип `QPainterPath`/`setClipPath` в
  `paintAnimationFrame`/`paintPixmap`/`paintPath` считается по фактическому
  rect отрисовки, а `customEmojiPart()`/`emojiSticker()` его пропускают, так
  что inline custom emoji и emoji-стикеры не трогаются.
  Кнопки профиля скрывают `OEChatsHideGiftButton`
  (Gift), `OEChatsHideCallButton` (Call в профиле и в шапке чата) и
  `OEChatsHideChannelSearchButton` (Search в шапке канала): тумблеры в Chats,
  алиасы `hideGiftButtonInChannel`/`hideGiftButton`, `hideCallButton` и
  `hideChannelSearchButton`, значение применяется после переоткрытия профиля
  или чата, `Ctrl+F` и меню «⋯» продолжают работать, `_groupCall` не трогали.
  Результаты опроса до голосования показывает `OEChatsShowResultsBeforeVoting`
  (default `false`, тумблер «Show poll results before voting» в Chats,
  бэкап-алиас `showResultsBeforeVoting`): предикат
  `Poll::showResultsBeforeVote()` пропускает только публичный опрос без голоса
  и без скрытых результатов, где `totalVoters > 0` и есть ответ с голосами, а
  `canVote()` смягчён, поэтому голосовать по-прежнему можно; multi-choice вне
  объёма, результаты применяются после переоткрытия или обновления опроса.
  Окраску цитаты ответа выключают `OEChatsReplyColors`, `OEChatsReplyEmoji` и
  `OEChatsReplyBackground` (все default `true`, подсекция Replies в Chats):
  у каждой цитаты свой `QuotePaintCache`, поэтому общий кэш не мутируется, а
  legacy-алиас `messageColoredBackground` сводится к `replyBackground` и
  отключает окраску только при явном `false`; медиа-карточки и ручные цитаты
  остаются цветными, а значение применяется после переоткрытия чата.
  Папку архива из списка чатов скрывает `OEChatsHideArchive` (default `false`,
  бэкап-алиасы `hideArchive`/`HideArchive`/`hideArchiveFolder`) через уже
  существующий skip-путь `_skipTopDialog` в `InnerWidget`; `shouldBeInChatList`
  не меняется, поэтому счётчики папок остаются корректными, а внешнее условие —
  `archiveInMainMenu() || hideArchive`. Чаты с непрочитанными поднимает
  `OEChatsSortByUnread` (default `false`, бэкап-алиасы
  `sortByUnread`/`SortByUnread`) через upstream-опцию
  `Dialogs::kOptionDialogsUnreadOnTop` без правок индексного списка. Оба ключа
  применяются сразу (`notifyListChanged`/пересорт), поиск и окно Archive архив
  по-прежнему видят, а при `hideArchive` с выключенным `archiveInMainMenu`
  вернуть архив можно только тумблером.
  Ширину баббла в broadcast-каналах снимает `OEChatsWideChannelPosts`
  (default `false`, тумблер «Wide channel posts» в Chats, бэкап-алиасы
  `wideChannelPosts`/`WideChannelPosts`): гейт рядом с
  `UnlimitedMessageWidth`, megagroup отсекается через `isBroadcast()`, медиа
  не затрагиваются, а ширина применяется после переоткрытия чата или ресайза.
  Мелочи чата (NB-86) задают `OEChatsHideGreetingSticker` и
  `OEChatsHidePanelsOnScroll` (обе default `false`) — гейт приветственного
  стикера в `GenerateChatIntro` и закрытие панелей `hidePanelsAnimated` в
  `onScroll` — плюс `OEChatsChannelBottomButton` (int 0/1/2, default `1`:
  `0` скрыть, `1` mute, `2` Discuss — кнопка с навигацией по
  `discussionLink`); алиасы
  `disableGreetingSticker`, `hideKeyboardOnScroll`/`HideKeyboardOnChatScroll`,
  `bottomButton`/`BottomButton`, legacy `DisableChannelMuteButton`
  (true → 0). Панели закрывает и автокомплит, hide-режим уносит gift/direct-
  иконки, значение применяется после переоткрытия, а `keepAttachButton` и
  `quickTransition*` — N/A (кнопки/жеста в desktop нет).
  Автоскачивание архивов и Windows-исполняемых выключают
  `OEChatsDisableAutoDownloadingArchive` (bool, default `false`) и
  `OEChatsBlockWin32ExecutableFiles` (bool, default `true`): гейты в
  `DocumentMedia::automaticLoad` и `VideoPreload::Can`, а
  `Exteraless::IsWin32Executable` (`exteraless/links.cpp`) держит свой список
  из 18 расширений, поэтому глобальный `kExecutable` не тронут; алиасы
  `disableAutoDownloadingArchive` и `Win32ExecutableFiles` принимаются в
  `.extera` (104 ключа, CLI 4 режима). Правила действуют на новые загрузки —
  уже скачанное не удаляется, `.exe` под Linux теперь тоже блокируется
  (раньше качался), а photo-preload без имени файла не покрыт.
  Скорость загрузки и отдачи задают `OEChatsDownloadSpeedBoost` (int `0..2`,
  default `0`) и `OEChatsUploadBoost` (bool, default `false`): пул сессий
  1/2/4 и часть 128K/512K/1M в `storage/download_manager_mtproto.cpp` (окно
  ожидания считается в частях, таймауты не менялись), часть отдачи до 512K
  при капе 1M в `Uploader::Entry::setDocSize` (пейсинг не тронут); алиасы
  `downloadSpeedBoost`/`UploadBoost` и legacy `OEGeneralDownloadSpeedBoost`,
  Radioenum Off/Boost/Ultra и тумблер в Chats, CLI 4 режима. При boost > 0
  CDN-редиректы могут не пройти hash-проверку (слайсинг CDN остался 128K),
  preload/streaming остались на 128K, а на слабой сети пул может сжаться —
  нужен замер MB/s.
- ID и DC в профиле: `OEGeneralShowIdDc` (default `false`) добавляет строку
  «ID and DC» (`id: 123456 · DC 2`) в профиль контакта (`makeInfo()` в
  `info/profile/info_profile_actions.cpp`) и в свой профиль (`SetupRows()` в
  `settings/sections/settings_information.cpp`); клик копирует `bareId` без
  префикса чата. Текст — `Info::Profile::IdDcText`
  (`info/profile/info_profile_values.cpp`), формат —
  `Exteraless::FormatIdDc` (`exteraless/format.{h,cpp}`). DC берётся из
  `mainDcId()` текущего аккаунта, поэтому дата-центр собеседника не
  показывается; значение не реактивно — строка появляется при следующем
  открытии профиля. Строка подавлена, если включён upstream-тумблер
  «Show peer ID below about» (`show-peer-id-below-about`). Тумблер — в
  General, бэкап-алиасы `showIdAndDc` (bool) и `idDcType` (число).
- Bot privacy: ссылку на бота открывает в выделенном аккаунте
  `OEGeneralBotPrivacyEnabled` (bool, default `false`) вместе с
  `OEGeneralBotPrivacyAccount` (int `-1..15`, default `-1`, 1-based индекс
  `orderedAccounts`) и `OEGeneralBotPrivacyAlwaysAsk` (bool, default `false`).
  Хук `RouteBotToPrivacyAccount` в `SessionNavigation::showPeerByLinkResolved`
  (`window/window_session_controller.cpp`) стоит перед всеми пятью ветками
  резолва, переключает аккаунт через `Core::App().domain().maybeActivate()` и
  отдаёт ссылку контроллеру целевого окна; `index <= 0` — Off, индекс вне
  `orderedAccounts` даёт тост, рекурсию отсекает сравнение аккаунта и
  контроллера. В General — тумблер, селектор Off/1..N и тумблер «Always
  ask…», бэкап-алиасы `botPrivacyEnabled`/`botPrivacyUserId`/`alwaysAsk`,
  CLI `bot-privacy`/`bot-privacy-set`. Пикера для `alwaysAsk` пока нет —
  режим маршрутизирует без вопроса, а четыре тоста остаются английскими
  литералами без lang-ключей.
- Двойной тап по сообщению: `OEChatsDoubleTapIn`/`OEChatsDoubleTapOut`
  (-1 — как в настройках Telegram, 0 — ничего, 1/2 — любимая реакция, 4 —
  ответ); резолвер — `exteraless/doubletap.{h,cpp}`. Поддержаны ещё шесть
  Android-действий (3 translate, 5 save, 8 edit, 10 delete, 11 forward,
  12 download), 6/7/9 дают «ничего», а 2 остаётся «Send reaction».
- Время на стикерах: `OEChatsStickerTimeSide` (сбоку) и
  `OEChatsHideStickerTime` (скрыто), отрисовка и hit-test в `UnwrappedMedia`
  через `Element::drawSideInfo`/`sideInfoRect`/`sideInfoTextState`.
- Порядок пилюль: drag на полосе `PillStack`, сохраняется в `OEPillStackOrder`
  (CSV из `cpu,ram,net,dc,proxy,cache,ghost,lastseen`); кнопка Reset
  возвращает дефолт.
- Ghost: пункт главного меню включает/выключает все четыре тумблера и
  показывает `On`/`Off`/`Partial` при `OEGhostShowStatus`; статусные функции —
  `exteraless/ghost_status.{h,cpp}`. Четыре `OEGhostSend*Locked` защищают
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
- Regex-фильтры сообщений (NB-22): движок `exteraless/regex_filters.{h,cpp}`
  (`Filter{regex, caseInsensitive, reversed, enabled}`, `CompileFilters`
  с `optimize()`, `MatchesText` = совпадение `find()` инвертируется
  `reversed`, `Matcher` с кэшем на 4096 записей) скрывает совпавшие сообщения
  в `pushItem` (`history/view/history_view_list_widget.cpp`); ключи
  `OEChatsRegexFilters*` (`Enabled`, `Data`, `EnableInChats`, `ShowHidden`,
  `ExcludedDialogs`) с алиасами `regexFiltersEnabled`/`regexFiltersData`/
  `enableInChats`/`excludedDialogs`, три тумблера в Chats, CLI 5 команд
  (`regex-*`), редактора списка фильтров и бара «N скрыто» нет, `^`/`$`
  матчат весь текст (`MultilineOption` выключен).
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
