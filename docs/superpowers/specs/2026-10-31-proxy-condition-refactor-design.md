# Proxy Condition Refactor Design

Дата: 2026-10-31.

## Задача

Убрать дублирование магических чисел `1/2/4` для условий авто-отключения
прокси (VPN / mobile / Wi-Fi) в `exteraless/proxy_rules.cpp` и
`Telegram/SourceFiles/settings/sections/settings_exteraless.cpp`.

## Границы

- Поведение не меняется.
- Новые условия пока не добавляются.
- Формат `exteraless.json` и `.extera` не меняется: маска остаётся `int`.
- Git-история не меняется, коммит не создаётся.

## Подход

В `exteraless/proxy_rules.h`:

- `enum class ProxyCondition : int { None = 0, VPN = 1, Mobile = 2, WiFi = 4 }`;
- `constexpr bool HasProxyCondition(int mask, ProxyCondition condition)`;
- `constexpr int WithProxyCondition(int mask, ProxyCondition condition, bool enabled)`;
- compile-time `static_assert` для значений и helpers.

В `proxy_rules.cpp`:

- `ConditionFor(Transport)` возвращает `ProxyCondition`;
- проверка маски через `HasProxyCondition`.

В `settings_exteraless.cpp`:

- вместо `1/2/4` используются `ProxyCondition::VPN/Mobile/WiFi`;
- обновление маски через `WithProxyCondition`.

## Документация

- `Agents/README.md` — карта проекта, модулей и правил сборки.
- `CHANGELOG.md` — запись об этом рефакторинге и краткая история M0–M2.

## Проверки

- `cmake --build out --target exteraless -j2`;
- компиляция `settings_exteraless.cpp.o`;
- `nix flake check --no-build`.
