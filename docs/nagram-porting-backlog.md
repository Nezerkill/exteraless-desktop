# Порт-бэклог: Exteraless/NagramX → Exteraless Desktop

Дата: 2026-11-04. Документ превращает обзор из
`docs/nagram-feature-audit.md` в приоритизированный список задач. Каждая
строка — одна задача с Android-источником, точкой интеграции в desktop,
классом размера, риском, способом проверки и текущим статусом. Краткий
порядок исполнения и рабочий цикл — в `docs/nagram-master-plan.md`.

Задачи не дублируют друг друга: если отчёт 1 и отчёт 2 предлагали одно и то
же, строка одна, а в колонке «Источник» стоят оба идентификатора. Если отчёты
расходились в приоритете, в колонке «Источник» указано, чья оценка взята и
почему (обычно берётся более высокий приоритет, а риск — по более
осторожной оценке).

## 1. Как читать таблицы

| Поле | Значение |
|---|---|
| `ID` | идентификатор задачи в этом бэклоге; не менять, на него ссылаются коммиты и заметки |
| `Android key(s)` | ключи Android-клиента, из которых берётся поведение и дефолт |
| `Desktop point` | файл и функция, куда ложится изменение |
| `Размер · риск` | `S` — до нескольких десятков строк, `M` — один-два модуля, `L` — новая подсистема, `XL` — новый архитектурный слой; риск: низкий / средний / высокий |
| `Smoke` | минимальная проверка, доказывающая поведение |
| `Статус` | см. легенду ниже |
| `Источник` | `R1 P0-N`, `P1-N`, `P2-N` — бэклог первого отчёта; `R2 M-N`, `R2 §N` — раздел второго |

| Статус | Значение |
|---|---|
| ✅ | реализовано и проверено |
| 🟡 | реализовано частично, ограничения записаны |
| 🔧 | было сломано, исправлено в текущем дереве |
| ⬜ | открыто |

Критерии P0 по первому отчёту: только локальные данные, нет новых сетевых
вызовов, нет новых подсистем, изменение обратимо, smoke воспроизводим на
существующем Xvfb-профиле. Второй отчёт добавляет к P0 требование сначала
закрыть дефекты данных и приватности, а уже потом брать новые функции.

| Приоритет | Задачи | Смысл |
|---|---|---|
| P0 | NB-01 … NB-18 (18) | корректность, данные, приватность и дешёвые локальные настройки |
| P1 | NB-19 … NB-46 (28) | доверие к сборке, документация, средние подсистемы |
| P2 | NB-47 … NB-90 (44) | паритет функций и крупные подсистемы |
| Вне бэклога | раздел 6 | Android-only, plugin/Xposed, нереализуемое |

Отдельно: `lib_ui` — сабмодуль, он **не попадает** в
`nix/patches/exteraless.patch`, потому что патч собирается по `CMakeLists.txt`,
`Telegram/` и `exteraless/`. Всё, что требует правок в `Telegram/lib_ui/**`,
не доедет до Nix-пользователей, и в таблицах это отмечено.

## 2. Текущий статус: что уже есть

Это не задачи, а база, на которую опирается бэклог.

| Функция | Ключи | Точка входа | Статус |
|---|---|---|---|
| Relative last seen | `OERelativeLastSeen` | `data/data_peer_values.cpp` (`OnlineText`), top bar, профиль, превью | ✅ |
| Comma after mention | `OEAddCommaAfterMention` | `chat_helpers/field_autocomplete.cpp`, `window/window_peer_menu.cpp` | ✅ |
| Exact counters | `OEGeneralDisableNumberRounding` | `lang/lang_tag.cpp` (`FormatCountToShort`) | ✅ |
| Ghost: read / stories / online / typing | `OEGhostSendReadPackets`, `OEGhostSendReadStories`, `OEGhostSendOnline`, `OEGhostSendUploadProgress` | `exteraless/ghost_mode.cpp`, `mtproto/mtproto_request_filter.h`, `apiwrap.cpp`, `api/api_views.cpp`, `api/api_updates.cpp`, `api/api_send_progress.cpp`, `data/data_stories.cpp` | ✅ 4/4; мёртвый offline-ключ удалён в NB-38 |
| Ghost: просмотры и подписи тумблеров | `OEGhostSendReadPackets` | `api/api_views.cpp` (`ViewsManager::viewsIncrement`), `settings/sections/settings_exteraless.cpp` | 🔧 исправлено в `123a29f` |
| Ghost per-chat exclusions | `OEGhostReadExclusions`, `OEGhostTypingExclusions` | `exteraless/config.cpp`, подменю в `window/window_peer_menu.cpp` | 🟡 UI и запись есть, сетевой эффект не измерялся |
| Deleted messages | `OESaveDeletedMessages` | `exteraless/deleted_messages.cpp`, `boxes/deleted_messages_box.cpp`, `window/window_peer_menu.cpp` | 🟡 9 полей схемы; гейт, лимит и ротация закрыты в NB-02 |
| Remove message tail | `OEChatsRemoveMessageTail` | `history/view/history_view_message.cpp` (`skipTail`) | ✅ |
| Hide reactions (channel/group/private) + hover-кнопка | `OEChatsHideReactionsChannels/Groups/Private` | `exteraless/chats.cpp`, `history/view/history_view_element.cpp`, `history_view_message.cpp` | ✅ |
| Strip tracking on open | `OEChatsStripTrackingOnOpen` | `exteraless/links.cpp`, `core/ui_integration.cpp` | ✅ |
| Strip tracking on paste | `OEChatsStripTrackingOnPaste` | `exteraless/links.cpp`, `chat_helpers/message_field.cpp` | ✅ |
| Compact unread label | `OEChatsUnreadCapsule` | `history/view/history_view_element.cpp` (`UnreadBar::paint`), `ui/chat/chat.style` | ✅ |
| Drawer: аккаунты, бейджи, порядок, collapse | `OEDrawerAccountsExpanded`, `OEMainMenuOrder` | `window/window_main_menu.cpp`, `settings/sections/settings_information.cpp` (`AccountsOptions`) | ✅ |
| Pill Stack: 8 пилюль | `OEPillStackEnabled`, `OEPillStackPills` | `Telegram/SourceFiles/exteraless/pill_stack.{h,cpp}`, `dialogs/dialogs_widget.cpp` | ✅ (CPU/RAM/Net/DC/Proxy/Cache/Ghost/LastSeen) |
| Plugins stage 1–2 | `OEPluginStates` | `exteraless/plugins/scanner.cpp`, `Telegram/SourceFiles/exteraless/{plugin_runtime,plugins_box}.{h,cpp}` | ✅ по раннерам; разрешения информативные |
| `.extera` backup | — | `exteraless/backup.cpp` | ✅ 25 ключей, три секции (NB-04) |
| Брендинг | — | `core/launcher.cpp`, `boxes/about_box.cpp`, `core/version.h`, `Telegram/build/version` | ✅ |
| Proxy auto-disable | `OEProxyDisableConditions`, `OEProxyAutoDisabled` | `exteraless/proxy_rules.cpp`, `exteraless/network.cpp`, `core/application.cpp` | ✅ |
| WEB proxy | — | `mtproto/web_proxy/*`, `mtproto/details/mtproto_web_proxy_socket.{h,cpp}`, `boxes/connection_box.cpp` | 🟡 код и сборка есть, relay-сервер не развёрнут; в docs не описан |
| Локализация секции | — | `settings/sections/settings_exteraless.cpp`, `Telegram/Resources/langs/lang.strings` | ✅ 41 ключ `lng_settings_exteraless*` в `lang.strings`, все подписи в `settings_exteraless.cpp` переведены на `tr::` |

Проверки, на которые опирается статус: `exteraless/tests/chats/run_checks.py`
(527 проверок: strip, reactions, cli, config, general, deleted, deleted-entry, reset, workdir, menu, doubletap, stickertime, pillorder, ghost, deletedmark, vpn, proxy-rules, pangu, silent, sections, ai-editor, pluginops, send-as, inline-math, save-subfolder, ghost-schedule, seek-duration, edited-mark, forward-date, media-menu, chat-menu, disable-link-preview, hide-all-chats, always-show-download, false-bottom, backup),
`exteraless/tests/plugins/run_checks.py` (32) и
`exteraless/tests/plugins/run_host_checks.py` (213 — bwrap-host, IPC, пути,
timeout, state). Все три прогоняются на текущем дереве со статусом PASS.

## 3. P0 — корректность, данные, дешёвые настройки

| ID | Задача | Android key(s) | Desktop point | Размер · риск | Smoke | Статус | Источник |
|---|---|---|---|---|---|---|---|
| NB-01 | Ghost: инверсия счётчика просмотров и подписей тумблеров | `sendReadMessagePackets`, `messages.getMessagesViews` | `api/api_views.cpp` (`ViewsManager::viewsIncrement`), `settings/sections/settings_exteraless.cpp`, патч `nix/patches/exteraless.patch` | S · низкий | дефолтный конфиг: просмотры канала уходят; Xvfb: подписи «Don't send …» соответствуют состоянию | 🔧 исправлено в `123a29f` | R2 M1 |
| NB-02 | Deleted messages: читать `OESaveDeletedMessages`, добавить лимит и ротацию файла | `enableSaveDeletedMessages` | `exteraless/deleted_messages.cpp`, `exteraless/deleted_messages_trim.h`, `exteraless/config.cpp`, `settings/sections/settings_exteraless.cpp` | S–M · средний (данные, приватность, рост файла) | CLI: при выключенном ключе ничего не пишется; cap ротации; Xvfb: тумблер | ✅ 2026-11-05: гейт default `false`, лимит 1000/100, ротация при add/load (load только при включённом ключе), тумблер Chats, CLI `deleted*` (27 проверок); Xvfb: тумблер включается/выключается и пишет ключ | R2 M2 |
| NB-03 | Локализовать секцию Exteraless: захардкоженные подписи → `lang.strings` | `strings_oe*.xml` | `settings/sections/settings_exteraless.cpp`, `Telegram/Resources/langs/lang.strings` | S · низкий | Xvfb на ru/en: нет английских подписей; поиск по настройкам не падает | ✅ 2026-11-05: 30 новых ключей `lng_settings_exteraless_*` (всего 41), все подписи переведены на `tr::`, пересобран `td_lang` и `lang_instance`, Xvfb smoke | R1 P0-1, R2 M9 |
| NB-04 | Полный экспорт/импорт `.extera` для всех 25 ключей с отчётом о неизвестных | `EtgBackup`, `InvisibleEncryptor` | `exteraless/backup.cpp`, `exteraless/config.cpp` | S · низкий | `exteraless/tests/chats/run_checks.py` + CLI-импорт файла с новыми ключами | ✅ 2026-11-05: 25 ключей в секциях `exteraconfig`/`pillstackconfig`/`mainconfig`, валидация типов, целочисленности и диапазонов, санитизация exclusions/plugins, сброс производного `OEProxyAutoDisabled`, unknown с префиксом секции; round-trip CLI | R1 P0-2 |
| NB-05 | Сброс настроек Exteraless (кнопка + подтверждение) | `resetSettings` | `settings/sections/settings_exteraless.cpp`, `exteraless/config.cpp` | S · низкий | сброс → дефолты, `exteraless.json` без OE-ключей | ✅ 2026-11-05: `Config::reset()`, кнопка Reset с подтверждением, пересоздание секции, CLI-группа `reset` (7 проверок), Xvfb smoke | R1 P0-3 |
| NB-06 | Путь конфига, совместимый с `-workdir` | — | `exteraless/config.cpp` (путь данных), `core/application.cpp` | S · средний (миграция существующего файла) | запуск с `-workdir /tmp/x`: файл создаётся там, старый профиль читается | ✅ 2026-11-05: `Config::setWorkingDir()` + `cWorkingDir()` из Application, копирование старого файла при первом запуске, CLI `EXTERALESS_WORKDIR` (8 проверок), Xvfb-миграция на копии профиля | R1 P0-4 |
| NB-07 | Время с секундами | `NekoConfig.showSeconds` | `exteraless/format.{h,cpp}`, `history/view/history_view_bottom_info.cpp`, `ui/text/format_values.cpp`, `settings/sections/settings_exteraless.cpp` | S · средний (время рисуется в нескольких местах) | CLI форматтера (`HH:mm` / `HH:mm:ss`, полночь, локаль) + Xvfb: чат и список | ✅ 2026-11-05: `OEGeneralShowSeconds`, `Exteraless::FormatClock`, тумблер в General, CLI general/clock (17 проверок), Xvfb показал секунды в чате и списке | R1 P0-5, R2 §6.2 |
| NB-08 | Фильтр Zalgo | `NaConfig.zalgoFilter` | `ui/text/*`, `data/data_messages.cpp` | S · средний (ломает раскладку при агрессивном фильтре) | CLI чистой функции + Xvfb: текст с комбинирующими знаками ровно рендерится, БД не меняется | ⬜ | R1 P0-6 |
| NB-09 | Показать ID и DC в профиле | `NekoConfig.showIdAndDc`, `NaConfig.idDcType` | `info/profile/info_profile_values.cpp`, `info/profile/info_profile_top_bar.cpp` | S · низкий | Xvfb: профиль контакта и свой, значение копируется | ⬜ | R1 P0-7 |
| NB-10 | Скрыть истории в шапке списка чатов | `NaConfig.hideStoriesFromHeader` | `dialogs/dialogs_widget.cpp` (stories strip) | S · низкий | Xvfb: полоса историй исчезает, строки не сдвигаются | ⬜ | R1 P0-8 |
| NB-11 | Видимость пунктов меню сообщения (группа чекбоксов) | ~24 ключа `NekoConfig.show*`, `NaConfig.show*`, `ActionBarButton*` | `exteraless/message_menu.h`, `exteraless/config.{h,cpp}`, `history/history_inner_widget.cpp`, `history/view/history_view_context_menu.cpp`, `settings/sections/settings_exteraless.cpp` | M · средний (меню собирается по условиям) | CLI: дефолты и roundtrip `toJson`/`fromJson`; Xvfb: каждый тумблер убирает свой пункт, горячие клавиши не ломаются | ✅ 2026-11-05: 11 чекбоксов `OEChatsMenu*`, применены в обоих контекстных меню, CLI `menu` (8 проверок), Xvfb подтвердил скрытие Pin в чате | R1 P0-9, R2 §6.1 |
| NB-12 | Действие двойного тапа: ответ / реакция / ничего | `NaConfig.doubleTapAction`, `doubleTapActionOut` | `exteraless/doubletap.{h,cpp}`, `history/history_inner_widget.cpp`, `history/view/history_view_list_widget.cpp`, `settings/sections/settings_exteraless.cpp` | M · средний (жест пересекается с выделением текста) | Xvfb: двойной тап даёт reply / реакцию / ничего | ✅ 2026-11-05: `OEChatsDoubleTapIn`/`OEChatsDoubleTapOut` (-1 = настройка Telegram, 0 = ничего, 1/2 = реакция, 4 = ответ), селекторы в Chats, CLI `doubletap` (22 проверки), Xvfb подтвердил reply/reaction/nothing; остальные Android-действия — NB-91 | R1 P0-10 |
| NB-13 | Широкие посты каналов и ленты | `OEChatsWideChannelPosts`, `OEChatsWideFeedPosts` | `history/view/history_view_message.cpp` (ширина баббла) | M · высокий (геометрия, хвосты, медиа) | Xvfb: пост на всю ширину, ответы и медиа не ломаются | ⬜ | R1 P0-11 |
| NB-14 | Время на стикерах: сбоку или скрыто | `OEChatsStickerTimeSide`, `NekoConfig.hideTimeForSticker` | `history/view/media/history_view_media_unwrapped.cpp`, `history_view_element.{h,cpp}`, `history_view_message.{h,cpp}`, `settings/sections/settings_exteraless.cpp` | S · низкий | Xvfb: стикер с временем сбоку и без, обычные сообщения без изменений | ✅ 2026-11-05: `OEChatsStickerTimeSide`/`OEChatsHideStickerTime`, селектор `Sticker time`, CLI `stickertime` (14 проверок), Xvfb: время сбоку у исходящего стикера и полностью скрыто в режиме Hidden; текст не затронут | R1 P0-12 |
| NB-15 | Скрыть архив и сортировка по непрочитанным | `NaConfig.hideArchive`, `NekoConfig.sortByUnread` | `dialogs/dialogs_widget.cpp`, `dialogs/dialogs_indexed_list.cpp` | M · средний–высокий (индексация списка) | Xvfb: архив скрыт; сортировка на тестовом профиле | ⬜ (расхождение: R1 P0-13, R2 P2 — приоритет взят по R1, риск по R2) | R1 P0-13, R2 §3.1 |
| NB-16 | Drag-порядок пилюль | `PillStackConfig.savePillsLayout` | `Telegram/SourceFiles/exteraless/pill_stack.{h,cpp}`, `exteraless/config.{h,cpp}`, `exteraless/backup.cpp` | M · низкий | Xvfb: drag меняет порядок и переживает рестарт | ✅ 2026-11-05: drag на полосе пилюль, порядок в `OEPillStackOrder` (CSV), CLI `pillorder` (9 проверок), `.extera`; Xvfb: перетаскивание меняет порядок, после рестарта сохраняется | R1 P0-14 |
| NB-17 | Ghost: пакетный переключатель и статус в меню | `setGhostMode`/`toggleGhostMode`, `showGhostModeStatus` | `window/window_main_menu.cpp`, `exteraless/ghost_mode.cpp`, `exteraless/ghost_status.{h,cpp}`, `settings/sections/settings_exteraless.cpp` | S · низкий | один клик переключает 4 тумблера, статус виден в меню | ✅ 2026-11-05: пункт меню «Ghost Mode (On/Off/Partial)», `OEGhostShowStatus`, строгий `IsGhostActive`, CLI `ghost` (13 проверок), Xvfb: один клик включает/выключает 4 тумблера, статус меняется | R1 P0-15 |
| NB-18 | Ghost: «locked» тумблеры | `sendReadMessagePacketsLocked`, `sendReadStoriesPacketsLocked`, `sendOnlinePacketsLocked`, `sendUploadProgressLocked` | `exteraless/config.{h,cpp}`, `exteraless/ghost_status.cpp`, `exteraless/backup.cpp`, `settings/sections/settings_exteraless.cpp` | S · низкий | locked-тумблер не меняется пакетным переключателем | ✅ 2026-11-05: четыре `OEGhostSend*Locked`, подсекция «Locked Ghost settings», `SetGhostActive` пропускает locked, CLI `ghost-lock` (19 ghost-проверок), Xvfb: read остаётся включённым при пакетном Ghost On; offline-lock удалён вместе с мёртвым ключом в NB-38 | R1 P0-16 |

## 4. P1 — доверие к сборке, документация, средние подсистемы

| ID | Задача | Android key(s) | Desktop point | Размер · риск | Smoke | Статус | Источник |
|---|---|---|---|---|---|---|---|
| NB-19 | Расширить схему удалённых: реакции, ответ, форвард, entities, правки, read date | Room-схема AyuGram | `exteraless/deleted_entry.{h,cpp}`, `exteraless/deleted_messages.{h,cpp}`, `boxes/deleted_messages_box.cpp`, `window/window_peer_menu.cpp` | L · высокий (миграция JSON, размер файла) | сохранить сообщение с реакцией и ответом, перезапуск, всё читается | ✅ 2026-11-05: схема v2 (17 полей), старые записи совместимы, CLI `deleted-entry` (7 проверок), Xvfb: реакция и reply видны в боксе; исправлен захват controller/peer в меню | R1 P1-1 |
| NB-20 | Метка удалённого сообщения в чате и иконка | `translucentDeletedMessages`, `useDeletedIcon`, `customDeletedMark`, `deletedMarkColor` | `exteraless/config.{h,cpp}`, `exteraless/backup.cpp`, `boxes/deleted_messages_box.cpp`, `settings/sections/settings_exteraless.cpp` | M · средний | Xvfb: удалённое помечено, при off метки нет | ⚠️ split-required 2026-11-05: 4 настройки, значок/кастомная метка, прозрачность и цвет работают в боксе «Deleted messages» (CLI `deletedmark`, 10 проверок, Xvfb). Inline-вставка в историю чата требует подсистемы локальных HistoryItem — выделено в NB-92 | R1 P1-2, R2 M3 |
| NB-21 | Импорт базы AyuGram | `OEAyuDatabaseImport` | новый импортёр + `exteraless/deleted_messages.cpp` | M · средний (чужой формат БД) | импорт тестовой БД, счётчик совпал | ⛔ отложено 2026-11-05: формат — SQLite, в проекте нет ни QtSql, ни sqlite3 в runtime; нужна новая зависимость (или внешний `sqlite3` в Nix), это отдельный packaging-этап | R1 P1-3 |
| NB-22 | Regex-фильтры сообщений | `regexFiltersEnabled`, `regexFiltersData`, `enableInChats`, `excludedDialogs` | `dialogs/dialogs_inner_widget.cpp`, `data/data_histories.cpp` | L · высокий (производительность, потеря сообщений из вида) | CLI regex-движка + Xvfb: `(?i)spam` скрывает строку, выключение возвращает; нужен флаг «показать скрытые» | ⬜ | R1 P1-4 |
| NB-23 | Ghost-отправка «как отложенное» | `NaConfig.ghostScheduledSend` | `exteraless/ghost_schedule.{h,cpp}`, `history/history_widget.cpp`, `history/view/history_view_chat_section.cpp`, `settings/sections/settings_exteraless.cpp` | M · высокий (задержка 12 с) | при включённом online-ghost сообщение уходит через 12 с | ✅ 2026-11-05: `OEGhostScheduledSend` ставит `SendOptions.scheduled = now + 12` для текстовых отправок при активном online-ghost; исключены self, боты и секретные чаты; CLI `ghost-schedule`/`ghost-schedule-decide` (12 проверок), Xvfb toggle. Реальная отправка в тестовом профиле не выполнялась, чтобы не писать в чужие чаты | R1 P1-5 |
| NB-24 | Inline-математика без валют | `OEChatsInlineMathResult` | `exteraless/math.{h,cpp}`, `history/history_widget.cpp` (`eventFilter`), `history/view/controls/history_view_compose_controls.cpp` | M · средний (парсер выражений) | `2+2=` показывает 4, по Space вставляет | ✅ 2026-11-05: парсер с `+ - * / % ^`, скобками, `pi`/`e` и функциями; настройка `OEChatsInlineMathResult`, CLI `math-eval` (31 проверка), Xvfb: Space после `2+2=` даёт черновик `2+2=4` | R1 P1-6 |
| NB-25 | Pangu: пробелы между CJK и латиницей | `NaConfig.enablePanguOnSending` | `exteraless/pangu.{h,cpp}`, `history/history_widget.cpp`, `history/view/history_view_chat_section.cpp`, `settings/sections/settings_exteraless.cpp` | S · низкий | текст «中文test» уходит с пробелом | ✅ 2026-11-05: `OEChatsPanguOnSending` (default false), `ApplyPanguWithMap` с корректировкой text-tags, тумблер в Chats, CLI `pangu*` (15 проверок), Xvfb toggle | R1 P1-7 |
| NB-26 | Свой путь сохранения и подпапка на чат | `NekoConfig.customSavePath`, `NaConfig.saveToChatSubfolder` | `exteraless/save_path.{h,cpp}`, `data/data_file_click_handler.cpp`, `settings/sections/settings_exteraless.cpp` | M · средний (пути ФС) | файл сохраняется в выбранную папку и подпапку | ✅ 2026-11-05: `OEGeneralSaveToChatSubfolder` добавляет подпапку с именем чата в автоматические сохранения (`DocumentSaveClickHandler`), безопасные имена и коллизии; CLI `save-subfolder`/`chat-folder`/`chat-subfolder` (15 проверок), Xvfb toggle. `customSavePath` покрыт существующей настройкой desktop «Download path» | R1 P1-8 |
| NB-27 | Скрыть кнопки AI-редактора и саммари | `OEAppearanceHideAiEditor`, `HideIvSummary`, `HideMessageSummary` | `exteraless/config.{h,cpp}`, `history/view/controls/history_view_compose_controls.cpp`, `boxes/edit_caption_box.cpp`, `boxes/send_files_box.cpp` | S · низкий | Xvfb: кнопка скрыта, остальное работает | ✅ 2026-11-05: `OEAppearanceHideAiEditor` скрывает кнопку AI-редактора в compose и caption-боксах; `HideIvSummary`/`HideMessageSummary` не переносились — в этой версии tdesktop нет соответствующих AI-саммари (зафиксировано в аудите), CLI `ai-editor` (5 проверок), Xvfb toggle | R1 P1-9 |
| NB-28 | UI-просмотр audit-лога плагинов | `PluginActivityLog`, `PluginActivityExport` | `Telegram/SourceFiles/exteraless/plugins_box.cpp` | M · средний | строки `tdata/plugins-audit.log` видны в боксе, экспорт работает | ✅ 2026-11-05: кнопка «Audit log» в боксе плагинов показывает последние 24 строки, «Export» открывает файловый диалог с именем `plugins-audit.log`; Xvfb подтвердил строки и диалог | R1 P1-10 |
| NB-29 | Установка и удаление плагина из файла | `PluginsInstallFromFile`, `PluginsDelete` | `exteraless/plugins/installer.{h,cpp}`, `Telegram/SourceFiles/exteraless/plugins_box.cpp` | M · средний (копирование в `tdata/plugins`) | установить `.plugin`, он в списке; удалить | ✅ 2026-11-05: «Install plugin...» копирует `.plugin`/`.py` с санитайзом имени, дедупликацией и лимитом 2 МиБ, «Delete plugin» удаляет с подтверждением; CLI `plugin-install`/`plugin-delete`/`plugin-filename` (19 проверок), Xvfb: установка через файловый диалог и удаление работают | R1 P1-11 |
| NB-30 | Меню плагина в главном меню | `plugins/menus`, `PluginMenuItem` | `window/window_main_menu.cpp`, IPC в `plugin_runtime.cpp` | L · высокий (IPC и жизненный цикл) | плагин добавляет пункт, клик вызывает host | ⬜ | R1 P1-12 |
| NB-31 | Pill Stack: бесконечная прокрутка и сброс раскладки | `PillStackInfiniteScrolling`, `PillStackReset` | `settings/sections/settings_exteraless.cpp`, `exteraless/config.cpp` (reset) | S · низкий | колесо/скролл листает пилюли по кругу, сброс возвращает дефолт | ✅ 2026-11-05: кнопка «Reset pill order» сбрасывает `OEPillStackOrder` в дефолт (CLI `pillorder-reset`, 3 проверки, Xvfb). Бесконечная прокрутка не применима: пилюли переносятся на новую строку, а не листаются | R1 P1-13 |
| NB-32 | Внешние субтитры к видео | `OESubtitles`, `OEChooseSubtitles`, `OEDisableSubtitles` | `media/view/media_view_overlay_widget.cpp` | M · средний (парсинг SRT/ASS) | загрузить `.srt`: субтитры видны и отключаются | ⬜ | R1 P1-14 |
| NB-33 | Ответ на удалённое сообщение цитатой | `NaConfig.replyToDeletedAsQuote` | `history/view/history_view_context_menu.cpp`, `window/window_peer_menu.cpp` | S · средний (текст уже хранится) | reply на удалённое подставляет цитату | ⛔ заблокировано NB-92: в desktop удалённые сообщения не вставляются в историю, поэтому reply на них инициировать не из чего | R1 P1-15 |
| NB-34 | Тихая отправка по умолчанию | `NaConfig.silentMessageByDefault` | `exteraless/config.{h,cpp}`, `history/history_widget.cpp`, `history/view/history_view_chat_section.cpp`, `settings/sections/settings_exteraless.cpp` | S · низкий | отправка без звука по умолчанию | ✅ 2026-11-05: `OEChatsSilentByDefault` (default false) выставляет `SendOptions.silent` в `prepareSendAction` обоих виджетов, тумблер в Chats, CLI `silent` (5 проверок), Xvfb toggle | R1 P1-16 |
| NB-35 | Скрыть «Send as…» и кнопки ввода канала | `OEChatsHideSendAsPeer`, `hideShareButtonInChannel`, `hideCameraTile` | `exteraless/config.{h,cpp}`, `history/view/controls/history_view_compose_controls.cpp`, `settings/sections/settings_exteraless.cpp` | M · средний | кнопки скрыты в канале, отправка работает | ✅ 2026-11-05 (частично): `OEChatsHideSendAsPeer` убирает кнопку «Send as...» в каналах (CLI `send-as`, 5 проверок, Xvfb toggle). `hideCameraTile` не применим — в attach-меню tdesktop нет камеры; `hideShareButtonInChannel` — кнопка профиля, вынесена в NB-73 | R1 P1-17 |
| NB-36 | Скрыть секции настроек Premium и Help | `NaConfig.hidePremiumSection`, `hideHelpSection` | `exteraless/config.{h,cpp}`, `settings/sections/settings_main.cpp`, `settings/sections/settings_exteraless.cpp` | S · низкий | секции исчезают, поиск не падает | ✅ 2026-11-05: `OESettingsHidePremium`/`OESettingsHideHelp`, ранний выход из `BuildPremiumSection`/`BuildHelpSection`, два тумблера в General, CLI `settings-sections` (5 проверок), Xvfb: секции исчезли | R1 P1-18 |
| NB-37 | Скрыть телефон в профиле | `NekoConfig.hidePhone` | `info/profile/info_profile_values.cpp` (`PhoneValue`, `PhoneOrHiddenValue`), `settings/sections/settings_exteraless.cpp` | S · низкий | Xvfb: профиль контакта и свой; при off поведение как upstream | ✅ 2026-11-05: `OEGeneralHidePhone`, скрытие в `PhoneValue`/`PhoneOrHiddenValue`, тумблер в General, CLI `general` (22 проверки), Xvfb подтвердил скрытие номера в профиле | R2 §6.3 |
| NB-38 | Ghost offline-после-online: реализовать или убрать мёртвый ключ | `sendOfflinePacketAfterOnline` | `exteraless/config.cpp`, `exteraless/backup.cpp`, `settings/sections/settings_exteraless.cpp` | S · низкий | Xvfb/CLI: ключ либо работает, либо отсутствует в конфиге и бэкапе | ✅ 2026-11-05: удалены `OEGhostSendOfflineAfterOnline` и `OEGhostSendOfflineAfterOnlineLocked` из config/backup/UI; `.extera` теперь 56 ключей | R2 M5, M15 |
| NB-39 | Привести `docs/feature-mapping.md` в соответствие с кодом: метка удалённого, hook, pinned bar | — | `docs/feature-mapping.md` | S · низкий | сверка утверждений с `exteraless/deleted_messages.cpp`, `window/window_peer_menu.cpp`, поиском по `pinnedReactions` | ✅ 2026-11-05: Deleted messages описывает схему v2 и метку в боксе (inline — NB-92); удалено неверное утверждение о `pinnedReactions` (символа в дереве нет); hook/plugin-раздел сверен с `plugin_runtime.cpp` | R2 M3, M4, M6 |
| NB-40 | Пересобрать `result/` или убрать устаревший symlink | — | `result/`, `flake.nix` | S · средний (пользователи Nix видят старый клиент) | `strings result/bin/.Telegram-wrapped` содержит новые маркеры (`Plugin sandbox is unavailable`, `Compact unread messages label`) | ✅ 2026-11-05: устаревший untracked-symlink `result/` на сборку от 1 ноября удалён; свежий клиент создаётся `nix build` по требованию, репозиторий больше не выдаёт старый бинарник за актуальный | R2 M7 |
| NB-41 | Починить путь в `nix/smoke-test.sh` | — | `nix/smoke-test.sh` | S · низкий | скрипт запускает `out/bin/Telegram`, а не каталог `out/Telegram` | ✅ 2026-11-05: путь исправлен, `LD_LIBRARY_PATH` собирается из `NIX_LDFLAGS`; `bash nix/smoke-test.sh` PASS (30 с) | R2 M8 |
| NB-42 | Зафиксировать базу патча: перестать считать локальный тег upstream-снимком | — | `nix/update-patch.sh`, `nix/upstream-base`, `nix/patches/exteraless.patch` | S–M · высокий (молчаливая потеря правок) | `git apply --check` патча на чистом 7.3.1 + sha256-сверка | ✅ 2026-11-05: база закреплена хешем коммита и tree-хешем в `nix/upstream-base` (локальный снимок `aa09910` tree-идентичен upstream `6fed91f`), при расхождении тега скрипт предупреждает, `--check` применяет патч к чистому worktree и печатает sha256 | R2 M10 |
| NB-43 | Ghost-allowance: привязать разрешение к peer | `AyuState.setAllowReadPacket` (семантика) | `exteraless/ghost_mode.cpp` (`Allowance`, TTL 5 с) | M · средний (риск утечки read-квитанции чужому чату) | CLI: разрешение для чата A не расходуется чатом B | ⛔ отложено 2026-11-05: требует протяжки peer id через `MTP::RequestFilterInfo` и все вызовы `send`, слишком инвазивно для текущего этапа | R2 M11 |
| NB-44 | Мёртвый `Config::load()`: убрать или задокументировать | — | `exteraless/config.cpp` | S · низкий | сборка без предупреждений, поведение ленивой загрузки не меняется | ✅ 2026-11-05: неиспользуемый `Config::load()` удалён, ленивая `ensureLoaded()` не изменена; CLI-тесты PASS | R2 M12 |
| NB-45 | `hasVpnInterface()` не должен считать `ppp*` VPN | `OEProxyDisableConditions` (бит 1) | `exteraless/network.cpp` | S · средний (proxy выключается на PPPoE) | CLI/ручной тест: `ppp0` не даёт VPN-условия | ⬜ | R2 M13 |
| NB-46 | `ApplyProxyRules()` не должен отменять ручное включение прокси и не должен персистить автовыключение | `OEProxyAutoDisabled` | `exteraless/proxy_rules_state.{h,cpp}`, `exteraless/proxy_rules.cpp`, `core/application.cpp` | S · средний | включить прокси при активном условии → остаётся включённым; перезапуск без условия не включает прокси сам | ✅ 2026-11-05: состояние только в памяти, автовыключение один раз на переход условия, ручное включение побеждает; `OEProxyAutoDisabled` больше не пишется; CLI `proxy-rule` (6 проверок) | R2 M14 |

## 5. P2 — паритет функций

| ID | Задача | Android key(s) | Desktop point | Размер · риск | Smoke | Статус | Источник |
|---|---|---|---|---|---|---|---|
| NB-47 | Feed: лента каналов как режим списка | `OEAppearanceShowFeedTab`, `FeedConfig` | новый режим в `dialogs/` + синтетическая история в `data/` | XL · очень высокий | лента открывается, листается, счётчик непрочитанных корректен | ⬜ | R1 P2-1 |
| NB-48 | Свой AI-ассистент (endpoint, model, key, роли, история) | `AiConfig` (`KEY_SERVICES`, `KEY_ROLES`, `KEY_HISTORY`) | новый `ai/` + `boxes/`, `chat_helpers/compose` | L · высокий (сеть, ключи, приватность) | ввод ключа, запрос, стриминг, история, очистка | ⬜ | R1 P2-2 |
| NB-49 | Провайдеры перевода (Google, DeepL, LLM) | `translationProvider`, `googleCloudTranslateKey`, `deepLTranslateKey`, `deepLFormality` | `lang/translate_provider.cpp`, `boxes/translate_box.cpp` | L · высокий (ключи, ToS, сеть) | перевод выбранным провайдером, ошибки показываются тостом | ⬜ | R1 P2-3 |
| NB-50 | MD3-плеер и тексты песен | 30+ `OEPlayer*`, `OEPlayerLrclibAllowed` | `media/audio/*`, `media/view/*` | L · высокий (воспроизведение, UI) | музыка играет, мини-плеер, очередь, LRC | ⬜ | R1 P2-4 |
| NB-51 | Vosk: офлайн-распознавание речи | `ExteraConfig.getRecognitionLanguage`, `speech/*` | новый модуль + нативные модели | L · высокий (сборка и размер моделей) | распознать голосовое офлайн | ⬜ | R1 P2-5 |
| NB-52 | Иконпаки `.icons` | `IconPacksConfig`, `AppearanceConfig.iconPacks` | подсистема иконок `Telegram/lib_ui` | L · высокий (правит `lib_ui`, вне Nix-патча) | пак применяется и откатывается | ⬜ | R1 P2-6 |
| NB-53 | Облако настроек | `OECloudUpload`, `OECloudAutoSync`, `OECloudRestore`, `OECloudClear` | `storage/storage_account.cpp` + служебные сообщения | M · высокий (приватность, аккаунт) | выгрузка и восстановление на двух профилях | ⬜ | R1 P2-7 |
| NB-54 | GitHub-апдейтер | `NaConfig.autoUpdateChannel`, `updater/*` | `core/update_checker.cpp` | M · средний (безопасность обновлений) | проверка версии, канал off | ⬜ (спорно: R2 относит updater к ⛔; если делать — только поверх upstream-механизма) | R1 P2-8, R2 §5 |
| NB-55 | Bot privacy: маршрутизация ботов в выбранный аккаунт | `OEGeneralBotPrivacyUserId`, `botPrivacyEnabled`, `alwaysAsk` | `main/main_domain.cpp`, `Window::ShowAccountSelector` | M · высокий (несколько аккаунтов, история) | бот открывается в выбранном аккаунте, «всегда спрашивать» показывает пикер | ⬜ | R1 P2-9 |
| NB-56 | Пилюли погоды, курсов и золота | `WEATHER`, `BTC`, `USD`, `EUR`, `ETH`, `GOLD` | `Telegram/SourceFiles/exteraless/pill_stack.cpp` + сеть и кэш | M · средний (внешние API, оффлайн) | пилюля обновляется, ошибка сети не ломает UI; CLI парсера | ⬜ | R1 P2-10, R2 §3.4 |
| NB-57 | Полный JSON-бэкап всех настроек | `SettingsBackupHelper` | `exteraless/backup.cpp` | M · средний (версия схемы) | экспорт и импорт всех ключей, отчёт о неизвестных | ⬜ | R1 P2-11 |
| NB-58 | Персидский календарь | `NekoConfig.usePersianCalendar`, `displayPersianCalendarByLatin` | `lang/` форматы дат | M · средний | даты в чате и профиле по Solar Hijri | ⬜ | R1 P2-12 |
| NB-59 | Закладки сообщений | `helper/BookmarksHelper`, `BookmarkManagerActivity` | новый экран + `Storage::Cache` | M · средний | добавить и открыть закладку, переживает рестарт | ⬜ | R1 P2-13 |
| NB-60 | Правила автоскачивания: архивы и `.exe`, zip-политики | `disableAutoDownloadingArchive`, `Win32ExecutableFiles` | `settings/sections/settings_local_storage.cpp`, `storage/` | M · средний | архив не качается автоматически, правило снимается | ⬜ | R1 P2-14 (пересечение с NB-26 снято: NB-26 про путь, NB-60 про правила) |
| NB-61 | Скругления аватарок и единый радиус | `avatarCorners`, `singleCornerRadius`, `avatarSquareness` | `dialogs/dialogs_row.cpp`, `ui/userpic_view.cpp`; маска — вероятно `lib_ui/ui/image/*` | M · средний (проверить `lib_ui` до планирования) | Xvfb: аватарки меняют форму, онлайн-точка на месте | ⬜ | R2 §3.3 |
| NB-62 | Мини-аватарки отправителей в списке | `OEAppearanceSenderMiniAvatars` | `dialogs/dialogs_row.cpp` | M · средний | Xvfb: превью диалога с мини-аватаркой | ⬜ | R2 §3.3 |
| NB-63 | Стили шапки чата: iOS и M3 | `iosChatHeader`, `iosNavigationBarStyle`, `newChatHeaderStyle` | `history/view/history_view_top_bar_widget.cpp`, `ui/chat/chat.style` | M · средний (геометрия шапки) | Xvfb-скрин шапки в каждом режиме | ⬜ | R2 §3.3 |
| NB-64 | Радиус секций, отдельные заголовки, M3 list items | `sectionRadius`, `separateHeaders`, `m3ListItems` | `settings/sections/*`, `ui/boxes/*` | M–L · средний | Xvfb: настройки в трёх режимах, поиск не ломается | ⬜ | R2 §3.3 |
| NB-65 | Стеклянное меню и контур | `glassMessageMenu`, `glassOutlineStyle` | `menu/*`, `window/themes/*` | M · средний (блюр зависит от ОС) | Xvfb: меню сообщения в двух режимах | ⬜ | R2 §3.3 |
| NB-66 | Стили прогресса, свитчей и слайдеров | `newLoadingStyle`, `newSwitchStyle`, `newSliderStyle` | `ui/widgets/*`, `ui/effects/*` (частично `lib_ui`) | M · средний (вне Nix-патча) | Xvfb-скрин контролов | ⬜ | R2 §3.3 |
| NB-67 | Дата на форвардах | `NaConfig.dateOfForwardedMsg` | `exteraless/config.{h,cpp}`, `history/history_item_components.cpp`, `settings/sections/settings_exteraless.cpp` | S–M · низкий | Xvfb: плашка «forwarded from» с датой | ✅ 2026-11-05: `OEChatsShowForwardDate` дописывает `· дата время` к заголовку форварда; CLI `forward-date` (5 проверок); лог подтвердил строку «Forwarded from … · Oct 30 8:16:10 PM», в узких бабблах хвост эллипсится как в upstream | R2 §3.2 |
| NB-68 | Иконка вместо текста «edited» и своя метка | `replaceEditedWithIcon`, `customEditedMessage`, `useEditedIcon` | `exteraless/config.{h,cpp}`, `history/view/history_view_bottom_info.cpp`, `settings/sections/settings_exteraless.cpp` | S · низкий | Xvfb-скрин: иконка и своя строка | ✅ 2026-11-05: `OEChatsUseEditedIcon` (✎) и `OEChatsCustomEditedMark` (16 символов) заменяют «edited»; CLI `edited-mark` (6 проверок), Xvfb: метка «ed.» отрисовалась в сообщении; `replaceEditedWithIcon` сведён к `OEChatsUseEditedIcon` | R2 §3.2 |
| NB-69 | Размер и форма стикеров | `NekoConfig.stickerSize`, `OEChatsStickerShape` | `history/view/media/history_view_sticker.cpp`; уже есть `base::options::option<int> OptionStickerSize` | S–M · низкий | Xvfb: ползунок 8..20 и три формы | ⬜ | R2 §3.2 |
| NB-70 | Оформление цитаты ответа | `replyColors`, `replyEmoji`, `replyBackground`, `messageColoredBackground` | `history/view/history_view_reply.cpp` | M · средний | Xvfb: цитата в трёх режимах, ширина не ломается | ⬜ | R2 §3.2 |
| NB-71 | Результаты опроса до голосования | `OEChatsShowResultsBeforeVoting` | `data/data_poll.cpp`, `history/view/media/history_view_poll.cpp` | M · средний | Xvfb: проценты видны до голоса, после голоса поведение как раньше | ⬜ | R2 §3.2 |
| NB-72 | Длительность перемотки двойным тапом | `OEChatsDoubleTapSeekDuration` | `exteraless/config.{h,cpp}`, `media/view/media_view_overlay_widget.cpp`, `settings/sections/settings_exteraless.cpp` | S · низкий | Xvfb: 5/10/15/30 с | ✅ 2026-11-05: `OEChatsSeekDuration` (5/10/15/30, default 5) задаёт шаг перемотки стрелками, J/L — двойной шаг; Android-индекс `doubleTapSeekDuration` конвертируется; CLI `seek-duration` (8 проверок), Xvfb: селектор в Chats | R2 §3.2 |
| NB-73 | Скрыть кнопки профиля: gift, money, call, share, search | `hideGiftButton`, `hideMoneyButton`, `hideCallButton`, `hideShareButtonInChannel`, `hideChannelSearchButton` | `info/profile/info_profile_top_bar.cpp`, `history/view/history_view_top_bar_widget.cpp` | S–M · низкий | Xvfb: кнопки скрыты в канале и в профиле, действия доступны из меню | ⬜ | R2 §3.2 (дополняет NB-35: там compose, здесь профиль) |
| NB-74 | Отключить превью ссылок по умолчанию | `NekoConfig.disableLinkPreviewByDefault` | `exteraless/config.{h,cpp}`, `history/view/controls/history_view_webpage_processor.{h,cpp}`, `settings/sections/settings_exteraless.cpp` | M · средний | Xvfb: toggle превью выключен при вставке ссылки | ✅ 2026-11-05: `OEChatsDisableLinkPreviewByDefault` (default `false`) отключает автоматическую генерацию превью ссылок; ручное переключение доступно через `Ctrl+R`; CLI `disable-link-preview` (5 проверок), Xvfb: ссылка без карточки превью при включённой настройке | R2 §3.1 |
| NB-75 | Всегда показывать иконку скачивания | `NaConfig.alwaysShowDownloadIcon` | `exteraless/config.{h,cpp}`, `dialogs/ui/dialogs_suggestions.cpp`, `history/view/media/history_view_{document,gif}.cpp`, `settings/sections/settings_exteraless.cpp` | M · средний | Xvfb: иконка есть у незагруженного и загруженного медиа | ✅ 2026-11-05: `OEChatsAlwaysShowDownloadIcon` (default `false`) держит вкладку «Downloads» в поисковых подсказках и угловую иконку на уже загруженных аудиофайлах; CLI `always-show-download` (5 проверок); Xvfb: чип «Downloads» появляется сразу после включения, у играющего аудио иконка есть с настройкой и исчезает без неё, у незагруженного — на месте. Для GIF-уголка click-state теперь учитывает `loadedInMediaCache` так же, как paint | R2 §3.1 |
| NB-76 | Boost скачивания и загрузки | `OEGeneralDownloadSpeedBoost`, `NekoConfig.uploadBoost` | `mtproto/session_private.cpp`, `data/data_download_manager.cpp` | M–L · средний (сетевой smoke) | замер скорости на тестовом DC, регресс по таймаутам | ⬜ | R2 §3.1, R1 §3.5 |
| NB-77 | IPv6 и DNS over HTTPS | `NekoConfig.useIPv6`, `dnsType`, `customDoH` | `mtproto/connection_*`, `connection_resolving.cpp` | L · высокий (свой резолвер) | сетевой smoke: подключение по IPv6 и через DoH | ⬜ | R2 §3.1 |
| NB-78 | Обновление списка tracking-параметров с сервера | `links/LinkCleaner.kt`, `AdGuardRule.kt` | `exteraless/links.cpp` (сейчас хардкод) + HTTP-загрузчик | M · средний | CLI: загрузка списка, сбой сети не ломает срез | ⬜ | R2 §3.2 |
| NB-79 | Plugin engine stage 3: меню, настройки, intents, уровни доступа | `plugins/{ui,menus,intents,models}`, `PluginTrustLevel` | `Telegram/SourceFiles/exteraless/plugin_runtime.cpp` (IPC-контракт) | XL · высокий | CLI host-тесты: плагин публикует пункт меню, читает свои настройки, получает intent | ⬜ | R2 §3.6 |
| NB-80 | Пункты меню медиавьюера | 6 ключей `NaConfig.mediaViewerMenuItem*` | `exteraless/config.{h,cpp}`, `media/view/media_view_overlay_widget.cpp`, `settings/sections/settings_exteraless.cpp` | S–M · низкий | Xvfb: каждый пункт скрывается отдельно | ✅ 2026-11-05: Forward, Copy Frame, Copy, Set as profile photo скрываются четырьмя ключами; `NoQuoteForward` и `ScanQRCode` не применимы (таких пунктов в desktop нет); CLI `media-menu` (5 проверок), Xvfb: меню фото осталось без Copy/Forward/Set profile | R1 §3.4 |
| NB-81 | Пункты меню чата (14 ключей) | `chatMenuItemBoostGroup`, `LinkedChat`, `ToBeginning`, `GoToMessage`, `HideTitle`, `ViewDeleted`, `ClearDeleted`, `DeleteOwnMessages`, `showAddToBookmark` | `exteraless/config.{h,cpp}`, `window/window_peer_menu.cpp`, `settings/sections/settings_exteraless.cpp` | S–M · низкий | Xvfb: пункт скрывается, остальные на месте | ✅ 2026-11-05 (частично): Boost, linked chat, «Deleted messages», «Clear deleted messages» (с подтверждением) управляются четырьмя ключами; CLI `chat-menu` (5 проверок); Xvfb: Boost и Deleted messages исчезли, Clear остался. `ToBeginning`/`GoToMessage`/`HideTitle`/`DeleteOwnMessages`/закладки не применимы | R1 §3.4 |
| NB-82 | Быстрые админ-шорткаты в профиле | `shortcutsAdministrators`, `shortcutsRecentActions`, `shortcutsStatistics`, `shortcutsPermissions`, `shortcutsMembers` | `info/profile/*` | S–M · средний | Xvfb: пункты ведут на нужные экраны | ⬜ | R1 §3.4 |
| NB-83 | Группированное меню сообщения и меню удаления по умолчанию | `groupedMessageMenu`, `defaultDeleteMenu` | `history/view/history_view_context_menu.cpp` | S–M · средний | Xvfb: основные действия внизу, удаление по умолчанию как выбрано | ⬜ | R1 §3.4 |
| NB-84 | Скрыть вкладку «Все чаты» и стиль заголовков вкладок | `NekoConfig.hideAllTab`, `tabsTitleType`, `tabCounter` | `exteraless/config.{h,cpp}`, `data/data_chat_filters.{h,cpp}`, `window/window_filters_menu.cpp`, `window/window_session_controller.cpp`, `settings/sections/settings_exteraless.cpp` | S · низкий (в tdesktop вкладки — фильтры) | Xvfb: вкладка скрыта, фильтры работают | ✅ 2026-11-05: `OEChatsHideAllChatsFolder` (default `false`) скрывает папку «Все чаты» в сайдбаре фильтров при наличии других папок; CLI `hide-all-chats` (5 проверок), Xvfb: «All chats» скрыта, активной стала папка «IT»; стили вкладок (`tabsTitleType`) уже есть в upstream (`chatFiltersTabsMode`) | R2 §3.3 |
| NB-85 | Скрытая «ложная» секция настроек за passcode | `OEPrivacyFalseBottom*` | `exteraless/config.{h,cpp}`, `settings/sections/settings_exteraless.cpp`, `exteraless/backup.cpp` | M · средний (приватность, локальный passcode) | Xvfb: секция скрыта/показана после разблокировки, поиск не раскрывает | ✅ 2026-11-05: `OEPrivacyFalseBottom` (default `false`) добавляет в конец Exteraless секцию «Двойное дно»; при локальном пароле открывается только после его ввода (неверный — ошибка, верный — перестройка страницы), содержимое (пасскод-менеджмент и «Заблокировать сейчас») не индексируется поиском, пока секция закрыта; CLI `false-bottom` (5 проверок); Xvfb подтвердил оба состояния и `Lock now`. Per-account passcode/panic code Android-версии не переносятся: в desktop один локальный пароль на приложение; ID/DC в профиле desktop не показывает | R2 §3.1 |
| NB-86 | Мелочи чата: приветственный стикер, клавиатура при скролле, скрепка, нижняя кнопка канала, быстрые переходы | `disableGreetingSticker`, `hideKeyboardOnScroll`, `keepAttachButton`, `bottomButton`, `quickTransition*` | `chat_helpers/*`, `history/view/controls/history_view_compose_controls.h` | M · средний | Xvfb: каждый пункт меняет только своё поведение | ⬜ | R2 §3.2 (P2/P3) |
| NB-87 | Праздничные декорации шапки | `chatDecoration`, `NekoConfig.actionBarDecoration` | `window/window_top_bar_wrap.h`, `dialogs/dialogs_widget.cpp` | S · низкий | Xvfb: декор включается и выключается, геометрия шапки не меняется | ⬜ | R2 §3.3 (P3 у R2) |
| NB-88 | Медиа-качество и форматы: HLS по умолчанию, декодер, GIF как видео, битрейт аудио, MP4 как видео | `defaultHlsVideoQuality`, `playerDecoder`, `takeGIFasVideo`, `customAudioBitrate`, `sendMp4DocumentAsVideo` | `media/streaming/*`, `media/view/*`, `chat_helpers/compose/*` | M–L · средний (сеть и кодеки) | Xvfb + ручной: видео играет в выбранном качестве, GIF уходит видео | ⬜ | R1 §3.18, R2 §3.2 |
| NB-89 | Тулбар форматирования, markdown-парсер, спойлеры сразу, подсветка синтаксиса | `showText*` (10+), `textStyleOrder`, `markdownParser`, `showSpoilersDirectly`, `syntaxhighlight/*` | `chat_helpers/message_field.cpp`, `ui/text/*` | M · средний | Xvfb: состав и порядок кнопок сохраняются, спойлер раскрыт | ⬜ | R1 §3.19 |
| NB-90 | WEB proxy: relay-сервер, включение в UI и документации | desktop-only; `tproxy-server` из `docs/web-proxy-plan.md` | `mtproto/web_proxy/*`, `mtproto/details/mtproto_web_proxy_socket.{h,cpp}`, `boxes/connection_box.cpp`, `docs/feature-mapping.md` | L · высокий (инфраструктура, сеть, приватность) | поднять тестовый relay, проверить соединение и fallback; обновить docs | ⬜ | R2 §6.21 |
| NB-92 | Inline-показ удалённых сообщений в чате | `translucentDeletedMessages` (продолжение NB-20) | `history/history.cpp` (`addNewLocalMessage`), `history/view/history_view_message.cpp`, `history_view_element.cpp`, `exteraless/deleted_messages.cpp` | L · высокий (вставка локальных сообщений, порядок, дубликаты) | Xvfb: удалённое сообщение видно в чате полупрозрачным с меткой; выключение убирает вставки | ⬜ | NB-20 (split) |
| NB-91 | Расширенные действия двойного тапа: translate, save, edit, delete, forward, download, repeat | `doubleTapAction` (Android 3,5,6,7,8,10,11,12) | `exteraless/doubletap.{h,cpp}`, `history/history_inner_widget.cpp`, `history/view/history_view_list_widget.cpp` | M · средний (разные API в двух виджетах) | CLI `doubletap-resolve` для всех значений; Xvfb: каждое действие срабатывает | ⬜ | NB-12 (follow-up) |

## 6. Android-only, plugin/Xposed и «не переносить»

Эти строки не попадают в P0–P2: у них нет desktop-точки интеграции. Список
нужен, чтобы задачи не возвращались в бэклог повторно.

| Блок | Категория | Почему | Вердикт |
|---|---|---|---|
| `plugins/xposed/*` (`HookGate`, `PyMethodHook`, `XposedHooks`) | plugin/Xposed | нет ART, нет перехвата методов | не переносить |
| DEX внутри плагина, `.elyx`, Chaquopy Python 3.12 | plugin | нет Dalvik и встроенного Python | не переносить; desktop-плагины — через IPC-host |
| Шим `com.exteragram.messenger.ExteraConfig` (~150–170 геттеров) | plugin | API плагинов Android-клиента | не переносить; нужен desktop API |
| `appicons/*`, `NaConfig.iconReplacements` | Android-only | нет лаунчера | не переносить |
| Glyph Nothing (`glyph/*`, `OEGlyphEnable`) | Android-only | железо | не переносить |
| CameraX, камера кружков, зум, стабилизация, HDR, extended FPS | Android-only | драйверы и CameraX | не переносить; возможна отдельная задача «параметры записи кружков» |
| `disableVibration`, `disableProximityEvents`, `inAppVibration` | Android-only | железо и ОС | не переносить |
| Quick tiles (`QuickToggleActivity`), app widgets | Android-only | платформа | не переносить |
| Push-транспорты (`pushServiceType*`), notification bubbles/icon | Android-only | инфраструктура Android | не переносить |
| Crashlytics, battery optimization, FLAG_SECURE, jank/energy профайлеры | Android-only | сервисы ОС | не переносить; upstream `logs.cpp` достаточно |
| Google/OSM Maps SDK и геолокация | Android-only | SDK и разрешения | не переносить (формат дат Solar Hijri — отдельная задача NB-58) |
| `TabletMode`, нижняя навигация, `mainTabsHide*`, `hideBottomNavigationBar` | Android-only | другая модель окон | не переносить |
| `unlimitedPinnedDialogs` | нереализуемо | лимит пинов приходит в `appConfig`, клиентского лимита в 7.3.1 нет | не переносить |
| `dividerStyle`, `hideDividers` | нереализуемо | у строк списка чатов нет разделителей | не переносить |
| `typeface`, `useSystemEmoji`, `iconPacks` | `lib_ui` | правки в сабмодуле не попадают в Nix-патч | не переносить в этом виде; только через `lib_ui` |
| Monet, M3-компоненты Android | Android-only | Material-компоненты | не переносить |
| Свои `api_id`/`api_hash` из UI (`NekoXConfig.custom_api`) | Android-only | в tdesktop ключи сборочные | не переносить |
| `unlimited*` (пины, стикеры, GIF) | Android-only | лимиты серверные | не переносить |
| `ignoreContentRestrictions`, `localPremium` | спорно по смыслу | локальная подмена премиума и Sensitive-фильтра | не переносить без отдельного решения |

## 7. Рекомендуемый следующий batch

Порядок важен: сначала данные и приватность, потом новые функции.

### 7.1 Шаг 0 — закрыть P0 по данным

**NB-02 (deleted messages: гейт, лимит, ротация) — закрыто 2026-11-05.**
`OESaveDeletedMessages` (default `false`) читается перед записью, sidecar
ограничен 1000 записями и 100 на чат, ротация выполняется при добавлении и
при загрузке, тумблер добавлен в Chats. Схема файла не менялась; расширение
полей — отдельная задача NB-19.

Все задачи шага 0 закрыты: NB-02 (✅), NB-04 (✅), NB-05 (✅), NB-06 (✅),
NB-03 (✅).

### 7.2 Шаг 1 — batch из трёх функций

Все три лежат в `Telegram/SourceFiles/**` и `exteraless/**`, не требуют
`lib_ui`, поэтому попадают в `nix/patches/exteraless.patch`.

**NB-11 «Видимость пунктов меню сообщения» (`OEChatsMenu*`) — закрыто 2026-11-05.**
11 чекбоксов в подсекции «Visible message menu items» (`OEChatsMenuReply`,
`Edit`, `Pin`, `CopyText`, `CopyLink`, `CopyImage`, `Forward`, `Select`,
`Translate`, `Report`, `Delete`), поддержаны в обоих контекстных меню
(`history_inner_widget.cpp` и `history_view_context_menu.cpp`), CLI `menu`
(8 проверок), Xvfb подтвердил скрытие `Pin` в чате.

**NB-07 «Время с секундами» (`showSeconds` → `OEGeneralShowSeconds`) — закрыто 2026-11-05.**
Функция `Exteraless::FormatClock`, тумблер «Show seconds in timestamps» в
General, вызовы в `history_view_bottom_info.cpp` и `format_values.cpp`,
ключ `formatTimeWithSeconds` в `.extera`, CLI `general/clock` (17 проверок),
Xvfb подтвердил секунды в сообщениях и списке чатов.

**NB-37 «Скрыть телефон в профиле» (`hidePhone` → `OEGeneralHidePhone`) — закрыто 2026-11-05.**
`PhoneValue` и `PhoneOrHiddenValue` в `info_profile_values.cpp` скрывают строку
номера при включённой настройке, тумблер добавлен в General, ключ
`hidePhoneNumber` в `.extera`, CLI `general` (22 проверки), Xvfb подтвердил
скрытие номера в профиле.

Все задачи шага 1 закрыты: NB-11 (✅), NB-07 (✅), NB-37 (✅).

**NB-12 «Действие двойного тапа» — закрыто 2026-11-05.**
Настройки `OEChatsDoubleTapIn`/`OEChatsDoubleTapOut` (-1 — как в настройках
Telegram, 0 — ничего, 1/2 — любимая реакция, 4 — ответ), два селектора в
подсекции Chats, resolution в `exteraless/doubletap.{h,cpp}`, CLI `doubletap`
(22 проверки), Xvfb подтвердил reply, reaction и nothing. Остальные
Android-действия (translate/save/repeat/edit/delete/forward/download) — NB-91.

### 7.3 Definition of done для батча

1. Debug-сборка (`ninja -C out Telegram`) и запуск `out/bin/Telegram` под
   Xvfb.
2. `python3 exteraless/tests/chats/run_checks.py` с новой группой, без потери
   существующих 527 проверок; `exteraless/tests/plugins/run_checks.py` и
   `run_host_checks.py` остаются PASS (32 и 213).
3. `nix/update-patch.sh` и сверка sha256 патча — иначе повторяется дефект
   NB-42.
4. Пересобрать `result/` или явно убрать устаревший symlink (NB-40).
5. Обновить `docs/feature-mapping.md`, `STATUS.md`, `CHANGELOG.md`,
   `Agents/README.md` (включая счётчики тестов); по WEB proxy — либо
   отдельный раздел в `feature-mapping.md`, либо явная ссылка на
   `docs/web-proxy-plan.md`.

### 7.4 Что в этот batch не входит

NB-13 (широкие посты) и NB-15 (сортировка непрочитанных) — меняют геометрию и
индексацию списка, их лучше делать вместе, отдельным батчем после стабилизации
меню и времени. Feed, AI, плеер, Vosk и regex-фильтры — не начинать до
закрытия P0/P1: они требуют новых архитектурных слоёв.

## 8. Правила ведения бэклога

- Статус задачи меняется вместе с кодом, а не отдельным коммитом «обновили
  документ».
- Новая задача получает следующий свободный `NB-NN`; закрытые строки не
  удаляются, а получают ✅ с датой, чтобы история приоритетов не терялась.
- При расхождении оценки с `docs/nagram-feature-audit.md` правится аудит, а не
  только бэклог: два документа должны описывать одно состояние.
- Ключ добавляется в `exteraless/config.{h,cpp}` одновременно в геттер,
  сеттер, `fromJson` и список известных ключей, иначе импорт `.extera` сочтёт
  его неизвестным.
- Если задача требует правок в `Telegram/lib_ui/**`, она помечается как «вне
  Nix-патча» до начала работы.
