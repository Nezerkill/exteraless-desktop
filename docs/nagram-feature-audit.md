# Аудит функций Exteraless/NagramX и покрытие Exteraless Desktop

Дата: 2026-11-04. Документ описывает, какие функции Android-клиента
exteraless (exteraGram + NagramX + Nekogram + AyuGram) существуют, какие из них
уже перенесены в этот репозиторий, какие переносимы, а какие не имеют
desktop-аналога. Это обзорная карта: план работ лежит в
`docs/nagram-porting-backlog.md`, а точные списки ключей — в исходных отчётах
и в Android-дереве.

Документ собран из двух независимых read-only аудитов, которые проводились по
одному снимку кода и расходятся в части точных чисел. Расхождения не сглажены,
а перечислены в отдельном разделе вместе с причиной.

## 1. Источники и ревизии

| Источник | Что это | Ревизия на момент аудита |
|---|---|---|
| Этот репозиторий (desktop) | форк tdesktop 7.3.1 + `exteraless/` + Nix-упаковка | снимок отчётов — `f780d65`; текущий HEAD — `123a29f`, им исправлен P0 по Ghost |
| База форка | upstream-тег, от которого считается патч | `v7.3.1` |
| Android-клиент (снимок аудита) | исследовательский клон `exteraless/exteraless` | `4771c553` |
| Android-клиент (sibling-чекаут) | `../exteraless`, используется для сверки путей и новых ключей | `8b7ec40`; точные счётчики могут отличаться от снимка |
| Nix-сборка | `result/` и содержимое store-бинарника | снимок 2026-11-01 (устарел относительно текущего дерева) |

Оба отчёта работали только на чтение: `git status --porcelain` пуст, коммитов и
сборочных артефактов они не создавали. Единственное расхождение с текущим
деревом — уже упомянутый коммит `123a29f` «Fix inverted Ghost Mode read
toggles», поэтому статусы Ghost в этом документе учитывают исправление, а не
состояние снимка.

## 2. Методология

**Android.** Читались не приложение целиком, а точки, где живут настройки и их
потребители:

- конфиг-классы `OpenExteraConfig.kt`, `AppearanceConfig.kt`, `ChatsConfig.kt`,
  `GeneralConfig.kt`, `UtilsConfig.kt`, `IconPacksConfig.kt`, `GlyphConfig.kt`,
  `PillStackConfig.kt`, `AiConfig.java`, `FeedConfig.java`, `NaConfig.kt`,
  `NekoConfig.java`, `NekoXConfig.java`, `DialogConfig.java`;
- шим `com/exteragram/messenger/ExteraConfig.java`, через который плагины видят
  настройки как в exteraGram;
- экраны `app/exteraless/settings/OpenExtera*Activity.java`, `SettingsRegistry`;
- строки `res/values/strings_oe*.xml`, `strings_na.xml`, `strings_nax.xml`,
  `strings_neko.xml`;
- подсистемы `app/exteraless/{ai,backup,camera,chats,components,debug,drawer,feed,
  ghost,glyph,icons,math,notifications,nowplaying,pillstack,player,plugins,
  privacy,proxy,shortcuts,speech,updater,utils,links}`;
- наследие `tw/nekomimi/nekogram/*`, `com/radolyn/ayugram/*`,
  `xyz/nextalone/nagram/*`;
- Python-SDK плагинов `src/main/python/*` и тесты `tests/*.py`.

**Desktop.** Проверка велась grep-прогонами и чтением кода, без сборки:
поиск всех строковых `OE*`-ключей, поиск обращений `Exteraless::` по
`Telegram/SourceFiles`, поиск отсутствующих подсистем (`Nagram`, `Vosk`,
`IconPack`, `now playing`), проверка локализации секции настроек.

**Что реально запускалось.** Первый отчёт тесты не запускал и брал числа
проверок из `STATUS.md`. Второй отчёт прогнал раннеры: `run_checks.py` (chats,
config, general, backup, strip, reactions, cli), `run_checks.py` (сканер
манифестов плагинов) и `run_host_checks.py` (bwrap-host), все три — PASS;
дополнительно он сверял sha256 `nix/patches/exteraless.patch` с diff от
`v7.3.1` и смотрел `strings` по обоим бинарникам.

**Чего в аудите нет.** Оценок в часах, запуска Android-клиента, проверки
сетевых подсистем на живых API (переводчики, LLM, курсы, погода), проверки
паритета поведения «глазами» на устройстве. Там, где поведение выведено из
ключа и строки, но код-путь не читался построчно, это помечено как «по ключу».

Соглашение о путях: `exteraless/...` и `docs/...` — от корня репозитория;
путь вида `history/view/history_view_message.cpp` — относительно
`Telegram/SourceFiles`; путь вида `app/exteraless/...` — относительно
`TMessagesProj/src/main/java` Android-репозитория. Номера строк нигде не
указываются: они дрейфуют, поэтому названы файлы, функции и типы.

**Метки статуса.**

| Метка | Значение |
|---|---|
| ✅ | реализовано в форке: есть код и подтверждение из тестов или smoke-прогона |
| 🟡 | частично: подмножество функций либо поведение отличается от Android |
| ❌ | в форке отсутствует, но desktop-аналог есть и перенос реализуем |
| ⛔ | desktop-аналога нет: платформа, Java/Xposed, Android-плагины, железо |
| ➖ | уже есть в upstream tdesktop, порт не нужен |
| 🔧 | было сломано, исправлено в текущем дереве |

## 3. Масштаб: сколько функций и почему числа расходятся

Числа ниже — единственное место, где отчёты заметно расходятся. Причина не в
ошибке, а в способе счёта, поэтому здесь даны диапазоны и объяснение.

| Поверхность | Отчёт 1 | Отчёт 2 | Что в дереве | Причина расхождения |
|---|---|---|---|---|
| `NaConfig` (NagramX) | 242 ключа | 267 `val` | 250 вызовов `addConfig` | `val` считает и служебные поля объекта, `addConfig` — только пользовательские настройки; 242 — число «ключей» по перечню |
| `NekoConfig` (Nekogram) | 97 ключей | 98 `addConfig` | 98 `addConfig` | округление при перечислении |
| `NekoXConfig` | ~6 | 3 ключа + константы | 3 пользовательских ключа | отчёт 1 считал и константы типов |
| `OpenExteraConfig` | — | 10 | 10 `val` | — |
| `AppearanceConfig` | 61 ключ в перечне | 74 | 74 `val` | часть `val` — константы и хелперы, в перечень «настроек» не попали |
| `ChatsConfig` | 42 ключа в перечне | 50 | 50 `val` | то же: служебные поля против настроек |
| `GeneralConfig` | — | 10 | 10 `val` | — |
| `PillStackConfig` | 21 | — | 35 `val` | отчёт 2 не считал, отчёт 1 считал только пользовательские |
| `AiConfig` | 13 | — | набор ключей сервиса | отчёт 2 не выделял |
| `FeedConfig` | 4 на аккаунт | — | 4 на аккаунт | — |
| Шим `ExteraConfig` | ~170 геттеров | 152 геттера | ~150–170 | геттеры против уникальных настроек |
| Строки `OE*` в Android-коде | 179 уникальных | — | — | строки в коде, не настройки |
| Файлы `app/exteraless` | 268 | — | 269 java + 12 kotlin | разные шаблоны поиска |
| **Всего настроек Android** | **≈400** | **≈600 уникальных** | — | пересечения ключей между `NaConfig`/`NekoConfig` и экранами exteraless; `val` против `addConfig`; геттеры против ключей |
| **Ключей форка desktop** | **25** | **25** | 25 объявлено, 1 мёртвый (`OEGhostSendOfflineAfterOnline`) | оба отчёта сошлись; отчёт 1 дополнительно отмечал 3 тестовых литерала в поиске |
| Ключей `.extera` | 8 | 8 | 25 | на момент аудита было 8, после NB-04 — все 25 desktop-ключей |
| Локализация `strings_oe*` | 1045 строк | — | — | — |
| Локализация desktop-секции | 11 ключей `lng_settings_exteraless*` | 11 | 11 | оба отчёта сошлись |

Практический вывод: **порядок величины — сотни настроек на Android против
двух с половиной десятков на desktop**, то есть покрыто примерно 4–6 %.
Точная цифра зависит от метода счёта, поэтому в бэклоге задачи адресуются
ключами и путями, а не счётчиками.

## 4. Классификация функций

| Класс | Что входит | Отношение к desktop |
|---|---|---|
| **A. exteraless-собственные** | Pill Stack, cloud settings, AI-провайдеры, ghost/pill/chat-настройки, `.etg`/`.elyx` бэкап, модель плагинов, feed, камера, математика | часть уже перенесена, остальное реализуемо, но требует новых виджетов и подсистем |
| **B. NagramX (`NaConfig`)** | message saving, regex-фильтры, стили текста, переводчик, шорткаты, premium-пункты, пункты меню | почти всё отсутствует, но переносится как обычные настройки |
| **C. Nekogram (`NekoConfig`)** | ghost packets, `hidePhone`, `showIdAndDc`, `stickerSize`, DNS/DoH, IPv6, `unlimited*`, typeface | почти всё отсутствует |
| **D. upstream tdesktop** | перевод, AI-редактор, premium, stories, реакции, папки, архив, локальный пароль | уже есть, порт не нужен |
| **E. Android-only** | appicons, glyph, CameraX, APK-updater, Crashlytics, push/FCM, Vosk (как Android-модуль), MD3-плеер, launcher shortcuts, bottom tabs, tablet mode, вибрация, content-capture | по смыслу не переносится |
| **F. plugin/Xposed-only** | `app/exteraless/plugins/**`, хуки `xyz/nextalone/nagram/**`, `from java import …`, DEX, `.elyx` | несовместимо по определению |

Отчёты используют разные метки происхождения: первый — EX/NX/AND
(exteraGram / наследие Nagram-Nekogram-AyuGram / Android-only), второй —
A–F. Это одно и то же деление, и ниже используется второе.

## 5. Что уже реализовано в desktop

Проверено по коду, не по документации. Все ключи — `OE*`, живут в
`exteraless/config.{h,cpp}`, сохраняются в `tdata/exteraless.json`.

| # | Функция | Desktop-ключ | Точка входа в коде | Состояние |
|---|---|---|---|---|
| 1 | Relative last seen | `OERelativeLastSeen` | `data/data_peer_values.cpp` (`OnlineText`), используется в top bar, профиле и превью | ✅ |
| 2 | Comma after mention | `OEAddCommaAfterMention` | `chat_helpers/field_autocomplete.cpp`, `window/window_peer_menu.cpp` | ✅ |
| 3 | Exact counters | `OEGeneralDisableNumberRounding` | `lang/lang_tag.cpp` (`FormatCountToShort`) | ✅ для всех коротких счётчиков |
| 4 | Proxy auto-disable (VPN / mobile / Wi-Fi) | `OEProxyDisableConditions`, `OEProxyAutoDisabled` | `exteraless/proxy_rules.cpp`, `exteraless/network.cpp`, `core/application.cpp` (`StartProxyRulesWatcher`) | ✅ |
| 5 | Ghost: read / stories / online / typing | `OEGhostSendReadPackets`, `OEGhostSendReadStories`, `OEGhostSendOnline`, `OEGhostSendUploadProgress` | `exteraless/ghost_mode.cpp` + `mtproto/mtproto_request_filter.h`, вызовы в `apiwrap.cpp`, `api/api_views.cpp`, `api/api_updates.cpp`, `api/api_send_progress.cpp`, `data/data_stories.cpp` | 🟡 4 из 5; пятый ключ мёртв |
| 6 | Ghost per-chat exclusions | `OEGhostReadExclusions`, `OEGhostTypingExclusions` | `exteraless/config.cpp`, подменю в `window/window_peer_menu.cpp` | 🟡 UI и запись есть, сетевой эффект не измерялся |
| 7 | Deleted messages | `OESaveDeletedMessages` (default `false`) | `exteraless/deleted_messages.cpp`, `exteraless/deleted_messages_trim.h`, `boxes/deleted_messages_box.cpp`, `window/window_peer_menu.cpp` | 🟡 9 полей; гейт, лимит и ротация закрыты в NB-02, схема — NB-19 |
| 8 | Remove message tail | `OEChatsRemoveMessageTail` | `history/view/history_view_message.cpp` (`skipTail` → `BubbleRounding`) | ✅ |
| 9 | Hide reactions (channel/group/private) + hover-кнопка | `OEChatsHideReactions*` | `exteraless/chats.cpp`, `history/view/history_view_element.cpp` (`refreshReactions`), `history_view_message.cpp` (`reactionButtonParameters`) | ✅ |
| 10 | Strip tracking on open | `OEChatsStripTrackingOnOpen` | `exteraless/links.cpp`, `core/ui_integration.cpp` (`handleUrlClick`) | ✅ |
| 11 | Strip tracking on paste | `OEChatsStripTrackingOnPaste` | `exteraless/links.cpp`, `chat_helpers/message_field.cpp` | ✅ (plain-text/URL, лимит 4096) |
| 12 | Compact unread label | `OEChatsUnreadCapsule` | `history/view/history_view_element.cpp` (`UnreadBar::paint`), стили в `ui/chat/chat.style` | ✅ |
| 13 | Pill Stack (8 пилюль) | `OEPillStackEnabled`, `OEPillStackPills` | `Telegram/SourceFiles/exteraless/pill_stack.{h,cpp}`, `dialogs/dialogs_widget.cpp` | ✅ (CPU/RAM/Net/DC/Proxy/Cache/Ghost/LastSeen; метрики ОС — Linux) |
| 14 | Drawer: аккаунты, бейджи, порядок | `OEDrawerAccountsExpanded`, `OEMainMenuOrder` | `window/window_main_menu.cpp`, `settings/sections/settings_information.cpp` (`AccountsOptions`) | ✅ |
| 15 | Plugins stage 1+2 | `OEPluginStates` | `exteraless/plugins/scanner.cpp`, `Telegram/SourceFiles/exteraless/{plugin_runtime,plugins_box}.{h,cpp}` | ✅ по раннерам, ограничения — в `docs/feature-mapping.md` |
| 16 | `.extera` backup | — | `exteraless/backup.cpp` (25 ключей в трёх секциях, invisible-Unicode envelope) | ✅ 25 из 25 desktop-ключей |
| 17 | Брендинг | — | `core/launcher.cpp`, `boxes/about_box.cpp`, `core/version.h`, `Telegram/build/version` | ✅ |
| 18 | WEB proxy | — | `mtproto/web_proxy/*` (6 файлов) + `mtproto/details/mtproto_web_proxy_socket.{h,cpp}`, ~3.1k строк; UI в `boxes/connection_box.cpp`, статус в `core/application.cpp` | 🟡 код есть и компилируется, серверная часть (`tproxy-server`) не развёрнута |

Пункт 18 — desktop-only подсистема: её нет в Android-реестре и её не
упоминают `docs/feature-mapping.md`, `README.md` или `STATUS.md`, хотя код и
Nix-сборка её содержат и у неё есть отдельные планы `docs/web-proxy-plan.md`
и `docs/web-proxy-test-plan.md`. Наличие подтверждено строками
`Web Proxy Error: WebView carrier failed…` в обоих бинарниках и ссылками
`web_proxy` в `out/build.ninja`.

## 6. Функциональные семейства

Ниже — полный перечень семейств с оценкой объёма и статусом. Детальные
построчные перечни ключей остаются в исходных отчётах; здесь важны границы
семейств и то, куда они ложатся в tdesktop.

### 6.1 Сводная карта семейств

| Семейство | Android-источник | Объём | Desktop |
|---|---|---|---|
| Брендинг и ядро | `NaConfig`, `NekoXConfig`, `core/launcher.cpp` | 5–8 ключей | ✅ / ⛔ (иконки лаунчера, свои api_id) |
| Каркас настроек | `OpenExteraSettingsActivity`, `SettingsRegistry`, 8 экранов | 8 экранов, поиск | 🟡 одна страница с подсекциями |
| Appearance | `AppearanceConfig` (74 `val`) + `NaConfig`/`NekoConfig` | 74 + ~20 | ❌ почти всё, ⛔ часть |
| General / Other | `GeneralConfig` (10) + `NekoConfig`/`NaConfig` | ~40 | 🟡 3 реализовано |
| Chats и меню | `ChatsConfig` (50) + `NekoConfig`/`NaConfig` | ~120 | 🟡 6 реализовано |
| Ghost / deleted / edits (Ayu) | `NekoConfig`, `NaConfig`, `com/radolyn/ayugram/*` | ~50 | 🟡 6 частично |
| Прокси и сеть | `OEProxy*`, `NekoConfig` (DNS/IPv6) | ~10 | ✅ базово, ❌ DoH/IPv6 |
| Перевод и LLM | `NekoConfig`, `tw/nekomimi/nekogram/{translate,llm}` | ~55 | ➖ серверный перевод, ❌ провайдеры/LLM |
| AI-ассистент | `AiConfig`, `app/exteraless/ai/*` | 13 ключей + UI | ❌ (upstream AI — другой продукт) |
| Feed (лента каналов) | `app/exteraless/feed/*` (14 файлов), `FeedConfig` | 4 ключа на аккаунт | ❌ не начато |
| Cloud settings | `OpenExteraCloudActivity`, `CloudSettingsHelper` | 4 ключа | ❌ |
| Pill Stack | `PillStackConfig`, `app/exteraless/pillstack/*` | 15 типов пилюль | ✅ 8 из 15 |
| Drawer и навигация | `app/exteraless/drawer/*`, `MainMenuLayout` | 20 пунктов меню | 🟡 5 пунктов, порядок, аккаунты |
| Плеер и now playing | `app/exteraless/player/*` (21 файл), `nowplaying/*` | 30+ ключей | ❌ MD3-плеер, ➖ MPRIS |
| Иконпаки / эмодзи / глиф | `icons/*` (15 файлов), `appicons/*`, `glyph/*` | ~20 | ⛔/❌ (иконки tdesktop — `lib_ui` + `.style`) |
| Плагины | `plugins/*`, `src/main/python/*`, `ExteraConfig` шим | 9 разрешений, 3 уровня | ✅ stage 1–2, ❌ stage 3, ⛔ Xposed |
| Приватность | `app/exteraless/privacy/*`, `NekoConfig` | ~8 | ❌, ➖ локальный пароль |
| Медиа: субтитры, качество | `utils/VideoSubtitlesHelper`, `NaConfig`/`NekoConfig` | ~12 | 🟡/❌ |
| Текст, математика, типографика | `math/*`, `tw/nekomimi/nekogram/{filters,syntaxhighlight,shamsicalendar}` | ~30 | ❌ |
| Речь: Vosk и транскрипция | `speech/*` (5 файлов), `helpers/TranscribeHelper` | 4 провайдера + офлайн | ❌ |
| Бэкапы и миграция | `backup/*`, `SettingsBackupHelper`, `FaveStickersBackup` | 7 форматов | 🟡 1 формат (25 ключей) |
| WEB proxy (desktop-only) | — | ~3.1k строк | 🟡 код есть, сервер нет |

### 6.2 Appearance (74 пункта `AppearanceConfig`)

Семейство самое большое по числу пунктов и самое «анти-desktop» по природе:
экраны Android перерисовывают Material-компоненты, а tdesktop рисует свои
виджеты через `.style`-файлы. Группы:

| Группа | Примеры ключей | Desktop |
|---|---|---|
| Скругления и аватарки | `avatarCorners`, `singleCornerRadius`, `avatarSquareness`, `senderMiniAvatars` | ❌ (геометрия через `dialogs/dialogs_row.cpp`, `ui/userpic_view.cpp`, маска — в `lib_ui/ui/image/*`) |
| Шапки и навигация | `iosChatHeader`, `iosNavigationBarStyle`, `newChatHeaderStyle`, `centerTitle`, `hideActionBarStatus` | ❌ (`history/view/history_view_top_bar_widget.cpp`, `window/window_top_bar_wrap.h`) |
| Секции и списки | `sectionRadius`, `separateHeaders`, `sectionsSeparatedHeaders`, `m3ListItems` | ❌ (карточек-секций в настройках tdesktop нет) |
| Стекло и блюр | `glassMessageMenu`, `glassOutlineStyle`, `forceBlur` | ❌ (`menu/*`, `window/themes/*`) |
| Прогресс/свитчи/слайдеры | `newLoadingStyle`, `newSwitchStyle`, `newSliderStyle` | ❌, часть — `lib_ui/ui/widgets/*` |
| Вкладки и drawer | `navigationDrawer`, `immersiveDrawerAnimation`, `bottomNavigation*`, `tabletMode`, `tabCounter`, `mainTabsHide*` | ⛔ в tdesktop нет табов и нижней навигации; меню и есть drawer |
| Разделители | `dividerStyle`, `hideDividers` | ⛔ у строк списка чатов в 7.3.1 нет разделителей |
| Эмодзи и шрифты | `useSystemEmoji`, `emojiSets`, `typeface`, `iconPacks` | ⛔/❌ (`lib_ui/ui/emoji_config.cpp`, иконки `.style`) |
| Monet и M3-компоненты | `monetStyle`, `m3ListItems` | ⛔ динамические цвета Android 12+ |
| Плеер | 30+ `OEPlayer*` | ❌ см. 6.14 |
| Прочее | `forceSnow`, `profileMusicCard`, `titleText`, `showFeedTab`, `feedUnreadCounter` | ❌ |

Отдельно: `docs/feature-mapping.md` фиксирует, что `OEAppearance*` не
портирован, и это подтверждается поиском — в desktop нет ни одного такого
ключа. Утверждение верное, но неполное: оно не говорит, что часть пунктов
непереносима в принципе (табы, Monet, разделители, иконпаки).

### 6.3 General и Other

| Функция | Android-ключ | Desktop |
|---|---|---|
| Точные счётчики | `OEGeneralDisableNumberRounding` | ✅ `lang/lang_tag.cpp` |
| Относительное «был в сети» | `OERelativeLastSeen` | ✅ `data/data_peer_values.cpp` |
| Время с секундами | `NekoConfig.showSeconds` | ❌ (`history/view/history_view_bottom_info.cpp`, `dialogs/ui/dialogs_layout.cpp`) |
| Фильтр Zalgo | `NaConfig.zalgoFilter` | ❌ (`ui/text/*`, `data/data_messages.cpp`) |
| Boost скачивания/загрузки | `OEGeneralDownloadSpeedBoost`, `NekoConfig.uploadBoost` | ❌ (`data/data_download_manager.cpp`, `mtproto/session_private.cpp`) |
| IPv6 / DNS over HTTPS | `NekoConfig.useIPv6`, `dnsType`, `customDoH` | ❌ (`mtproto/connection_*`), upstream умеет IPv6 в настройках соединения |
| Скрыть телефон, показать ID/DC | `NekoConfig.hidePhone`, `NaConfig.idDcType` | ❌ (`info/profile/info_profile_values.cpp`, `info_profile_top_bar.cpp`) |
| Сортировка списка чатов | `sortByUnread`, `sortByUnmuted`, `sortByUser`, `sortByContacts` | ❌ (`dialogs/dialogs_indexed_list.cpp`) |
| Скрыть архив, свайп-архив | `hideArchive`, `openArchiveOnPull`, `doNotUnarchiveBySwipe` | ❌/➖ (`dialogs/dialogs_widget.cpp`) |
| Папка сохранения и подпапка на чат | `customSavePath`, `saveToChatSubfolder` | 🟡/❌ (`data/data_download_manager.cpp`, `settings/sections/settings_advanced.cpp`) |
| Bot privacy (выделенный аккаунт) | `OEGeneralBotPrivacyUserId`, `botPrivacyEnabled`, `alwaysAsk` | ❌ (`main/main_domain.cpp`, `Window::ShowAccountSelector`) |
| Локальный premium, игнор ограничений | `localPremium`, `ignoreContentRestrictions` | ❌ (спорные по смыслу) |
| Неограниченные пины | `NekoConfig.unlimitedPinnedDialogs` | ⛔ лимит серверный (`appConfig`), клиентского лимита в 7.3.1 нет |
| Вибрация, пузырьки, proximity, батарея, Crashlytics, content capture | `disableVibration`, `disableNotificationBubbles`, `disableProximityEvents`, `batteryOptimization`, `OEGeneralCrashReports`, `OEGeneralContentCapture` | ⛔ Android/ОС |
| Push-транспорты | `pushServiceType*` | ⛔ инфраструктура Android |
| Глиф, quick tiles, виджеты, jank-профайлеры | `OEGlyph*`, `QuickToggleActivity`, `ChatsWidgetConfigActivity`, `debug/*` | ⛔ железо/платформа |

### 6.4 Chats и меню сообщений

| Группа | Примеры ключей | Desktop |
|---|---|---|
| Уже перенесено | `removeMessageTail`, `hideReactions*`, `stripTrackingOnOpen/Paste`, `unreadCapsule` | ✅ |
| Стикеры | `stickerSize`, `stickerShape`, `stickerTimeSide`, `hideTimeForSticker` | ❌ (`history/view/media/history_view_sticker.cpp`; в tdesktop уже есть `base::options::option<int> OptionStickerSize`) |
| Ответы и цитаты | `replyColors`, `replyEmoji`, `replyBackground`, `messageColoredBackground` | ❌ (`history/view/history_view_reply.cpp`) |
| Пункты меню сообщения | ~24 ключа `NekoConfig.show*`, `NaConfig.show*`, `ActionBarButton*` | 🟡 пункты есть безусловно, ключей видимости нет (`history/view/history_view_context_menu.cpp`, `window/window_peer_menu.cpp`) |
| Пункты меню чата | 14 ключей `NaConfig.chatMenuItem*`, `shortcuts*` | ❌ (`window/window_peer_menu.cpp`) |
| Меню медиавьюера | 6 ключей `mediaViewerMenuItem*` | ❌ (`media/view/media_view_overlay_widget.cpp`) |
| Группировка и порядок меню | `groupedMessageMenu`, `defaultDeleteMenu`, `textStyleOrder` | ❌ |
| Широкие посты | `wideChannelPosts`, `wideFeedPosts` | ❌ (`history/view/history_view_message.cpp`) |
| Дата на форварде, иконка вместо «edited» | `dateOfForwardedMsg`, `replaceEditedWithIcon`, `customEditedMessage` | ❌ (`history_view_bottom_info.cpp`) |
| Двойной тап | `doubleTapAction`, `doubleTapActionOut` | ❌ (`history_view_list_widget.cpp`, сейчас двойной тап = reply) |
| Опросы | `showResultsBeforeVoting` | ❌ (`data/data_poll.cpp`, `history/view/media/history_view_poll.cpp`) |
| Inline-математика | `inlineMathResult`, `inlineMathCurrency` | ❌ (`chat_helpers/message_field.cpp`, `field_autocomplete.cpp`) |
| Скрытие кнопок профиля/шапки | `hideGiftButtonInChannel`, `hideMoneyButton`, `hideCallButton`, `hideShareButtonInChannel`, `hideChannelSearchButton`, `hideSendAsPeer`, `hideCameraTile` | ❌ (`info/profile/*`, `history_view_top_bar_widget.cpp`, `compose_controls`) |
| Медиа и качество | `alwaysSendInHD`, `preferOriginalQuality`, `hdrPhotos`, `doubleTapSeekDuration`, `swipeToPip`, `zoomSlider`, `staticZoom`, `enhancedVideoBitrate`, `defaultHlsVideoQuality`, `playerDecoder` | ❌/➖ (`media/view/*`, `media/streaming/*`; в upstream есть булев hwaccel) |
| Автопауза и звук | `pauseVoice`, `pauseRound`, `autoPauseVideo`, `unmuteWithVolumeButtons`, `noiseSuppressAndVoiceEnhance`, `dontAutoPlayNextVoice` | ❌/⛔ (`media/audio/*`, `calls/*`) |
| Прочее | `keepAttachButton`, `bottomButton`, `rememberAllBackMessages`, `hideKeyboardOnChatScroll`, `disableGlobalSearch`, `confirmAllLinks`, `fixLinkPreview`, `disableLinkPreviewByDefault`, `silentMessageByDefault`, `deleteChatForBothSides`, `replyToDeletedAsQuote`, `sendMp4DocumentAsVideo`, `takeGIFasVideo`, `customAudioBitrate` | ❌/🟡 |
| Камера кружков | `cameraType`, `cameraMirrorMode`, `cameraStabilization`, `extendedFps`, `alwaysSendInHD`, `hideCameraTile` | ⛔ CameraX и драйверы |

### 6.5 Ghost, удалённые и изменённые сообщения

| Функция | Android-ключ | Desktop |
|---|---|---|
| Не отправлять read receipts | `sendReadMessagePackets` | ✅ `exteraless/ghost_mode.cpp`, allow-list в `apiwrap.cpp`, `api/api_views.cpp` |
| Не отправлять просмотры историй | `sendReadStoriesPackets` | ✅ `data/data_stories.cpp` |
| Не отправлять online | `sendOnlinePackets` | ✅ `api/api_updates.cpp` |
| Не отправлять typing/upload | `sendUploadProgress` | ✅ `api/api_send_progress.cpp`, `chat_helpers/emoji_interactions.cpp`, `history/history_streamed_drafts.cpp` |
| Offline после online | `sendOfflinePacketAfterOnline` | 🟡 ключ `OEGhostSendOfflineAfterOnline` объявлен, но не читается и не экспортируется |
| Per-chat исключения | `ghostModeReadExclusion_<id>`, `ghostModeTypingExclusion_<id>` | ✅/🟡 UI и запись есть, сетевой эффект не измерялся |
| Пакетный переключатель и статус | `setGhostMode`/`toggleGhostMode`, `showGhostModeStatus`, `showGhostInDrawer` | ❌ (пункт меню Ghost есть, статуса и тумблера «всё сразу» нет) |
| «Locked» тумблеры | `*Locked` (5 ключей) | ❌ |
| Сохранять удалённые | `enableSaveDeletedMessages` | ✅ ключ `OESaveDeletedMessages` (default `false`) гейтит запись; область сохранения — ❌ |
| Область сохранения удалённых | `saveDeletedIn*`, `saveInArchivedChats`, `saveDeletedExceptionsMode` | ❌ (сохраняется всё) |
| Медиа в сохранённых | `messageSavingSaveMedia`, `saveMediaIn*` | ❌ (desktop пишет только имя медиа) |
| Метка и меню удалённого | `customDeletedMark`, `deletedMarkColor`, `translucentDeletedMessages`, `useDeletedIcon`, `chatMenuItemView/ClearDeleted` | 🟡 есть бокс списка и пункт меню, метки в чате нет |
| История правок | `enableSaveEditsHistory`, `saveEditedExceptionsMode`, `useEditedIcon` | ❌ |
| Локальный last seen и read date | `saveLocalLastSeen`, `saveReadDate` | ❌ |
| Regex-фильтры и фильтр реакций | `regexFilters*`, `blockedChannelsData`, `customFilteredUsersData` | ❌ (`dialogs/dialogs_inner_widget.cpp`, `data/data_histories.cpp`) |
| Ghost-отправка «как отложенное» | `ghostScheduledSend` | ❌ |
| Ответ на удалённое цитатой | `replyToDeletedAsQuote` | 🟡 текст хранится, подстановки нет |
| Импорт базы AyuGram | `OEAyuDatabaseImport`, `ClearMessageDatabase` | ❌ |

Схемы хранения расходятся принципиально: Android — Room с таблицами
`DeletedMessage`, `DeletedMessageFull`, `EditedMessage`, `DeletedDialog`,
`DeletedMessageReaction`, `LastSeenEntity`, `SpyMessageRead`,
`SpyMessageContentsRead` и базовым классом примерно на 35 полей; desktop —
JSON-sidecar `tdata/exteraless/deleted_messages.json` с 9 полями (`peerId`,
`peerName`, `msgId`, `fromId`, `fromName`, `date`, `text`, `media`, `out`).
Реакции, ответы, форварды, entities, медиа-файлы, история правок и read-date в
desktop-схеме отсутствуют.

### 6.6 Прокси и сеть

| Функция | Android-ключ | Desktop |
|---|---|---|
| Авто-отключение по VPN / mobile / Wi-Fi | `OEProxyDisableConditions` (биты 1/2/4) | ✅ `exteraless/proxy_rules.cpp`, `exteraless/network.cpp` |
| Восстановление после смены сети | — | ✅ `OEProxyAutoDisabled`, watcher в `exteraless/network.cpp` |
| Чипы-условия в UI | `ProxyDisableChipsView` | 🟡 чекбоксы в настройках вместо чипов |
| Пинг прокси | `ProxyPingController` | ➖ `boxes/connection_box.cpp` |
| DNS over HTTPS, свой DoH | `NekoConfig.dnsType`, `customDoH` | ❌ (`mtproto/*`) |

### 6.7 Перевод и LLM

| Функция | Android-ключ | Desktop |
|---|---|---|
| Кнопка «Перевести» | `NekoConfig.showTranslate` | ➖ upstream (`lang/translate_provider.cpp`, `boxes/translate_box.cpp`) |
| Провайдер перевода | `translationProvider` | ➖ только серверный Telegram; 11 Android-провайдеров (Google, Yandex, DeepL, Microsoft, LLM и др.) отсутствуют |
| Целевой язык | `translateToLang`, `translateInputLang` | 🟡 переводит на язык интерфейса |
| Ключи Google Cloud / DeepL и формальность | `googleCloudTranslateKey`, `deepLTranslateKey`, `deepLFormality` | ❌ |
| Режимы и автоперевод | `translatorMode`, `telegramUIAutoTranslate`, `enableSeparateArticleTranslator` | ❌/➖ |
| LLM-переводчик | `llmApiUrl`, `llmApiKey`, `llmModelName`, `llmSystemPrompt`, `llmUserPrompt`, `llmTemperature`, `llmUseContext`, 10 пресетов провайдеров | ❌ |
| Markdown при переводе | `translatorKeepMarkdown` | ❌ |

### 6.8 AI-ассистент

Собственный AI (не Telegram AI): `AiConfig` с сервисами `endpoint`+`model`+`key`,
роли и промпты, локальная история, стриминг, вставка ответа цитатой,
замена Telegram-редактора и саммари. В desktop отсутствует; upstream имеет
`boxes/compose_ai_box.cpp` и `api/api_compose_with_ai.cpp` — это другой
продукт (Telegram AI), а не пользовательские провайдеры. Ключи скрытия
AI-кнопок (`OEAppearanceHideAi*`) тоже не портированы.

### 6.9 Feed

Лента каналов реализована на Android как «синтетический чат» внутри
`ChatActivity` (`FeedController`, `FeedTimelineLoader`, `FeedStore`,
`FeedBackfillCoordinator`, `FeedUnreadTracker`, `FeedChannelRegistry`,
`FeedMessageIdentityMap`, `FeedRequestNormalizer`, `FeedChatIntegration`).
Настройки: `showFeedTab`, `feedUnreadCounter`, `excludedChannels`,
`includeArchived`, `addNewChannels`, `markAllRead` — на аккаунт. В desktop нет
ни ключа, ни режима списка; для переноса потребуется собственный тип строки
`Dialogs::Row` или отдельный список плюс синтетическая история в `data/`.

### 6.10 Облако настроек

`OpenExteraCloudActivity` + `CloudSettingsHelper`: выгрузка настроек в
аккаунт (служебное сообщение), автосинк по изменению prefs, восстановление,
очистка, опция включать API-ключи. Ключи `OECloudUpload`, `OECloudAutoSync`,
`OECloudRestore`, `OECloudClear`, `CloudConfigIncludeApiKeys`. В desktop нет;
потребуется канал и служебные сообщения в `storage/storage_account.cpp`.

### 6.11 Pill Stack

15 типов пилюль в Android против 8 в desktop.

| Тип | Android id | Desktop |
|---|---|---|
| CPU, RAM | `CPU(9)`, `RAM(8)` | ✅ (`/proc`) |
| Скорость сети | `NET_SPEED(10)` | ✅ (`/proc/net/dev`) |
| DC ping | `DC_PING(11)` | ✅ (`mtp.pingTime()`) |
| Прокси | `PROXY(6)` | ✅ |
| Кэш | `CACHE(5)` | ✅ |
| Ghost | `GHOST(7)` | ✅ |
| Мой статус | `LAST_SEEN(15)` | ✅ |
| Погода | `WEATHER(1)` | ❌ (нужен провайдер, кэш, геолокация) |
| Telegram-подписки («gram») | `GRAM(2)` | ❌ |
| BTC / USD / EUR / ETH / GOLD | `BTC(3)`, `USD(4)`, `EUR(14)`, `ETH(13)`, `GOLD(12)` | ❌ (сетевые курсы) |
| Настройка видимых и порядок | `PillStackPillsSettingsInfo`, `savePillsLayout` | 🟡 видимость есть (`OEPillStackPills`, маска 8 бит), drag-порядка нет |
| Бесконечная прокрутка, сброс раскладки | `PillStackInfiniteScrolling`, `PillStackReset` | ❌ |

Ограничение: маска видимости — 8 бит, поэтому новые типы пилюль требуют
расширения хранения (`exteraless/config.h`), а не просто нового `enum`.

### 6.12 Drawer и навигация

| Функция | Android-ключ | Desktop |
|---|---|---|
| Шторка вместо меню «⋮» | `OEAppearanceNavigationDrawer` | 🟡 меню tdesktop само выезжает панелью (`window/window_main_menu.cpp`) |
| Состав и порядок пунктов | `OEAppearanceMainMenuLayout`, `showGhostInDrawer` | 🟡 `OEMainMenuOrder` — 5 пунктов (ghost, exteraless, pillstack, proxy, storage) против 20 в Android |
| Скрытые пункты, «Закрыть приложение» | `OEMainMenuHiddenItems`, `KILL_APP` | ❌ |
| Аккаунты, бейджи, порядок | `DrawerAccountPickerView`, `DrawerUnreadBadge` | ✅ `Settings::SetupAccounts`, `AccountsOptions`, `OEDrawerAccountsExpanded` |
| Иммерсивная анимация | `OEAppearanceImmersiveDrawer` | ❌ |
| Нижние вкладки | `hideBottomNavigationBar`, `mainTabsHide*` | ⛔ |

### 6.13 Плеер и now playing

MD3-плеер — 21 файл (`Md3Player`, `PlayerSheet`, `PlayerBarView`,
`PlayerMiniView`, `PlayerQueueSheet`, `PlayerStyleEditor`, `WavySeekBar`,
`Lyrics`, `OnlineLyrics` и др.) и 30+ ключей `OEPlayer*`: стиль обложки,
размер, радиус, вращение, выравнивание заголовка, порядок блоков, цвета, фон,
11 ключей мини-плеера, тексты песен из файла и онлайн (LRCLIB/lrcmux).
В desktop аналога нет; `media/view/media_view_overlay_widget.cpp` и
`media/audio/media_audio.cpp` дают обычный плеер. Очередь, скорость и
shuffle в upstream есть. System media controls (MPRIS) — upstream
`media/system_media_controls_manager.cpp`, порт не нужен.

### 6.14 Иконпаки, эмодзи, глиф

`IconPackManager`/`IconPackStorage`/`IconPackProvider`/`IconPacksEditorActivity`
(архив `.icons` с `metadata.json`, приоритет, лимиты), пикер иконки из
картинки, 16 аппиконок лаунчера, наборы эмодзи, глиф Nothing. В desktop
иконки — `lib_ui/icons` и `.style`, поэтому иконпаки упираются в подсистему
`lib_ui` (сабмодуль, в Nix-патч не входит). Аппиконки и глиф — ⛔.

### 6.15 Плагины

| Слой | Android | Desktop |
|---|---|---|
| Движок | Chaquopy Python 3.11/3.12 внутри процесса | ✅ `exteraless/plugin_runtime.cpp` — отдельный `bwrap` + `python3 -I -B`, JSON-lines IPC |
| Манифест | `Plugin.java`, `metadata_parser.py` | ✅ `exteraless/plugins/scanner.cpp` (AST без исполнения, `.plugin`/`.py`, лимит 2 MiB) |
| Разрешения (9) | `PluginPermissions.java` | 🟡 сканер знает список, исполнение не ограничивается (informational) |
| Уровни доступа | isolated / gated / trusted | ❌ |
| Capability scan | `capability_scan.py` | ❌ (сканер читает только метаданные) |
| Audit-журнал | `PluginActivityLog`, `PluginActivityExport` | 🟡 пишется `tdata/plugins-audit.log`, просмотра в UI нет |
| Watchdog, safe/unsafe, авто-safe-mode | `PluginsWatchdog`, `PluginsSafeMode`, `PluginsAutoSafeMode` | ❌ |
| Установка / удаление / обновление | `PluginsInstallFromFile`, `PluginsDelete`, `PluginsUpdate` | ❌ |
| Меню и настройки плагина | `plugins/menus`, `PluginSettingsActivity` | ❌ |
| Intents, бэкап плагинов, Python SDK | `plugins/intents`, `PluginsBackup`, `src/main/python/*` | ❌ |
| Xposed-хуки, DEX, `.elyx`, шим `ExteraConfig` | `plugins/xposed/*`, `elyx_runtime/*`, `ExteraConfig.java` | ⛔ нет ART и совместимого API |

### 6.16 Приватность и защита настроек

Выделенный аккаунт для ботов (`OEGeneralBotPrivacyUserId`,
`botPrivacyEnabled`, `alwaysAsk`), «ложное дно» настроек
(`OEPrivacyFalseBottom*`), скрытие телефона и ID/DC, content capture,
игнорирование sensitive-фильтра. Desktop: локальный пароль и блокировка окна
есть в upstream (`settings_local_passcode.cpp`, `window/window_lock_widgets.cpp`),
скрытия секций и маршрутизации ботов нет.

### 6.17 Медиа: субтитры, качество, кодеки

Внешние субтитры (`.srt`/`.ass`, `OESubtitles`, `OEChooseSubtitles`),
HLS-качество по умолчанию, улучшенный битрейт, выбор декодера, GIF как видео,
свой битрейт аудио, MP4-документ как видео, премиум-эмодзи стикером, правила
автоскачивания архивов и `.exe`. Desktop: субтитры в видео есть, внешних
файлов нет; правила автоскачивания по типам есть в
`settings/sections/settings_local_storage.cpp`; остальное отсутствует.

### 6.18 Текст, математика, типографика

Inline-математика (`math/*`: `MathExpression`, `InlineMathController`,
`GhostTextLayout`, `MathRevealAnimation`), валюты через `@calcmulabot`,
Pangu (пробелы между CJK и латиницей), Zalgo, выбор markdown-парсера, порядок
и состав тулбара форматирования (10+ ключей `showText*`), связанные
кастом-эмодзи, шрифт ссылок, показ спойлеров сразу, подсветка синтаксиса,
персидский календарь. В desktop — только upstream-поведение (markdown при
вставке, спойлеры, ссылки); остальное отсутствует.

### 6.19 Речь: Vosk и транскрипция

Офлайн-распознавание Vosk (`speech/VoskManager`, `VoskModel(s)`,
`VoskTranscriber`, `VoskSettingsActivity`) со скачиванием моделей, и четыре
провайдера транскрипции (`transcribeProvider`: Cloudflare, Gemini, OpenAI,
дефолт) с ключами и промптами. В desktop нет ни того, ни другого: upstream
умеет серверную транскрипцию Telegram (`menu_item_rate_transcribe*`), но
пользовательских провайдеров не имеет. VK-специфичных функций в Android-дереве
не найдено (поиск по `VK` пуст), поэтому «VK» в перечне семейств не выделяется
в отдельную строку.

### 6.20 Бэкапы и миграция

| Формат | Android | Desktop |
|---|---|---|
| `.extera` (невидимые Unicode + JSON) | `EtgBackup`, `InvisibleEncryptor` | ✅ `exteraless/backup.cpp`, 25 ключей |
| Полный `.json` всех настроек | `SettingsBackupHelper` | ❌ |
| Импорт базы AyuGram | `AyuDatabaseImport` | ❌ |
| Бэкап плагинов | `PluginsBackup` | ❌ |
| Бэкап избранных стикеров | `FaveStickersBackup` | ❌ |
| Облачный синк | `CloudSettingsHelper` | ❌ |
| Экспорт данных аккаунта | — | ➖ upstream `export/` (другой смысл) |

### 6.21 WEB proxy (desktop-only)

Подсистема есть только в этом репозитории: `mtproto/web_proxy/web_proxy_frame`,
`web_proxy_transport`, `web_proxy_webview` и
`mtproto/details/mtproto_web_proxy_socket`, около 3.1k строк. Смысл — MTProxy,
у которого внешний транспорт идёт через скрытый нативный WebView, а не через
прямой сокет; UI в `boxes/connection_box.cpp`, статус в `core/application.cpp`.
Код компилируется и попадает в оба бинарника, но hosted-часть
(`tproxy-server`) не развёрнута, а `docs/web-proxy-plan.md` и
`docs/web-proxy-test-plan.md` описывают дизайн и будущую процедуру проверки.
В `docs/feature-mapping.md` подсистема не упомянута — это пробел документации,
а не отсутствие функции.

## 7. Android-only: буквальный перенос невозможен

| Блок | Почему | Что можно переосмыслить |
|---|---|---|
| Xposed/Java-хуки плагинов (`plugins/xposed/*`) | нет ART, нет механизма перехвата методов | ничего: desktop-плагины работают через IPC-host |
| DEX внутри плагинов, `.elyx`, Chaquopy | нет Dalvik/ART и встроенного Python-рантайма | собственный SDK на Python (stage 3) |
| Шим `com.exteragram.messenger.ExteraConfig` | API плагинов Android-клиента | новый desktop API плагинов |
| App widgets, quick settings tiles, launcher shortcuts | нет платформенных точек | меню приложения и горячие клавиши |
| Аппиконки лаунчера, adaptive icons | нет лаунчера | иконка окна/трея |
| Glyph Interface (Nothing) | железо | — |
| CameraX, камера кружков, proximity, вибрация | железо и драйверы | «параметры записи кружков» отдельной задачей |
| Monet / dynamic color, M3-компоненты | Material-компоненты Android | свои `.style`-токены |
| Push (FCM/microG/UnifiedPush), notification bubbles/icon | инфраструктура Android | upstream-уведомления |
| Google/OSM Maps SDK, геолокация | SDK и разрешения | upstream-карта |
| TabletMode, нижняя навигация, tablet-раскладки | другая модель окон | — |
| ART Profile Saver, compatibility mode | рантайм Android | — |
| Crashlytics, battery optimization, FLAG_SECURE | сервисы ОС | upstream `logs.cpp` и crash-репорты |
| Иконпаки `.icons`, `typeface`, `useSystemEmoji`, стили свитчей/слайдеров | подсистема иконок и шрифтов tdesktop в `lib_ui`, общий upstream | только через `lib_ui`, то есть вне Nix-патча |
| `unlimitedPinnedDialogs` | лимит пинов приходит с сервера в `appConfig`, клиентского лимита в 7.3.1 нет | — |
| Разделители строк списка чатов | у строк нет линии, рисовать нечего | — |

## 8. Уже есть в upstream tdesktop (порт не нужен)

Перевод сообщений (серверный), тихая отправка, отложенные сообщения,
копирование ссылки на сообщение, папки-вкладки, архив, локальный пароль,
экспорт данных аккаунта, MPRIS/медиа-контролы, булев аппаратный видео-декодер
в Advanced, спойлеры, markdown при вставке, IPv6 в настройках соединения,
пинг прокси в списке, темы отдельных чатов, наборы эмодзи из
`chat_helpers/emoji_sets_manager.cpp`.

Не подтверждено как upstream и потому остаётся в бэклоге: «Message details»
(в контекстном меню tdesktop такого пункта нет), выбор HW/SW-декодера (есть
только булев hwaccel), «Unblock deleted users» (нет).

## 9. Документация против кода

| # | Что | Где | Суть | Статус |
|---|---|---|---|---|
| M1 | Инвертированное условие Ghost в счётчике просмотров | `api/api_views.cpp` (`ViewsManager::viewsIncrement`), патч `nix/patches/exteraless.patch` | было `MTP_bool(true)`, затем `MTP_bool(!ghostSendReadPackets())`, из-за чего при дефолте просмотры не отправлялись вовсе | 🔧 исправлено в `123a29f`: условие снова `ghostSendReadPackets()`, плюс исправлены инвертированные подписи четырёх тумблеров «Don't send …» в `settings_exteraless.cpp`, патч перегенерирован |
| M2 | Deleted messages: мёртвый ключ, всегда включено, без лимита | `exteraless/config.cpp` (`saveDeletedMessages`), `exteraless/deleted_messages.cpp`, `core/application.cpp` (`attach`) | было: `OESaveDeletedMessages` записан и принимается `fromJson`, но не читается; запись шла всегда; `save()` синхронно перезаписывает весь JSON на каждое удаление, без cap и ротации | 🔧 закрыто в NB-02 (2026-11-05): ключ читается перед записью, default `false`, cap 1000 записей/100 на чат с ротацией при добавлении и загрузке, тумблер в Chats; расширение схемы — NB-19 |
| M3 | docs: «UI: a deleted-message mark in the chat» | `docs/feature-mapping.md`, раздел Deleted messages | метки в чате нет: `DeletedMessages` используется только в `window/window_peer_menu.cpp` (счётчик и бокс), `history/view/**` о ней не знает | открыто, P1 |
| M4 | docs: «On `UpdateDeleteMessages`, the message is saved before the local history is cleared» | там же | реализация висит на `session->data().itemsAboutToBeDestroyed`, то есть ловит любое уничтожение элемента (очистка истории, выход из канала, сброс кэша), а не только серверное удаление | открыто, P1 |
| M5 | `OEGhostSendOfflineAfterOnline` — мёртвый ключ | `exteraless/config.cpp` | объявлен, но нигде не читается и нет в UI; в `.extera` экспортируется для полноты, семантика Android `sendOfflinePacketAfterOnline` не реализована | открыто, P1 |
| M6 | docs: «Reactions in the pinned bar can keep their old state until a restart» | `docs/feature-mapping.md`, Chats | в 7.3.1 нет pinned-reactions бара: поиск по `pinnedReactions` и `ReactionsStrip` по `Telegram/SourceFiles` пуст, файлов `history_view_reactions*` нет | устаревшее утверждение, P2 |
| M7 | `result/` устарел | `result/bin/.Telegram-wrapped` (симлинк/деривация от 2026-11-01) против `out/bin/Telegram` (2026-11-04) | в Nix-бинарнике нет `Plugin sandbox is unavailable` и новых строк; `STATUS.md` признаёт, что пакет не пересобран, но в «Проверках» всё ещё ссылается на старый `result` как на проверенный | открыто, P1 |
| M8 | `nix/smoke-test.sh` запускает каталог | `nix/smoke-test.sh` | запускается `./out/Telegram`, а это build-каталог; реальный бинарник — `out/bin/Telegram` | открыто, P1 |
| M9 | Настройки не локализованы | `settings/sections/settings_exteraless.cpp` | почти все подписи Ghost/Chats/Pill Stack/Plugins — захардкоженный английский `rpl::single(u"…"_q)`; в `Telegram/Resources/langs/lang.strings` только 11 `lng_settings_exteraless*` (general/proxy/backup) | открыто, P1 |
| M10 | Хрупкий процесс генерации патча | `nix/update-patch.sh` | база — локальный тег `v7.3.1`, который уже содержит fork-код (`mtproto/web_proxy/*`, `.agents/**`); всё, что лежит в базовом теге, в патч не попадает, поэтому на настоящем upstream-исходнике WEB proxy и будущие правки «базовых» файлов молча исчезнут | открыто, P1 |
| M11 | Ghost-allowance: глобальный счётчик и TTL 5 с | `exteraless/ghost_mode.cpp` (`Allowance`) | счётчик не привязан к peer: чтение исключённого чата A взводит разрешение на 5 с, чтение чата B в этом окне его «съедает» | открыто, P1 (минимум — документировать) |
| M12 | `Config::load()` не вызывается | `exteraless/config.cpp` | мёртвый публичный API: всё делает ленивый `ensureLoaded()` | открыто, P2 |
| M13 | `hasVpnInterface()` считает `ppp*` VPN | `exteraless/network.cpp` | PPPoE/DSL распознаётся как VPN, прокси выключается без VPN | открыто, P2 |
| M14 | `ApplyProxyRules()` каждые 2 с | `exteraless/proxy_rules.cpp` | пока условие активно, пользователь не может вернуть прокси вручную; `proxyAutoDisabled` персистится и после перезапуска включает прокси сам | открыто, P2 |
| M15 | `IsGhostActive()` не учитывает offline-после-online | `exteraless/ghost_mode.cpp` | связано с M5 | открыто, P2 |

Отдельно — расхождения `docs/feature-mapping.md` с фактом, которые не вошли в
M-нумерацию:

- «Backup экспортирует 7 ключей»: в текущей редакции файла числа нет. На
  момент аудита `exteraless/backup.cpp` экспортировал 8 ключей; после NB-04
  он экспортирует все 25 desktop-ключей в трёх секциях.
- «Pill Stack и drawer» описаны как MVP2, но не сказано, что пилюль 8 из 15 и
  что drag-порядка нет; в `STATUS.md` этого тоже нет.
- `README.md` до сих пор держит пункт «Движок Python-плагинов с моделью
  разрешений» невыполненным, хотя stage 1–2 реализованы и покрыты раннерами.
- WEB proxy отсутствует и в `feature-mapping.md`, и в `README.md`, и в
  `STATUS.md`, хотя код и Nix-сборка его содержат.
- Утверждения docs о числе проверок (93 / 32 / 213) вторым отчётом
  подтверждены фактическим прогоном; первый отчёт числа не проверял.

## 10. Где два отчёта расходятся

| Пункт | Отчёт 1 | Отчёт 2 | Проверка в дереве | Вывод |
|---|---|---|---|---|
| Ghost-полнота | 5 из 5 ключей ✅, включая `OEGhostSendOfflineAfterOnline` | 4 из 5: offline-ключ мёртв | ключ встречается только в `exteraless/config.{h,cpp}` | 4 из 5, ключ мёртв |
| Deleted messages | ✅ реализовано (`OESaveDeletedMessages` + модуль) | P0: ключ не читается, запись всегда включена, нет лимита | гейт, лимит и ротация закрыты в NB-02; схема по-прежнему 9 полей и метки в чате нет | частично: P0 закрыт, NB-19/NB-20 открыты |
| `NaConfig` | 242 ключа | 267 `val` | 250 `addConfig` | 242–250 пользовательских настроек |
| `NekoConfig` | 97 | 98 `addConfig` | 98 `addConfig` | 98 |
| `AppearanceConfig` | 61 ключ в перечне | 74 | 74 `val` | 74 пункта, 61 «настройка» по перечню |
| `ChatsConfig` | 42 ключа в перечне | 50 | 50 `val` | 50 пунктов, 42 в перечне |
| `PillStackConfig` | 21 | не считал | 35 `val` | 21–35 в зависимости от счёта |
| `ExteraConfig` шим | ~170 геттеров | 152 геттера | ~150–170 | диапазон |
| Всего настроек Android | ≈400 | ≈600 уникальных | — | сотни; диапазон 400–600 |
| Файлы `app/exteraless` | 268 | не считал | 269 java + 12 kotlin | ≈280 |
| `unlimitedPinnedDialogs` | ❌ реализуемо | ⛔ лимит серверный | в 7.3.1 нет клиентского лимита пинов | ⛔ |
| `hideDividers` | ❌ (аналог `st::dividerPaint`) | ⛔ у строк нет разделителей | `dialogs/ui/dialogs_layout.cpp` рисует строки без линии | ⛔ |
| `sortByUnread` | P0 (средний риск) | P2 (высокий риск для индексных списков) | зависит от `dialogs/dialogs_indexed_list.cpp` | приоритет взят по первому отчёту (P0), риск — по второму (высокий); задача NB-15 |
| Переводчики | ➖/⛔ дубль upstream | P2: свои провайдеры нужны | upstream умеет только серверный перевод | серверный — ➖, провайдеры — P2 |
| Тесты | не запускались, числа из `STATUS.md` | 93 / 32 / 213 PASS | совпадает со `STATUS.md` | числа подтверждены |
| `result/`, `nix/smoke-test.sh`, база патча | не проверялись | M7/M8/M10 | `result/` старее дерева; скрипт указывает на каталог; тег локальный | замечания отчёта 2 действительны |
| WEB proxy | не рассматривался (desktop-only) | описан как реальная подсистема | 6 файлов + сокет, ~3.1k строк, строки в обоих бинарниках | подсистема существует, в docs отсутствует |

Практическое правило для бэклога: если отчёты расходятся в приоритете, берётся
более осторожная оценка, а расхождение указывается в строке задачи.

## 11. Что осталось непроверенным

- Android-клиент не собирался и не запускался: поведение взято из кода, строк и
  комментариев, а не из наблюдения.
- Первый отчёт не запускал desktop-раннеры; числа 93/32/213 взяты из
  `STATUS.md` и лишь вторым отчётом подтверждены прогоном.
- Сетевые подсистемы Android (переводчики, LLM, курсы, погода, транскрипция) не
  проверялись на живых API — это влияет на оценку P2.
- Часть ключей `NaConfig` с неоднозначными именами помечена «по ключу»: перед
  реализацией нужно читать соответствующий Java-путь.
- WEB proxy не проверялся в работе: нет развёрнутого relay-сервера, есть только
  клиентский код и планы тестирования.

## 12. Приложение A. Ключи desktop-конфига (25)

`OERelativeLastSeen`, `OEAddCommaAfterMention`, `OEGeneralDisableNumberRounding`,
`OEProxyDisableConditions`, `OEProxyAutoDisabled`, `OEGhostSendReadPackets`,
`OEGhostSendReadStories`, `OEGhostSendOnline`, `OEGhostSendUploadProgress`,
`OEGhostSendOfflineAfterOnline` (мёртвый), `OEGhostReadExclusions`,
`OEGhostTypingExclusions`, `OESaveDeletedMessages`,
`OEPillStackEnabled`, `OEPillStackPills`, `OEMainMenuOrder`,
`OEDrawerAccountsExpanded`, `OEPluginStates`, `OEChatsRemoveMessageTail`,
`OEChatsHideReactionsChannels`, `OEChatsHideReactionsGroups`,
`OEChatsHideReactionsPrivate`, `OEChatsStripTrackingOnOpen`,
`OEChatsStripTrackingOnPaste`, `OEChatsUnreadCapsule`.

Восемь из них служебные: `OEProxyAutoDisabled`, `OEDrawerAccountsExpanded`,
`OEGhostReadExclusions`, `OEGhostTypingExclusions`, `OEMainMenuOrder`,
`OEPillStackEnabled`, `OEPillStackPills`, `OEPluginStates`.

## 13. Приложение B. Ключи `.extera` (25)

Секция `exteraconfig` (21): `relativeLastSeen`, `addCommaAfterMention`,
`disableNumberRounding`, `doNotUseProxy`, `removeMessageTail`,
`hideReactionsInChannels`, `hideReactionsInGroups`,
`hideReactionsInPrivateChats`, `OEGhostSendReadPackets`,
`OEGhostSendReadStories`, `OEGhostSendOnline`, `OEGhostSendUploadProgress`,
`OEGhostSendOfflineAfterOnline`, `OEGhostReadExclusions`,
`OEGhostTypingExclusions`, `OESaveDeletedMessages`, `OEProxyAutoDisabled`,
`OEPluginStates`, `OEChatsStripTrackingOnOpen`,
`OEChatsStripTrackingOnPaste`, `OEChatsUnreadCapsule`.

Секция `pillstackconfig` (2): `OEPillStackEnabled`, `OEPillStackPills`.

Секция `mainconfig` (2): `OEMainMenuOrder`, `OEDrawerAccountsExpanded`.

Импорт проверяет тип и диапазон каждого ключа (proxy-маска 0–7, маска пилюль
0–255, допустимый `OEMainMenuOrder`), требует целые числа для масок,
санитизирует exclusions и plugin states и репортит остальное как неизвестное,
с префиксом секции для `pillstackconfig` и `mainconfig`. `OEProxyAutoDisabled`
экспортируется как снимок, но при импорте сбрасывается: это производное
сетевое состояние, и чужой файл не должен включать прокси. Импорт
`OEPluginStates` включает перечисленные plugin id, поэтому импортировать
стоит только доверенные файлы. Старое имя `hideReactionsInPrivate` попадает
в отчёт как неизвестное.

## 14. Приложение C. Куда ложится новая функция

| Назначение | Файл |
|---|---|
| Контекстное меню сообщения | `Telegram/SourceFiles/history/view/history_view_context_menu.cpp` |
| Меню чата и профиля | `Telegram/SourceFiles/window/window_peer_menu.cpp` |
| Время и «edited» под сообщением | `Telegram/SourceFiles/history/view/history_view_bottom_info.cpp` |
| Строка чата в списке | `Telegram/SourceFiles/dialogs/dialogs_row.cpp`, `dialogs/ui/dialogs_layout.cpp` |
| Профиль: телефон, ID, DC | `Telegram/SourceFiles/info/profile/info_profile_values.cpp`, `info_profile_top_bar.cpp` |
| Статусы online и last seen | `Telegram/SourceFiles/data/data_peer_values.cpp` |
| Реакции и unread-бар | `Telegram/SourceFiles/history/view/history_view_element.cpp`, `history_view_message.cpp` |
| Стикеры и медиа в сообщении | `Telegram/SourceFiles/history/view/media/history_view_sticker.cpp` |
| Пункты главного меню и аккаунты | `Telegram/SourceFiles/window/window_main_menu.cpp` |
| Настройки Exteraless | `Telegram/SourceFiles/settings/sections/settings_exteraless.cpp`, `Telegram/Resources/langs/lang.strings` |
| Конфиг и ключи | `exteraless/config.{h,cpp}` (ключ добавлять и в `fromJson`, и в список известных, иначе импорт сочтёт его неизвестным) |
| Бэкап | `exteraless/backup.cpp` |
| Пилюли | `Telegram/SourceFiles/exteraless/pill_stack.{h,cpp}` |
| Плагины | `exteraless/plugins/scanner.cpp`, `Telegram/SourceFiles/exteraless/{plugin_runtime,plugins_box}.{h,cpp}` |
| Тесты | `exteraless/tests/chats/`, `exteraless/tests/plugins/` |
| Прокси | `exteraless/proxy_rules.cpp`, `exteraless/network.cpp` |
| Удалённые сообщения | `exteraless/deleted_messages.{h,cpp}`, `boxes/deleted_messages_box.cpp` |
| WEB proxy | `Telegram/SourceFiles/mtproto/web_proxy/*`, `mtproto/details/mtproto_web_proxy_socket.*` |
