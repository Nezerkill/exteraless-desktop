# Статус порта Exteraless Desktop

Обновлено: 2026-11-06.

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
- Chats: `exteraless/tests/chats/run_checks.py` — 742 проверки PASS
  (44 strip, 10 reactions, 58 CLI, 7 config, 22 general, 9 bot-privacy,
  27 deleted, 7 deleted-entry, 7 reset, 8 workdir, 8 menu, 24 doubletap,
  14 stickertime, 11 pillorder, 19 ghost, 10 deletedmark, 10 vpn,
  6 proxy-rules, 15 pangu, 17 zalgo, 5 silent, 5 sections, 5 ai-editor,
  19 pluginops, 5 send-as, 5 profile-buttons, 31 inline-math,
  15 save-subfolder, 12 ghost-schedule, 8 seek-duration, 21 sticker-size,
  6 edited-mark, 5 forward-date, 5 media-menu, 5 chat-menu,
  5 disable-link-preview, 5 hide-all-chats, 13 hide-archive-sort,
  5 always-show-download, 7 show-results-before-voting, 5 false-bottom,
  5 decoration, 5 shortcuts, 5 message-menu-style, 7 reply-style,
  5 wide-posts, 15 chat-misc, 9 autodownload-rules, 10 download-boost,
  48 subtitles, 7 avatar-corners, 100 backup); плагины без изменений — 32 и
  213 проверок.
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
- Отложены: NB-21 (нужен SQLite), NB-33 (ждёт NB-92), NB-43 (peer в MTP-фильтре).
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
