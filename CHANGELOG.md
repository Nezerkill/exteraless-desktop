# Changelog

## 2026-11-04

- Drawer-аккаунты подтверждены на итоговом бинарнике: Debug-сборка и Xvfb
  smoke прошли, `nix flake check --no-build` проходит.

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
