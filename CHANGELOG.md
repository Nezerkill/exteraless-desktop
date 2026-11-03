# Changelog

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
