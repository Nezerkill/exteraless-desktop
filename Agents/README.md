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
  - `plugins/installer.*` — установка/удаление файлов плагинов;
  - `math.*` — оценка inline-математики (`2+2=` → `4`);
  - `save_path.*` — подпапка чата при автосохранении файлов;
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
  (32 проверки) и `run_host_checks.py` (213 проверок) с фикстурами
  `fixtures/runtime/`; в приложение не входят.
- `exteraless/tests/chats/` — CLI `exteraless-chats-cli`, фикстуры
  `fixtures/strip.tsv` и `fixtures/reactions.tsv`, `run_checks.py`
  (761 проверка: strip, reactions, CLI, config, general, bot-privacy, deleted, deleted-entry, reset, workdir, menu, doubletap, stickertime, sticker-size, pillorder, ghost, deletedmark, vpn, proxy-rules, pangu, zalgo, silent, sections, ai-editor, pluginops, send-as, profile-buttons, inline-math, save-subfolder, ghost-schedule, seek-duration, edited-mark, forward-date, media-menu, chat-menu, disable-link-preview, hide-all-chats, hide-archive-sort, always-show-download, show-results-before-voting, false-bottom, decoration, shortcuts, message-menu-style, reply-style, wide-posts, chat-misc, autodownload-rules, media-quality, download-boost, subtitles, avatar-corners, backup);
  в приложение не входят.
- `Telegram/SourceFiles/settings/sections/settings_exteraless.cpp` — раздел настроек;
  пункт «Avatar corners» (`OEChatsAvatarRadius`, int `-1..32`) выбирает форму
  юзерпика через `Ui::Radioenum` Circle/Square/Rounded, а маску применяет
  `Ui::ValidateUserpicCache` в `Telegram/SourceFiles/ui/userpic_view.cpp`;
  подсекция «Media» (`OEGeneralDefaultVideoQuality` Auto/720p/1080p,
  `OEGeneralHardwareAcceleration` — алиас поверх upstream-настройки,
  `OEChatsTakeGifAsVideo` и `OEGeneralCustomAudioBitrate` 16/24/32/48 kbps)
  задаёт стартовое качество видео, аппаратное декодирование, отправку GIF
  видео и битрейт записи Opus.
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
  декодируются. Backup пишет все 111 desktop-ключей в секциях `exteraconfig`,
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
  переоткрытии чата, а не на лету. Размер стикеров задаёт
  `OEChatsStickerSize` (4..20, default 14, слайдер «Sticker size» в Chats):
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
