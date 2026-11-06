# Changelog

## 2026-11-06

- Chats / Тулбар форматирования (NB-89, вторая половина):
  `OEChatsShowFormattingButtons` (bool, default `false`, алиас
  `showFormattingButtons`) показывает над полем ввода ряд из восьми
  `Ui::LinkButton`: B, I, U, S, `</>`, `⊘`, `❝` и ссылка. Теговые кнопки
  переиспользуют `Ui::InputField::toggleCurrentMarkdownTag` (только когда
  `markdownEnabledState().enabledForTag(tag)`), последняя —
  `editCurrentMarkdownLink`; фокус у поля не отбирается (`Qt::NoFocus`),
  accessible name берётся из готовых `lng_menu_formatting_*`, а шаг между
  кнопками — `st::defaultLinkButton.font->height / 2`. Видимость решает
  `updateFormattingButtons` (вызовы из `initKeyHandler` и
  `updateControlsVisibility`), высота ряда входит в `updateHeight`, а
  `updateControlsGeometry` поднимает шапку на его высоту. Тумблер «Show
  formatting buttons» в Chats, один lang-ключ
  `lng_settings_exteraless_chats_show_formatting_buttons`, CLI
  `formatting-buttons` и `formatting-buttons-set`,
  `check_formatting_buttons` — 7 проверок (две на неверный вызов CLI, группа
  `cli` не росла — 61); раннеры 825/32/213 PASS (backup 101); `.extera` — 123
  ключа (118 + 3 + 2, сверено с legacy-кортежем `run_checks`), ключ пишется
  как `showFormattingButtons`; `kKeysCount` 11206 (`kTagsCount` 185); sha256
  патча `e81c0789…`, `nix flake check` PASS. Ограничения: ряд появляется при
  следующем обновлении видимости контролов — перезапуск не нужен; глифы `⊘` и
  `❝` — компромисс `LinkButton` (иконок нет); markdown-парсер и подсветка
  синтаксиса не делались; Xvfb-смоук не выполнялся.
- Chats / Спойлеры сразу (NB-89, первая половина): `OEChatsShowSpoilersDirectly`
  (bool, default `false`, алиас `showSpoilersDirectly`) раскрывает спойлеры
  без нажатия. В `InitElementTextPart` режим добавляет
  `setSpoilerRevealed(true, anim::type::instant)` и пропускает установку
  `setSpoilerLinkFilter`, а превью в списке чатов делает то же для
  `_textCache` в `MessageView::prepare`. Медиа гейтит `EffectiveSpoiler` в
  `data/data_media_types.cpp`: спойлер снимается только у обычного
  пользовательского медиа, а TTL, `isMediaSensitive()` и платное медиа
  (`invoice()->isPaidMedia`) его сохраняют, `Media::hasSpoiler()` не тронут.
  Тумблер «Show spoilers without tapping» в Chats, один lang-ключ
  `lng_settings_exteraless_chats_show_spoilers_directly`, CLI `spoilers-direct`
  и `spoilers-direct-set`, `check_spoilers_direct` — 7 проверок (две на
  неверный вызов CLI); раннеры 818/32/213 PASS (backup 101, CLI 61);
  `.extera` — 122 ключа (117 + 3 + 2, сверено с legacy-кортежем `run_checks`),
  ключ пишется как `showSpoilersDirectly`; `kKeysCount` 11205
  (`kTagsCount` 185); sha256 патча `cfa7a617…`, `nix flake check` PASS.
  Ограничения: спойлер раскрывается после перестройки текста, а не мгновенно —
  открытый чат обновится на ближайшем событии; счётчики и превью
  чувствительного, платного и TTL-медиа остаются как в upstream; Xvfb-смоук не
  выполнялся..
- Chats / Стили шапки чата (NB-63): `OEChatsChatHeaderStyle` (int `0..2`,
  default `0`; `0` — upstream, `1` — iOS-центровка, `2` — M3) перестраивает
  шапку чата. При `1` и `2` имя и статус центрируются внутри доступной ширины
  (`headerAvail` — `width()` минус `_leftTaken` и `_rightTaken`): `nameleft`
  считается от `headerAvail`, а `statusleft`/`statuswidth` выставляет
  `centerStatus`, ограничивая ширину статуса оставшимся местом, поэтому
  `paintConnectingState`, `paintSendAction` и `paintStatus` получают уже
  сдвинутые координаты. Ветки report-reason, topic в `Section::Replies`,
  заголовок папки/replies и `namePeer` центрируются тем же способом, а
  `getMembersShowAreaGeometry` повторяет расчёт для клика по статусу. Режим
  `2` берёт `M3TitleStyle()` — static-копию `st::msgNameStyle` с подменённым
  шрифтом `st::topBarM3TitleFont` (`font(17px semibold)` в `info/info.style`,
  не в `chat.style`), тот же шрифт напрямую получает заголовок папки/replies,
  а смена стиля перестраивает `_title` по сравнению `_title.style()`.
  Узкий режим (narrow, `_narrowRatio`) и высота шапки не тронуты. Алиасы
  `iosChatHeader` (bool → `1`/`0`) и `newChatHeaderStyle` (int `0..2`,
  bool → `2`/`0`); в Chats пункт «Chat header style» с `Ui::Radioenum`
  Upstream/iOS/M3 и четыре lang-ключа
  `lng_settings_exteraless_chats_header_style*`; CLI `header-style` и
  `header-style-set`, `check_header_style` — 8 проверок плюс одна на неверный
  вызов CLI; раннеры 811/32/213 PASS (backup 101, CLI 61); `.extera` — 121
  ключ (116 + 3 + 2, сверено с legacy-кортежем `run_checks`); `kKeysCount`
  11204 (`kTagsCount` 185) — в счётчике остаётся retired-слот, чистая
  генерация дала бы 11203; sha256 патча `0dd9f6f7…`, `nix flake check` PASS.
  Ограничения: статус typing/connecting может слегка съезжать при центровке;
  в replies-чате бара нет, поэтому режим там не проявляется; Xvfb-смоук не
  выполнялся.
- Chats / Мини-аватарки отправителей (NB-62): `OEChatsSenderMiniAvatars` (bool,
  default `false`, алиас `senderMiniAvatars`) рисует мини-аватарку отправителя
  в превью диалога. Пейнт стоит в `PaintRow`
  (`dialogs/ui/dialogs_layout.cpp`) перед `paintItemCallback`: `sender` берётся
  как `item->displayFrom()` при предикате (превью не скрыто, автор поста не
  спрятан, отправитель не обычный пользователь, кроме гостевого бота), а
  отрисовка требует места под аватарку — `itemWidth` не меньше `skip` плюс
  `st::dialogsTextWidthMin`; затем сдвигаются `itemLeft` и `itemWidth`, поэтому
  имя и превью ужимаются рядом. Кэш `PeerUserpicView` держит `BasicRow`
  (`dialogs/dialogs_row.h`) и сбрасывает его при смене пира. Четыре style-ключа:
  `dialogsSenderMiniAvatar` (16 px), `dialogsSenderMiniAvatarSkip` (2 px),
  `dialogsSenderMiniAvatarTop` (1 px) и `dialogsSenderMiniAvatarRight` —
  объявлен в `dialogs.style`, но не используется, аватарка привязана к левому
  краю имени. В Chats тумблер «Show mini avatars next to sender names» и
  lang-ключ `lng_settings_exteraless_chats_sender_mini_avatars`. `.extera` —
  120 ключей (115 + 3 + 2, сверено с legacy-кортежем `run_checks`), CLI
  `sender-mini-avatars` и `sender-mini-avatars-set`,
  `check_sender_mini_avatars` — 5 проверок плюс одна на неверный вызов CLI;
  раннеры 802/32/213 PASS (backup 101, CLI 60), `kKeysCount` 11200
  (`kTagsCount` 185), sha256 патча `105a17c7…`, `nix flake check` PASS.
  Ограничения: значение применяется при следующей отрисовке строки —
  принудительного репейнта нет, поэтому открытый список чатов обновится на
  ближайшем событии; фото отправителя подтягивается при первом показе, до этого
  рисуются инициалы; аватарки нет у постов со скрытым автором и там, где превью
  скрыто (`hideSender`); Xvfb-смоук не выполнялся.
- General / Канал обновлений (NB-54): `OEGeneralAutoUpdateChannel` (int
  `0..2`, default `1`; `0` — off, `1` — release, `2` — beta, значение
  клампится) задаёт канал релизов, алиас `autoUpdateChannel`. Разбор списка
  релизов — Qt-only модуль `exteraless/update_check.{h,cpp}`:
  `Exteraless::ParseReleasesJson(data, current, includePrerelease)` берёт самый
  новый подходящий тег (draft отбрасывается всегда, prerelease — только при
  `includePrerelease`), `ReleasesApiUrl()` возвращает адрес API releases; сети
  модуль не делает. В Exteraless General пункт «Update channel» с
  `Ui::Radioenum` Off/Release/Beta и четыре lang-ключа
  `lng_settings_exteraless_update_channel` (`_off`, `_release`, `_beta`).
  `.extera` — 119 ключей (114 + 3 + 2, сверено с legacy-кортежем
  `run_checks`), CLI `update-channel`, `update-channel-set`, `update-check`
  (разбор без сети), `check_update_channel` — 19 проверок (16 + 3 на неверный
  вызов CLI); раннеры 796/32/213 PASS (backup 101), `kKeysCount` 11199
  (`kTagsCount` 185), sha256 патча `a44a72bb…`, `nix flake check` PASS.
  Ограничения: автозагрузки и установки нет — объём задачи ограничен
  нотификатором (проверка версии и ссылка на страницу releases), но
  потребителя в UI пока нет: парсер вызывается только из CLI, сетевых
  запросов модуль не делает, релизов у форка пока 0, поэтому реальный ответ
  API не проверялся; `ReleasesPage()` и `kReleasesApi` — константы, зеркала и
  другие хосты не поддерживаются; Xvfb-смоук не выполнялся.
- Text / Персидский календарь (NB-58): `OEGeneralUsePersianCalendar` и
  `OEGeneralPersianCalendarLatin` (обе bool, default `false`) переводят даты на
  Solar Hijri. Строку собирает
  `Exteraless::PersianDateText(date, latin, withYear)`
  (`exteraless/format.{h,cpp}`) через `QCalendar(QCalendar::System::Jalali)`:
  таблица 12 персидских месяцев с латинскими именами, цифры — ASCII при
  `latin`, иначе `U+06F0..U+06F9`; невалидная дата даёт пустую строку. В
  `lang/lang_keys.cpp` подмена стоит в `langDayOfMonth`, `langDayOfMonthFull`,
  `langDayOfMonthShort`, `langDateTime` и `langDateTimeFull` — меняется только
  `lt_date`, время и порядок строки остаются как в upstream. Алиасы
  `usePersianCalendar` и `displayPersianCalendarByLatin`. В Exteraless General
  два тумблера, второй доступен только при включённом первом; два lang-ключа
  `lng_settings_exteraless_persian_calendar` и
  `lng_settings_exteraless_persian_calendar_latin`. `.extera` — 118 ключей
  (113 + 3 + 2, сверено с legacy-кортежем `run_checks`), CLI `persian-date`,
  `persian-calendar`, `persian-calendar-set`, `check_persian` — 15 проверок;
  раннеры 776/32/213 PASS (backup 100), `kKeysCount` 11195 (`kTagsCount` 185),
  sha256 патча `daa23e68…`, `nix flake check` PASS. Ограничения: показывать ли
  год, решает григорианский год, а персидский меняется на Наврузе, поэтому у
  дат около смены персидского года метка года может отсутствовать; RTL и
  порядок не переупорядочиваются — строка отдаётся готовой; вендорные
  `lng_month*` персидским путём не используются; смена настройки видна после
  переоткрытия чата; Xvfb-смоук не выполнялся.
- Media / Качество и форматы (NB-88): `OEGeneralDefaultVideoQuality` (int
  `0..2`, default `0`) задаёт стартовое HLS-качество — `0` оставляет upstream
  480p, `1` — 720p, `2` — 1080p (`initialPlaybackVideoQuality` в
  `media/view/media_view_overlay_widget.cpp`, `Settings::videoQuality()` в
  `core/core_settings.cpp`). `OEGeneralHardwareAcceleration` (bool, default
  `true`) работает как алиас поверх upstream-настройки: воспроизведение требует
  `hardwareAcceleratedVideo() && hardwareAcceleration()` в `restartAtSeekPosition`
  оверлея и PiP, а тумблер в настройках пишет оба значения.
  `OEChatsTakeGifAsVideo` (bool, default `false`) отменяет конверсию GIF в
  `video/mp4` в `FileLoadTask::process` (`storage/localimageloader.cpp`).
  `OEGeneralCustomAudioBitrate` (int `16/24/32/48`, default `32`, остальное
  сводится к `32`) задаёт `bit_rate` записи Opus в `InitializeFFmpeg`
  (`media/audio/media_audio_capture.cpp`); если `avcodec_open2` не принимает
  выбранный битрейт, поток один раз переоткрывается на 32 kbps. Алиасы
  `defaultHlsVideoQuality`, `playerDecoder`, `takeGIFasVideo` и
  `customAudioBitrate`. В Exteraless — подсекция «Media»: селектор качества
  Auto/720p/1080p, тумблер Hardware video decoding, тумблер Send GIFs as video
  и селектор 16/24/32/48 kbps; шесть lang-ключей
  `lng_settings_exteraless_media*`. `.extera` — 116 ключей (111 + 3 + 2,
  сверено с legacy-кортежем `run_checks`), CLI четыре группы
  (`media-quality`, `hw-accel`, `gif-as-video`, `audio-bitrate`),
  `check_media_quality` — 19 проверок; раннеры 761/32/213 PASS (backup 100),
  `kKeysCount` 11193 (`kTagsCount` 185), sha256 патча `c647f76d…`,
  `nix flake check` PASS. Ограничения: ручной тир (1/2) отключает адаптивное
  переключение — на слабой сети это может ухудшить просмотр; повышенный
  битрейт может упасть на `avcodec_open2`, тогда запись идёт на 32 kbps без
  сообщения; GIF-как-видео теряет no-sound-спойлер и уходит обычной отправкой
  видео; MP4-как-видео не делалось — это уже дефолт upstream, а
  `enhancedVideoBitrate`, `alwaysSendInHD` и `preferredOriginalQuality` —
  отдельные ключи; Xvfb-смоук и сетевой замер не выполнялись.
- Chats / Скругления аватарок (NB-61): `OEChatsAvatarRadius` (int `-1..32`,
  default `-1`; `-1` — круг как в upstream, `0` — квадрат, `N` — радиус в px)
  задаёт форму загруженного и пустого юзерпика. Выбор маски — в
  `Ui::ValidateUserpicCache` (`ui/userpic_view.cpp`): при `radius >= 0`
  `Images::Round`/`EmptyUserpic::paintRounded`, иначе `Images::Circle`/
  `paintCircle`; радиус входит в ключ кэша (`PeerUserpicView::radius`), поэтому
  смена значения перегенерирует картинку. Алиасы: `avatarCorners` и
  `singleCornerRadius` — абсолютный радиус (кламп `-1..32`), `avatarSquareness`
  (0..1) пересчитывается как `round(32 * (1 - q))`. В Chats — пункт «Avatar
  corners» с `Ui::Radioenum` Circle/Square/Rounded 4/8/12/16/24 плюс отдельная
  строка для произвольного текущего значения; четыре lang-ключа
  `lng_settings_exteraless_chats_avatar_corners*` (`_rounded` с `lt_radius`,
  значение подставляется `QString::number`). `.extera` — 112 ключей, CLI
  `avatar-corners` и `avatar-corners-set`, `check_avatar_corners` — 7 проверок;
  раннеры 742/32/213 PASS (backup 100), `kKeysCount` 11187, sha256 патча
  `b58e4a17…`, `nix flake check` PASS (прогон из батча, здесь не повторялся).
  Ограничения: форма применяется при следующей отрисовке — уже нарисованные
  аватарки не перерисовываются мгновенно; `Data::Peer::GenerateUserpicImage` и
  `UserpicButton` (`useForumShape`) остались круглыми; онлайн-точка не тронута;
  Xvfb-смоук не выполнялся.
- Media / Внешние субтитры (NB-32), фаза 2: рендер, меню и память по видео.
  `media_subtitles.{h,cpp}` (`Telegram/CMakeLists.txt`) добавляют
  `Exteraless::Subtitles`: он помнит путь, метку и разобранные реплики для
  каждого видео, а выбор переживает перезапуск — карта `doc_<id>` →
  `{path, label}` лежит в `OESubtitlesFiles` (JSON-объект, не больше
  128 видео, ключ проверяет `ValidVideoKey`). При импорте `.extera` карта
  проходит через `SanitizeSubtitlesFiles` — принимаются только строковые
  `path` и `label`, записи без пути отбрасываются, а
  `Config::setSubtitlesFiles` не сохраняет не-объект. Второй ключ —
  `OEMediaViewerMenuSubtitles` (bool, default `true`) с тумблером в
  подсекции «Media viewer menu»; lang-ключей три —
  `lng_mediaview_subtitles_choose`, `lng_mediaview_subtitles_disable` и
  `lng_settings_exteraless_media_menu_subtitles`. В меню медиавьюера два
  пункта: «Choose Subtitles File» открывает `FileDialog` с фильтром
  `*.srt *.vtt`, а «Disable Subtitles» показывается только при выбранном
  файле; гейт `canShowSubtitles()` — видео-документ без webpage и без
  стрима. Позиция приходит из `UpdateVideo` в `handleStreamingUpdate` и из
  `updatePlaybackState`, а `refreshSubtitlesText` перерисовывает оверлей
  только когда текст активной реплики сменился. Рендер —
  `paintSubtitles(QRect, float64)` в `Renderer` и в трёх бэкендах (raster,
  OpenGL со своим `_subtitlesImage`, RHI), контент общий
  (`paintSubtitlesContent`): скруглённый прямоугольник
  `mediaviewSubtitleBg` и текст `mediaviewSubtitleFg` — алиасы
  `mediaviewCaptionBg`/`mediaviewCaptionFg` — по центру над нижней панелью.
  Вызов стоит после opacity-блока, поэтому fullscreen (`opacity = 0`) тоже
  видит субтитры. `.extera` — 111 ключей (`exteraconfig` 106 +
  `pillstackconfig` 3 + `mainconfig` 2, сверено с legacy-кортежем
  `run_checks` `(106, 3, 2)`); раннеры 734/32/213 PASS (backup 100),
  `kKeysCount` 11183, sha256 патча `80ade745…`. Ограничения: PiP и stories
  не покрыты, preload и streaming остались на 128K, CLI и раннеры проверяют
  только парсер — отдельной проверки меню и пер-видео памяти нет,
  Xvfb-смоук не выполнялся, `nix flake check` не запускался в docs-задаче;
  пикер для `alwaysAsk` — это NB-55, не эта задача.
- Chats / Внешние субтитры (NB-32), фаза 1: парсер
  `exteraless/subtitles.{h,cpp}` — `ParseSubtitles`, `ActiveCueText` и
  `VideoKey`. Читаются SRT и WebVTT: тайм-код принимает `.` и `,`, номер
  реплики необязателен, заголовок `WEBVTT` и директивы `NOTE`/`STYLE`/
  `REGION` пропускаются, теги `<...>` и `{...}` вырезаются, CRLF и CR
  сводятся к LF, а декодер понимает UTF-8 с BOM и без, плюс UTF-16 LE/BE по
  BOM. Реплики сортируются по старту, активный интервал end-exclusive
  (`position < end`), перекрывающиеся реплики склеиваются через `\n`.
  Шесть фикстур (`basic.srt`, `basic.vtt`, `overlap.srt`, `tags.srt`,
  `broken.srt`, `bom-utf16.srt`), CLI `subtitles-parse`, `subtitles-active` и
  `subtitles-key`; `check_subtitles` — 48 проверок, итого 734 PASS (backup 100,
  CLI 58) плюс 32 и 213 у плагинов. `.gitattributes` закрепляет
  `exteraless/tests/chats/fixtures/*.srt -text`, иначе CRLF-фикстура
  схлопнется при checkout. Новых `OE`-ключей нет, `.extera` не менялся;
  sha256 патча `8c587ce7…`, `nix flake check` PASS. Фаза 2 (рендер в четырёх
  бэкендах, меню выбора/отключения и пер-видео память) не сделана — follow-up.
- General / Bot privacy (NB-55): `OEGeneralBotPrivacyEnabled` (bool, default
  `false`), `OEGeneralBotPrivacyAccount` (int `-1..15`, default `-1`, 1-based
  индекс `orderedAccounts`) и `OEGeneralBotPrivacyAlwaysAsk` (bool, default
  `false`) — ссылка на бота открывается в выбранном аккаунте. Хук
  `RouteBotToPrivacyAccount` в `SessionNavigation::showPeerByLinkResolved`
  (`window/window_session_controller.cpp`) стоит перед всеми пятью ветками
  резолва, переключает аккаунт через `Core::App().domain().maybeActivate()` и
  отдаёт ссылку контроллеру целевого окна; `index <= 0` означает Off, индекс
  вне `orderedAccounts` даёт тост, а рекурсию отсекает сравнение аккаунта и
  контроллера. Алиасы `botPrivacyEnabled`, `botPrivacyUserId` и `alwaysAsk`
  принимаются в `.extera` (всего 109 ключей: 104 + 3 + 2, сверено с
  legacy-кортежем `run_checks`), в General — тумблер, селектор Off/1..N и
  тумблер «Always ask…», четыре lang-ключа, CLI — `bot-privacy` и
  `bot-privacy-set`; `check_bot_privacy` — 9 проверок плюс две на неверный
  вызов CLI, итого 683 PASS (backup 100) плюс 32 и 213 у плагинов;
  `kKeysCount` 11180, sha256 патча `ba0d94a7…`, `nix flake check` PASS.
  Follow-up: пикера для `alwaysAsk` пока нет — режим только маршрутизирует без
  вопроса; четыре тоста остаются английскими литералами без lang-ключей;
  `unblock` в текущем аккаунте при `startAutoSubmit` не сделан.
- Chats / Boost скачивания и загрузки (NB-76): `OEChatsDownloadSpeedBoost`
  (int `0..2`, default `0`) и `OEChatsUploadBoost` (bool, default `false`).
  Загрузка — пул сессий 1/2/4 и часть 128K/512K/1M (`BoostedDownloadPartSize()`
  и `StartSessionsCount()` в `storage/download_manager_mtproto.cpp`), окно
  ожидания считается в частях, а сами таймауты не менялись. Отдача — часть
  поднимается до 512K при капе 1M в `Uploader::Entry::setDocSize`
  (`storage/file_upload.cpp`), пейсинг не тронут. Алиасы
  `downloadSpeedBoost`/`UploadBoost` и legacy `OEGeneralDownloadSpeedBoost`
  принимаются в `.extera` (всего 106 ключей: 104 + 2, сверено с
  legacy-кортежем `run_checks`), в Chats — Radioenum Off/Boost/Ultra и
  тумблер, пять lang-ключей, CLI — четыре режима (`download-boost`,
  `download-boost-set`, `upload-boost`, `upload-boost-set`);
  `check_download_boost` — 10 проверок, итого 672 PASS (backup 100) плюс 32 и
  213 у плагинов; `kKeysCount` 11176, sha256 патча `7689cc10…`,
  `nix flake check` PASS. Ограничения: при boost > 0 CDN-редиректы могут не
  пройти hash-проверку (слайсинг CDN остался 128K); на слабой сети пул может
  сжаться, потому что пороги времени не менялись — нужен замер MB/s; смена
  boost посреди загрузки меняет шаг офсета; preload и streaming остались на
  128K; Xvfb-смоук и сетевой замер не выполнялись.
- Chats / Auto-download rules (NB-60): `OEChatsDisableAutoDownloadingArchive`
  (bool, default `false`) и `OEChatsBlockWin32ExecutableFiles` (bool, default
  `true`) — архив больше не скачивается автоматически, а Windows-исполняемые
  считаются опасными. Гейты — в `DocumentMedia::automaticLoad`
  (`data/data_document_media.cpp`) и `VideoPreload::Can`
  (`data/data_media_preload.cpp`); helper `Exteraless::IsWin32Executable`
  (`exteraless/links.cpp`) держит свой список из 18 расширений, поэтому
  глобальный `kExecutable` (`core/mime_type.cpp`) не тронут. Android-алиасы
  `disableAutoDownloadingArchive` и `Win32ExecutableFiles` принимаются в
  `.extera` (всего 104 ключа: 102 + 2, сверено с legacy-кортежем `run_checks`),
  два тумблера — в Chats, два lang-ключа в `lang.strings`, CLI —
  `autodownload-archive`, `autodownload-archive-set`, `win32-exe` и
  `win32-exe-set`; `check_autodownload_rules` — 9 проверок плюс две на неверное
  число аргументов, итого 660 PASS (backup 100, CLI 51) плюс 32 и 213 у
  плагинов; `kKeysCount` 11171, sha256 патча `baf908c7…`,
  `nix flake check` PASS. Ограничения: правила действуют на новые загрузки,
  уже скачанное не удаляется; `.exe` под Linux теперь тоже блокируется
  (раньше качался); photo-preload без имени файла не покрыт — пустое имя не
  блокируется; Xvfb-смоук не выполнялся.
- Chats / Мелочи чата (NB-86): три ключа — `OEChatsHideGreetingSticker` и
  `OEChatsHidePanelsOnScroll` (обе bool, default `false`) плюс
  `OEChatsChannelBottomButton` (int 0/1/2, default `1`). Приветственный стикер
  отсекается гейтом в `GenerateChatIntro`, панели закрываются через
  `hidePanelsAnimated` в `onScroll`, нижняя кнопка канала: `0` — скрыть,
  `1` — mute, `2` — Discuss (кнопка, навигация по `discussionLink`). Алиасы
  `disableGreetingSticker`, `hideKeyboardOnScroll`/`HideKeyboardOnChatScroll`,
  `bottomButton`/`BottomButton`, legacy `DisableChannelMuteButton` (true → 0).
  `.extera` — 102 ключа (99 + 3, сверено с legacy-кортежем `run_checks`), два
  тумблера и Radioenum Hide/Mute/Discuss, шесть lang-ключей, CLI 3 группы;
  `check_chat_misc` — 15 проверок, итого 649 PASS (backup 100) плюс 32 и 213 у
  плагинов; `kKeysCount` 11169, sha256 патча `0dff2727…`,
  `nix flake check` PASS. `keepAttachButton` и `quickTransition*` — N/A
  (кнопки/жеста нет). Ограничения: panels закрывает и автокомплит (проверить
  под Xvfb: автоскролл не должен закрывать), bottom hide уносит gift/direct-
  иконки, применяется после переоткрытия, Xvfb-смоук не выполнялся.
- Chats / Double tap actions: резолвер `exteraless/doubletap.{h,cpp}` понимает
  ещё шесть Android-действий — `translate` (3), `save` (5), `edit` (8),
  `delete` (10), `forward` (11) и `download` (12); 6/7/9 по-прежнему дают
  «ничего», а 2 остаётся `Send reaction` — расхождение с Android. Селекторы
  для всех шести — в подсекции Chats, шесть lang-ключей `double_tap_*` — в
  `lang.strings`; новых `OE`-ключей нет, `.extera` не менялся. CLI —
  `doubletap`, `doubletap-set` и `doubletap-resolve` (`resolve_cases` — 14,
  проверок doubletap 22 → 24), итого 629 PASS (backup 99) плюс 32 и 213 у
  плагинов; `kKeysCount` 11163, sha256 патча `900ed89d…`, `nix flake check`
  PASS. Ограничения: клик-хендлеры перехватывают часть действий, delete идёт
  через бокс подтверждения, repeat/LLM — N/A.
- Chats / Wide channel posts: настройка `OEChatsWideChannelPosts` (default
  `false`) снимает ограничение ширины баббла в broadcast-каналах, и пост
  растягивается на всю доступную ширину. Гейт — в `Message` рядом с
  `UnlimitedMessageWidth`: лимит `_bubbleWidthLimit` уходит в `0x3FFFFFF`,
  но только для broadcast — megagroup отсекается через `isBroadcast()`, а
  медиа-сообщения не затрагиваются. Android-алиасы `wideChannelPosts` и
  `WideChannelPosts` принимаются в `.extera` (всего 99 ключей: 94 в
  `exteraconfig`, 3 в `pillstackconfig`, 2 в `mainconfig`), тумблер «Wide
  channel posts» добавлен в Chats, lang-ключ
  `lng_settings_exteraless_chats_wide_channel_posts` — в `lang.strings`,
  CLI — `wide-posts` и `wide-posts-set`; `check_wide_posts` в
  `run_checks.py` — 5 проверок (дефолт, сеттер, персистентность, алиас и
  его значение), итого 627 PASS (backup 99) плюс 32 и 213 у плагинов;
  `kKeysCount` 11157, sha256 патча `c5309705…`, `nix flake check` PASS.
  Широкие посты ленты — N/A: лент в desktop нет. Ограничения: ширина
  применяется после переоткрытия чата или ресайза, кап `maxWidth` общий с
  `UnlimitedMessageWidth`, GUI-смоук под Xvfb не выполнялся.
- Chats / Reply style: три настройки `OEChatsReplyColors`, `OEChatsReplyEmoji`
  и `OEChatsReplyBackground` (все default `true`) — выключатели поверх текущего
  поведения цитаты ответа: при `false` цитата рисуется так же, как в upstream.
  Отрисовка — `history/view/history_view_reply.cpp`: у каждой цитаты свой
  `QuotePaintCache`, общий кэш не мутируется, а отдельный guard для custom
  emoji не нужен — эмодзи блока свои. Android-алиасы `replyColors`,
  `replyEmoji` и `replyBackground` принимаются в `.extera` (всего 96 ключей:
  91 в `exteraconfig`, 3 в `pillstackconfig`, 2 в `mainconfig`), при этом
  legacy-алиас `messageColoredBackground` сводится к `replyBackground` и
  отключает окраску только при явном `false`. Три тумблера добавлены в
  подсекцию Replies в Chats, четыре lang-ключа в `lang.strings`, CLI —
  `reply-style` и `reply-style-set`; `check_reply_style` в `run_checks.py` —
  7 проверок (дефолт, сеттер, персистентность, legacy-алиас, `OE`-ключ),
  итого 607 PASS (backup 97) плюс 32 и 213 у плагинов; `kKeysCount` 11154,
  sha256 патча `27098fd7…`, `nix flake check` PASS. Ограничения: медиа-карточки
  и ручные цитаты остаются цветными — осознанно; окраска применяется после
  переоткрытия чата (ширина цитаты считается от `_hasBackgroundEmoji`),
  GUI-смоук под Xvfb не выполнялся.
- Chats / Profile buttons: три настройки `OEChatsHideGiftButton`,
  `OEChatsHideCallButton` и `OEChatsHideChannelSearchButton` (все default
  `false`) убирают кнопки Gift и Call в профиле
  (`info/profile/info_profile_top_bar.cpp`), Call — в шапке чата, а Search — в
  шапке канала (`history/view/history_view_top_bar_widget.cpp`). Android-алиасы
  `hideGiftButtonInChannel`/`hideGiftButton`, `hideCallButton` и
  `hideChannelSearchButton` принимаются в `.extera` (всего 91 ключ: 86 в
  `exteraconfig`, 3 в `pillstackconfig`, 2 в `mainconfig`; экспорт пишет
  `OEChats*`), три тумблера добавлены в Chats, три lang-ключа в
  `lang.strings`, CLI — `profile-buttons` и `profile-buttons-set`;
  `check_profile_buttons` в `run_checks.py` — 5 проверок плюс проверка
  неверного числа аргументов, итого 582 PASS (backup 97) плюс 32 и 213 у
  плагинов; `kKeysCount` 11145, sha256 патча `486d82fd…`,
  `nix flake check` PASS. Money и Share в профиле desktop отдельных кнопок не
  имеют — N/A, как `hideCameraTile`. Ограничения: значение применяется после
  переоткрытия профиля или чата, `Ctrl+F` и меню «⋯» продолжают работать,
  `_groupCall` не трогали, GUI-смоук под Xvfb не выполнялся.
- Chats / Zalgo: настройка `OEChatsZalgoFilter` (default `false`) вычищает
  «zalgo»-текст из сообщений; модуль `exteraless/zalgo.{h,cpp}` находит серию
  из четырёх и более комбинирующих знаков (`Mark_NonSpacing`,
  `Mark_SpacingCombining`, `Mark_Enclosing`) и выбрасывает их, не разрывая
  суррогатные пары; текст короче 4 и длиннее 2048 символов не трогается.
  Вставка — в `Element::setTextWithLinks`
  (`history/view/history_view_element.cpp`) с пересчётом entities по карте
  смещений в стиле pangu: обнулившиеся entities отбрасываются, остальные
  сдвигаются и обрезаются. Android-алиас `zalgoFilter` принимается в `.extera`
  (всего 87 ключей; экспорт пишет `zalgoFilter`), тумблер «Filter Zalgo text»
  добавлен в Chats, CLI — `zalgo`, `zalgo-set`, `zalgo-apply` и `zalgo-tags`;
  `check_zalgo` в `run_checks.py` — 17 проверок (дефолт, пороги длины,
  суррогаты, сдвиг тегов, персистентность, алиас, ошибки CLI), итого 566 PASS
  плюс 32 и 213 у плагинов; `kKeysCount` 11141, sha256 патча `4d44da74…`,
  `nix flake check` PASS. Ограничения: матры арабского и индийских
  письменностей серией 4+ тоже вырезаются (паритет Android); в уже открытом
  чате фильтр применяется только при переоткрытии (live-сигнала нет);
  GUI-смоук под Xvfb не выполнялся.
- Chats / Stories header: настройка `OEChatsHideStoriesHeader` (default
  `false`) скрывает полосу историй над списком чатов. Android-алиасы
  `hideStoriesFromHeader` и `HideStoriesFromHeader` принимаются в `.extera`
  (всего 85 ключей; экспорт пишет `hideStoriesFromHeader`), тумблер добавлен
  в Chats, CLI — `storiesheader` и `storiesheader-set` (дефолт, сеттер,
  чтение из `.extera`, бэкап). Xvfb-план: полоса исчезает, строки списка не
  сдвигаются; GUI-смоук ещё не выполнялся, `out/bin/Telegram` собран до
  правок NB-10.
- General / Profile: настройка `OEGeneralShowIdDc` (default `false`) добавляет
  в профиль строку «ID and DC» вида `id: 123456 · DC 2`; по клику копируется
  «голый» id без префикса чата (`bareId`). Текст собирает `IdDcText`
  (`info/profile/info_profile_values.cpp`), формат — `Exteraless::FormatIdDc`
  (`exteraless/format.cpp`), строку выводят `makeInfo()` в
  `info/profile/info_profile_actions.cpp` и `SetupRows()` в
  `settings/sections/settings_information.cpp` (свой профиль). Строка не
  дублируется, если в экспериментальных настройках включён upstream-тумблер
  «Show peer ID below about» (`ShowPeerIdBelowAbout`). Android-алиасы
  `showIdAndDc` (bool) и `idDcType` (число) принимаются в `.extera` (всего
  86 ключей; экспорт пишет `showIdAndDc`), тумблер добавлен в General, CLI —
  `id-dc`, `id-dc-set` и `id-dc-format`. Xvfb-план: профиль контакта и свой,
  строка видна и `bareId` копируется; GUI-смоук не выполнялся,
  `out/bin/Telegram` (02:15) собран раньше правок NB-09.
- Chats / Sticker size: настройка `OEChatsStickerSize` (int, 4..20, default
  `14`) меняет размер стикеров в чате. В `Sticker::Size()`
  (`history/view/media/history_view_sticker.cpp`) множитель
  `1 + (v - 14) / 15` применяется к базе 224 px: при `4` выходит 75 px, при
  `20` — 314 px, результат клампится в 50..336 px и переводится через
  `style::ConvertScale`. При `14` работает прежний путь с
  `OptionStickerSize`, поэтому поведение по умолчанию не меняется. Android-алиас
  `stickerSize` принимается в `.extera` (дробное округляется), экспорт пишет
  `OEChatsStickerSize` (всего 88 ключей), слайдер «Sticker size» 4..20
  добавлен в Chats (`Ui::SettingsSlider`), lang-ключ
  `lng_settings_exteraless_chats_sticker_size`, CLI — `sticker-size` и
  `sticker-size-set`; `check_sticker_size` в `run_checks.py` — 10 проверок
  (дефолт, сеттер, персистентность, клампы 1→4 и 30→20, алиас с округлением,
  ключ `.extera`, экспорт), итого 576 PASS плюс 32 и 213 у плагинов;
  `kKeysCount` 11142, sha256 патча `c47276be…`, `nix flake check` PASS. Размер
  применяется при переоткрытии чата — live-сигнала нет; GUI-смоук под Xvfb не
  выполнялся.
- Chats / Sticker shape: настройка `OEChatsStickerShape` (int, `0..2`,
  default `0`) задаёт форму стикеров в чате: `0` — квадрат, как в upstream,
  `1` — скругление 4 px (`Ui::MsgFileThumbRadiusSmall`), `2` — «bubble»
  (`Ui::BubbleRadiusLarge`). Форма добавляет `QPainterPath` со скруглением и
  `setClipPath` в `paintAnimationFrame`, `paintPixmap` и `paintPath`
  (`history/view/media/history_view_sticker.cpp`); клип считается по
  фактическому rect отрисовки, а не по размеру картинки. При `0` клипа нет, а
  `customEmojiPart()` и `emojiSticker()` его пропускают — inline custom emoji
  и emoji-стикеры не трогаются. Android-алиас `stickerShape` принимается в
  `.extera` (дробное округляется, всего 93 ключа), выбор «Sticker shape»
  (Square/Rounded/Bubble) сделан на `Ui::Radioenum` в Chats, четыре
  lang-ключа в `lang.strings`, CLI — `sticker-shape` и `sticker-shape-set`;
  `check_sticker_size` в `run_checks.py` теперь покрывает и форму — 21
  проверка в группе `sticker-size`, итого 593 PASS (backup 97) плюс 32 и 213
  у плагинов; `kKeysCount` 11149, sha256 патча `9b497121…`,
  `nix flake check` PASS. NB-69 закрыт полностью — размер и форма. Не
  покрыто: premium-effect и панель стикеров — там клипа нет; dice
  клипуются, это сознательное решение. GUI-смоук под Xvfb не выполнялся.
- Chats / Poll results: настройка `OEChatsShowResultsBeforeVoting` (default
  `false`) показывает результаты опроса до голосования. Предикат
  `Poll::showResultsBeforeVote()` (`history/view/media/history_view_poll.cpp`)
  отдаёт результаты только для публичного опроса без голоса и без скрытых
  результатов, где `totalVoters > 0` и есть хотя бы один ответ с голосами;
  admin-режим, `Context::MediaEditor`, уже отданный голос, `Closed`,
  `HideResultsUntilClose`, `MultiChoice`, `totalVoters <= 0` и пустые `votes`
  предикат отсекает. `showVotes()` учитывает новый предикат, а `canVote()`
  смягчён до `!showVotes() || showResultsBeforeVote()`, поэтому голосование
  остаётся доступным. Android-алиас `showResultsBeforeVoting` принимается в
  `.extera` (всего 93 ключа: 88 в `exteraconfig`, 3 в `pillstackconfig`, 2 в
  `mainconfig`; сверено с legacy-кортежем `run_checks.py`), тумблер «Show poll
  results before voting» добавлен в Chats, lang-ключ
  `lng_settings_exteraless_chats_show_results_before_vote`, CLI —
  `show-results-before-voting` и `show-results-before-voting-set`;
  `check_show_results_before_voting` в `run_checks.py` — 7 проверок (дефолт
  off, сеттер, персистентность, алиас, `OE`-ключ), итого 600 PASS (backup 97)
  плюс 32 и 213 у плагинов; `kKeysCount` 11150, sha256 патча `39c78761…`,
  `nix flake check` PASS. Ограничения: multi-choice вне объёма, результаты
  применяются после переоткрытия или обновления опроса, сервер может не отдать
  счётчики (`hide_results_until_close` скрыт всегда), Xvfb-смоук не
  выполнялся.
- Chats / Archive and unread order: настройки `OEChatsHideArchive` и
  `OEChatsSortByUnread` (обе default `false`) скрывают папку архива из списка
  чатов и поднимают чаты с непрочитанными наверх. `hideArchive` идёт по уже
  существующему skip-пути `_skipTopDialog` в
  `InnerWidget::refreshWithCollapsedRows` (`dialogs/dialogs_inner_widget.cpp`)
  с внешним условием `archiveInMainMenu() || hideArchive`;
  `shouldBeInChatList` не трогали — иначе счётчики папок уходят в минус.
  `sortByUnread` — upstream-опция `Dialogs::kOptionDialogsUnreadOnTop`
  (`dialogs/dialogs_entry.cpp`): её значение засевается из конфига в
  конструкторе `Application` (`core/application.cpp`), поэтому индексный
  список и `dialogs_indexed_list.cpp` не менялись. Android-алиасы
  `hideArchive`/`HideArchive`/`hideArchiveFolder` и
  `sortByUnread`/`SortByUnread` принимаются в `.extera` (всего 98 ключей: 93 в
  `exteraconfig`, 3 в `pillstackconfig`, 2 в `mainconfig`; сверено с
  legacy-кортежем `run_checks.py`), два тумблера добавлены в Chats, два
  lang-ключа в `lang.strings`, CLI — `hide-archive`, `hide-archive-set`,
  `sort-unread`, `sort-unread-set` и `archive-skip`;
  `check_hide_archive_sort` в `run_checks.py` — 13 проверок плюс две проверки
  в `backup`, итого 622 PASS (backup 99) плюс 32 и 213 у плагинов;
  `kKeysCount` 11156, sha256 патча `24e1446f…`, `nix flake check` PASS.
  Ограничения: оба ключа применяются через `notifyListChanged`/пересорт, так
  что скрытие архива и новая сортировка видны сразу, без перезапуска; поиск и
  окно Archive архив по-прежнему видят; при `hideArchive` и выключенном
  `archiveInMainMenu` вернуть архив можно только тумблером; GUI-смоук под
  Xvfb не выполнялся.

## 2026-11-05

- Chats / Message menu: настройки `OEChatsGroupedMessageMenu` (default
  `false`) переносит пункт Delete в конец меню сообщения с разделителем, а
  `OEChatsDefaultDeleteMenu` (default `false`) запоминает выбор
  «удалить у всех / только у себя» автоматически, без галочки «Remember».
  Android-алиасы `groupedMessageMenu`/`groupMessageMenu` и
  `DefaultDeleteMenu` поддержаны в `.extera` (всего 84 ключа).
- Chats / Admin shortcuts: пять настроек `OEChatsShortcutAdministrators`,
  `OEChatsShortcutRecentActions`, `OEChatsShortcutStatistics`,
  `OEChatsShortcutPermissions`, `OEChatsShortcutMembers` (все default
  `false`) добавляют быстрые пункты Administrators, Recent actions,
  Statistics, Permissions и Members в начало меню «…» профиля канала/группы,
  если пользователь админ. Statistics показывается только при
  `CanGetStatistics`. Android-алиасы `ChannelAdministrators`, `EventLog`,
  `Statistics`, `ChannelPermissions`, `GroupMembers` поддержаны в `.extera`
  (всего 82 ключа).
- Appearance / Chat decoration: настройка `OEAppearanceChatDecoration`
  (default `0` = выключено) рисует снежинки (`1`), фейерверк (`2`) или декор
  по дате (`3`, снежинки в декабре и январе) поверх шапки чата; слой прозрачен
  для мыши и не меняет геометрию шапки. Android-алиасы `actionBarDecoration`
  и `chatDecoration` конвертируются в `.extera` (всего 77 ключей).
- Privacy / False bottom: настройка `OEPrivacyFalseBottom` (default `false`)
  добавляет в конец раздела Exteraless скрытую секцию «Двойное дно». Если
  задан локальный пароль, секция открывается только после его ввода; неверный
  пароль показывает ошибку, верный — перестраивает страницу с содержимым
  («Локальный пароль», «Заблокировать сейчас»). Пока секция закрыта, её
  содержимое не попадает в поиск по настройкам. Android-алиасы `falseBottom`
  и `FalseBottom` поддержаны в `.extera` (всего 76 ключей).
- Chats / Downloads: настройка `OEChatsAlwaysShowDownloadIcon` (default
  `false`) держит вкладку «Downloads» в поисковых подсказках и угловую иконку
  скачивания на уже загруженных аудиофайлах, чтобы их можно было сохранить в
  файл; у незагруженных файлов поведение прежнее. Android-алиасы
  `alwaysShowDownloadIcon` и `AlwaysShowDownloadIcon` поддержаны в `.extera`
  (всего 75 ключей), тумблер добавлен в Chats.
- Chats / Folders: настройка `OEChatsHideAllChatsFolder` (default `false`)
  скрывает папку «Все чаты» в левой панели вкладок/фильтров при наличии
  пользовательских папок; активный выбор автоматически переключается на первую
  доступную папку. Android-алиасы `hideAllTab`, `HideAllTab` и `hideAllChats`
  поддержаны в `.extera` (всего 74 ключа), тумблер добавлен в Chats.
  Стили вкладок (`tabsTitleType`) уже присутствуют в upstream tdesktop
  (`chatFiltersTabsMode`: текст, иконки, иконки+текст).
- Chats / Link preview: настройка `OEChatsDisableLinkPreviewByDefault` (default
  `false`) отключает автоматический поиск и добавление превью ссылок при вводе
  сообщений; ручное включение/выключение остаётся доступным по комбинации
  `Ctrl+R` (`ToggleWebPagePreview`). Android-алиасы `disableLinkPreviewByDefault`
  и `DisableLinkPreviewByDefault` поддержаны в `.extera` (всего 73 ключа),
  тумблер добавлен в Chats.
- Chat / Menu: четыре настройки `OEChatsChatMenuBoost`, `OEChatsChatMenuLinked`,
  `OEChatsChatMenuViewDeleted`, `OEChatsChatMenuClearDeleted` (все default
  `true`) управляют пунктами меню чата Boost, связанный чат, «Deleted
  messages» и «Clear deleted messages»; очистка удалённых теперь спрашивает
  подтверждение. Android-алиасы `chatMenuItem*` поддержаны в `.extera`
  (всего 72 ключа). `ToBeginning`, `GoToMessage`, `HideTitle`,
  `DeleteOwnMessages` и закладки не применимы — таких пунктов в desktop нет.
- Media viewer / Menu: четыре настройки `OEMediaViewerMenuForward`,
  `OEMediaViewerMenuCopyFrame`, `OEMediaViewerMenuCopyPhoto`,
  `OEMediaViewerMenuSetProfile` (все default `true`) скрывают пункты
  Forward, Copy Frame, Copy и Set as profile photo; Android-алиасы
  `mediaViewerMenuItem*` поддержаны в `.extera` (всего 68 ключей),
  чекбоксы — в подсекции «Media viewer menu». `NoQuoteForward` и
  `ScanQRCode` не применимы: в desktop таких пунктов нет.
- Chats / Forwards: настройка `OEChatsShowForwardDate` (default `false`)
  дописывает дату оригинала в заголовок «Forwarded from …» (`· 5 нояб.
  13:39`); Android-алиас `dateOfForwardedMsg` поддержан в `.extera` (всего
  64 ключа), тумблер в Chats.
- Chats / Edited mark: настройки `OEChatsUseEditedIcon` (иконка «✎» вместо
  слова) и `OEChatsCustomEditedMark` (свой текст, до 16 символов) заменяют
  метку «edited» в сообщениях; Android-алиасы `useEditedIcon`/
  `customEditedMessage` поддержаны в `.extera` (всего 63 ключа), в Chats
  добавлены тумблер и поле метки.
- Player / Seek: настройка `OEChatsSeekDuration` (5/10/15/30 с, default 5)
  задаёт шаг перемотки стрелками в медиавьюере (J/L — двойной шаг);
  Android-алиас `doubleTapSeekDuration` (индекс 0–3) поддержан в `.extera`
  (всего 61 ключ), селектор добавлен в Chats.
- Ghost Mode / Schedule: настройка `OEGhostScheduledSend` (default `false`)
  отправляет новые текстовые сообщения как отложенные на 12 секунд, когда
  online-ghost активен; личные чаты с ботом, self и секретные чаты исключены
  (`exteraless/ghost_schedule.{h,cpp}`); Android-алиас `GhostScheduledSend`
  поддержан в `.extera` (всего 60 ключей).
- General / Files: настройка `OEGeneralSaveToChatSubfolder` (default `false`)
  сохраняет автоматически скачиваемые файлы в подпапку с именем чата внутри
  папки загрузок (`exteraless/save_path.{h,cpp}`, безопасные имена, суффикс
  ` (2)` при коллизии); Android-алиас `saveToChatSubfolder` поддержан в
  `.extera` (всего 59 ключей). `customSavePath` отдельно не добавлялся:
  в desktop уже есть настройка «Download path» в Advanced.
- Chats / Inline math: настройка `OEChatsInlineMathResult` (default `false`)
  вычисляет выражение при нажатии Space после `=`: `2+2=` → `2+2=4 `.
  Парсер (`exteraless/math.{h,cpp}`) поддерживает `+ - * / % ^`, скобки,
  константы `pi`/`e` и функции (`sqrt`, `abs`, `sin`, `cos`, `tan`, `ln`,
  `log`, `exp`, `floor`, `ceil`, `round`); Android-алиас `inlineMathResult`
  поддержан в `.extera` (всего 58 ключей).
- Chats / Channels: настройка `OEChatsHideSendAsPeer` (default `false`)
  скрывает кнопку «Send as...» в каналах; Android-алиас
  `hideSendAsChannel` поддержан в `.extera` (всего 57 ключей). Из трёх
  ключей NB-35 перенесён только этот: камеры в attach-меню tdesktop нет, а
  «Share» в канале относится к профилю (NB-73).
- Plugins: в боксе плагинов появились «Install plugin...» (копирование
  `.plugin`/`.py` в `tdata/plugins` с дедупликацией имени и лимитом 2 МиБ) и
  «Delete plugin» с подтверждением; установка и удаление проверены в Xvfb.
  Логика вынесена в `exteraless/plugins/installer.{h,cpp}` и покрыта CLI
  `plugin-install`/`plugin-delete`/`plugin-filename` (19 проверок).
- Plugins: в боксе плагинов появилась кнопка «Audit log» — последние 24
  строки `tdata/plugins-audit.log` с возможностью экспорта через файловый
  диалог; Xvfb подтвердил отображение и диалог.
- Сборка: удалён устаревший untracked-symlink `result/` на старую Nix-сборку;
  актуальный клиент создаётся `nix build` по требованию.
- Docs: `feature-mapping.md` сверен с кодом — Deleted messages описывает
  схему v2 и метку в боксе, удалено несуществующее `pinnedReactions`-
  утверждение; hook-раздел соответствует `plugin_runtime.cpp`.
- Ghost: мёртвые ключи `OEGhostSendOfflineAfterOnline` и
  `OEGhostSendOfflineAfterOnlineLocked` удалены из конфига, `.extera` и UI;
  `.extera` теперь содержит 56 ключей. Семантика Android-настройки требует
  прямого управления статусом и в desktop не реализована.
- Pill Stack: кнопка «Reset pill order» возвращает стандартный порядок
  (`OEPillStackOrder`), показывает тост; CLI `pillorder-reset` (3 проверки).
  Бесконечная прокрутка не переносится: полоса пилюль переносит элементы на
  новую строку вместо постраничного листания.
- Appearance: `OEAppearanceHideAiEditor` (default `false`) скрывает кнопку
  AI-редактора в поле ввода и caption-боксах; Android-алиас `hideAiEditor`
  поддержан в `.extera` (всего 58 ключей). AI-саммари IV/сообщений в этой
  версии tdesktop отсутствуют, поэтому `HideIvSummary`/`HideMessageSummary`
  не добавлялись.
- Settings: настройки `OESettingsHidePremium` и `OESettingsHideHelp`
  (default `false`) скрывают секции Premium/Credits и Help на главном экране
  настроек; Android-алиасы `hidePremiumSection`/`hideHelpSection` поддержаны
  в `.extera` (всего 57 ключей), тумблеры в General.
- Chats / Silent send: настройка `OEChatsSilentByDefault` (default `false`)
  отправляет новые сообщения без звука через `SendOptions.silent` в
  `prepareSendAction` (`HistoryWidget` и `ChatWidget`); Android-алиас
  `silentMessageByDefault` поддержан в `.extera` (всего 55 ключей).
- Chats / Pangu: настройка `OEChatsPanguOnSending` (default `false`) добавляет
  пробелы между CJK и латиницей/цифрами при отправке; реализовано в
  `exteraless/pangu.{h,cpp}`, смещения text-tags корректируются, Android-алиас
  `EnablePanguOnSending` поддержан в `.extera` (всего 54 ключа).
- Патч: база закреплена в `nix/upstream-base` (коммит `aa09910`, tree
  `4cf9e0e1…`, upstream `6fed91f`); `nix/update-patch.sh` сверяет tree-хеш,
  предупреждает о сдвинутом теге, а `--check` применяет патч в чистом
  worktree и печатает sha256. Доказано, что снимок `v7.3.1` tree-идентичен
  upstream.
- Proxy: `ApplyProxyRules()` хранит автовыключение только в памяти и срабатывает
  один раз на переход условия; ручное включение прокси при активном условии
  больше не отменяется, а после перезапуска без условия прокси не включается
  сам. `OEProxyAutoDisabled` остаётся legacy-полем и не пишется; решение
  вынесено в `DecideProxyRule` и покрыто CLI `proxy-rule` (6 проверок).
- Network: `ppp*` (PPPoE) больше не считается VPN-интерфейсом в
  `IsVpnInterfaceName()`, поэтому авто-отключение прокси не срабатывает на
  PPPoE; CLI `vpn-name` (10 проверок).
- Config: удалён неиспользуемый `Config::load()`; загрузка остаётся ленивой
  через `ensureLoaded()`.
- Сборка: `nix/smoke-test.sh` запускает `out/bin/Telegram` и собирает
  `LD_LIBRARY_PATH` из `NIX_LDFLAGS`, smoke-тест проходит (30 с).
- Deleted messages / Mark: настройки `OEChatsTranslucentDeleted` (default
  `true`), `OEChatsUseDeletedIcon` (default `true`), `OEChatsCustomDeletedMark`
  (default пусто) и `OEChatsDeletedMarkColor` (default `0`); бокс удалённых
  сообщений показывает значок/кастомную метку, прозрачность 0.6 и цвет.
  Android-алиасы `TranslucentDeletedMessages`/`UseDeletedIcon`/
  `CustomDeletedMark`/`DeletedMarkColor` поддержаны в `.extera` (всего 53
  ключа). Inline-вставка удалённых сообщений в историю чата вынесена в NB-92.
- Deleted messages / Schema: сайдкар переведён на версию 2 с 17 полями —
  реакции, ответ (`replyToMsgId`/`replyToText`), форвард (`forwardedFrom`/
  `forwardedDate`), entities, дата правки, флаг прочтения; старые записи
  читаются с дефолтами, бокс показывает новые поля. Заодно исправлен захват
  `controller`/`peer` в пункте «Deleted messages» (раньше бокс мог не
  открыться или упасть после уничтожения `Filler`).
- Ghost Mode / Locks: пять «locked»-тумблеров (`OEGhostSendReadLocked`,
  `OEGhostSendStoriesLocked`, `OEGhostSendOnlineLocked`,
  `OEGhostSendUploadProgressLocked`, `OEGhostSendOfflineAfterOnlineLocked`;
  всего 49 ключей) защищают отдельные настройки Ghost от пакетного
  переключателя; Android-алиасы `send*Locked` поддержаны в `.extera`,
  подсекция «Locked Ghost settings» добавлена в настройки.
- Ghost Mode: пункт главного меню показывает статус `On` / `Off` / `Partial`
  (настройка `OEGhostShowStatus`, Android-ключ `showGhostModeStatus` в
  `.extera`, всего 44 ключа), один клик включает или выключает все четыре
  тумблера; `IsGhostActive` теперь строгий (все четыре), как в Android.
- Pill Stack / Order: пилюли можно перетаскивать прямо на полосе над списком
  чатов; порядок сохраняется в `OEPillStackOrder` (CSV), переживает рестарт и
  экспортируется в `.extera` (всего 43 ключа), в настройках добавлена подсказка
  о drag-сортировке; заодно исправлен порядок инициализации `PillStack`
  (перенос применения сохранённого порядка в конец конструктора), из-за
  которого приложение могло упасть на старте.
- Chats / Stickers: настройка `Sticker time` (`As usual`, `Beside sticker`,
  `Hidden`) управляет временем на стикерах через `OEChatsStickerTimeSide` и
  `OEChatsHideStickerTime`; Android-ключи `stickerTimeMode`/`hideStickerTime`
  поддержаны в `.extera` (всего 42 ключа), боковое время рисуется рядом со
  стикером и остаётся кликабельным.
- Chats / Double tap: настройки `OEChatsDoubleTapIn` и `OEChatsDoubleTapOut`
  задают действие двойного тапа по входящим и исходящим сообщениям
  (как в настройках Telegram, ничего, любимая реакция, ответ); Android-ключи
  `doubleTapAction`/`doubleTapActionOutOwner` поддержаны в `.extera` (всего
  40 ключей), добавлены два селектора в подсекцию Chats.
- General / Privacy: настройка `OEGeneralHidePhone` (default `false`) скрывает
  строку номера телефона в профиле контакта и своём профиле; Android-ключ
  `hidePhoneNumber` поддержан в `.extera` (всего 38 ключей), тумблер
  добавлен в General.
- General / Time: настройка `OEGeneralShowSeconds` (default `false`) и
  функция `Exteraless::FormatClock` форматируют время с секундами в
  сообщениях и списке чатов с сохранением 12/24-часового формата и локали;
  Android-ключ `formatTimeWithSeconds` поддержан в `.extera` (всего 37
  ключей), тумблер добавлен в General.
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
