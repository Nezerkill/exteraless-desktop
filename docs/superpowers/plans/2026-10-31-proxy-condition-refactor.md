# Proxy Condition Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Заменить магические `1/2/4` условий прокси на общий `ProxyCondition` enum и helpers без изменения поведения.

**Architecture:** Тип и чистые constexpr-функции живут в `exteraless/proxy_rules.h`; `proxy_rules.cpp` и настройки используют их. Маска в JSON остаётся `int`.

**Tech Stack:** C++20, Qt, tdesktop 7.3.1.

**Spec:** `docs/superpowers/specs/2026-10-31-proxy-condition-refactor-design.md`

## Global Constraints

- Не менять формат `exteraless.json`/`.extera`.
- Не менять git-историю и не создавать коммит.
- Сборка с ограничением памяти: не более `-j2`.

## Review Focus

- Маска `0` означает «авто-отключение выключено».
- `ProxyCondition::None` не должен попадать в маску.
- Существующие значения VPN=1, Mobile=2, WiFi=4 должны сохраниться.

---

### Task 1: ProxyCondition API

**Files:**
- Modify: `exteraless/proxy_rules.h`

**Interfaces:**
- Produces: `enum class Exteraless::ProxyCondition`, `HasProxyCondition(int, ProxyCondition)`, `WithProxyCondition(int, ProxyCondition, bool)`.

- [ ] **Step 1: Добавить enum и helpers**

```cpp
enum class ProxyCondition : int {
	None = 0,
	VPN = 1,
	Mobile = 2,
	WiFi = 4,
};

[[nodiscard]] constexpr bool HasProxyCondition(
	int mask,
	ProxyCondition condition) {
	return (mask & int(condition)) != 0;
}

[[nodiscard]] constexpr int WithProxyCondition(
	int mask,
	ProxyCondition condition,
	bool enabled) {
	return enabled
		? (mask | int(condition))
		: (mask & ~int(condition));
}
```

- [ ] **Step 2: Добавить compile-time проверки**

```cpp
static_assert(int(ProxyCondition::VPN) == 1);
static_assert(int(ProxyCondition::Mobile) == 2);
static_assert(int(ProxyCondition::WiFi) == 4);
static_assert(HasProxyCondition(1, ProxyCondition::VPN));
static_assert(!HasProxyCondition(0, ProxyCondition::VPN));
static_assert(WithProxyCondition(0, ProxyCondition::WiFi, true) == 4);
static_assert(WithProxyCondition(4, ProxyCondition::WiFi, false) == 0);
```

### Task 2: Использовать API в proxy_rules

**Files:**
- Modify: `exteraless/proxy_rules.cpp`

- [ ] **Step 1: Заменить константы**

`ConditionFor(Transport)` возвращает `ProxyCondition`:
`VPN`, `Cellular -> Mobile`, `WiFi`, иначе `None`.

- [ ] **Step 2: Заменить проверку маски**

```cpp
const auto condition = ConditionFor(CurrentTransport());
const auto shouldDisable = (condition != ProxyCondition::None)
	&& HasProxyCondition(config.proxyDisableConditions(), condition);
```

### Task 3: Использовать API в настройках

**Files:**
- Modify: `Telegram/SourceFiles/settings/sections/settings_exteraless.cpp`

- [ ] **Step 1: Передавать enum вместо int**

Вызовы: `ProxyCondition::VPN`, `ProxyCondition::Mobile`,
`ProxyCondition::WiFi`.

- [ ] **Step 2: Обновлять маску helper-ом**

```cpp
const auto mask = Exteraless::WithProxyCondition(
	Exteraless::Config::Instance().proxyDisableConditions(),
	condition,
	checked);
```

### Task 4: Документация

**Files:**
- Create: `Agents/README.md`
- Create: `CHANGELOG.md`

- [ ] **Step 1: Описать карту проекта, модули и сборку в `Agents/README.md`.**
- [ ] **Step 2: Добавить запись о рефакторинге и краткую историю M0–M2 в `CHANGELOG.md`.**

### Task 5: Проверки

- [ ] **Step 1: Собрать `exteraless`:**

```bash
nix develop --command cmake --build out --target exteraless -j2
```

- [ ] **Step 2: Скомпилировать настройки:**

```bash
nix develop --command ninja -C out \
  Telegram/CMakeFiles/Telegram.dir/SourceFiles/settings/sections/settings_exteraless.cpp.o
```

- [ ] **Step 3: Проверить flake:**

```bash
nix flake check --no-build
```
