# Drawer Accounts & Unread Badges Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Показать аккаунты в drawer (главном меню) даже при одном аккаунте, включить unread-бейдж активного аккаунта и сохранять состояние секции в `OEDrawerAccountsExpanded`.

**Status:** Task 1–3 completed, verified on final binary.

**Architecture:** Расширяем существующие `Settings::SetupAccounts` / `AccountsList` / `MakeAccountButton` опциями вместо нового drawer. Главное меню передаёт `showSingle` и `badgeActive`, хранит раскрытие в `rpl::variable`, пишет его в `Exteraless::Config`; раздел Information продолжает использовать дефолтные опции.

**Tech Stack:** C++20, Qt, tdesktop 7.3.1, rpl, Ninja/CMake, Xvfb.

**Spec:** `STATUS.md` (MVP2, незакрытый пункт `Drawer: аккаунты, unread-бейджи`); `docs/feature-mapping.md` (раздел Drawer). Исследование подтвердило: main menu tdesktop уже является drawer, `Settings::SetupAccounts` умеет аккаунты и бейджи, но при одном аккаунте строка скрыта, а бейдж активного отключён.

## Global Constraints

- Правки production-кода только в: `Telegram/SourceFiles/settings/sections/settings_information.{h,cpp}`, `Telegram/SourceFiles/window/window_main_menu.{h,cpp}`, `exteraless/config.{h,cpp}`. Документы — только в Task 3.
- Не создавать overlay drawer, не дублировать `AccountsList`, не менять порядок аккаунтов, логин, логаут и account switcher.
- Итоговый `AccountsOptions { showSingle = false, badgeActive = false }` по умолчанию сохраняет поведение Settings Information; поле `badgeActive` добавляется только в Task 2.
- Ключ конфига ровно `OEDrawerAccountsExpanded`, значение по умолчанию `true`; ключ обязателен в `Config::fromJson` whitelist.
- Upstream `mainMenuAccountsShown` пишут главное меню (`MainMenu::toggleAccounts` при клике по chevron) и `Main::Domain::add` при появлении второго аккаунта; Settings Information его не читает и не пишет. Фактический источник состояния drawer — `OEDrawerAccountsExpanded`; upstream auto-expand и синхронизация уже открытых главных меню в drawer пока не отражаются.
- Использовать существующие `session->data().unreadWithMentionsBadge()`, `unreadWithMentionsBadgeMuted()`, `Settings::Badge`; короткий формат даёт `Lang::FormatCountToShort` внутри `Badge::CreateUnread`.
- Не добавлять debug-код, `#ifdef _DEBUG`, тестовые seam'ы и новые зависимости.
- Только Debug-сборка; параллелизм не выше `-j2` (16 ГБ без свапа).
- Не выполнять `git commit`, `git push`, `git reset`, `git rebase`; `git add` допустим только как `git add -N` внутри `nix/update-patch.sh`.
- Текстовые файлы — LF, UTF-8 без BOM. Комментарии — максимум одна строка и только «почему».
- Smoke-тесты запускать только с изолированными `-workdir` и `XDG_DATA_HOME`; реальный профиль `~/.local/share/ExteralessDesktop` не трогать, останавливать только записанный PID.

## Review Focus

- Один аккаунт без unread: строка видна, бейджа нет (не пустая плашка). Тест — Task 2, Steps 9–10.
- Один аккаунт с unread: короткий бейдж непустой и не полный счётчик. Тест — Task 2, Step 9.
- Старый профиль с `mainMenuAccountsShown=false` и без нового ключа: drawer раскрыт по default `true`. Тест — Task 2, Step 9 (в Task 1 секция ещё стартует свёрнутой из-за pre-existing `ToggleAccountsButton`).
- Collapse/expand и перезапуск: `OEDrawerAccountsExpanded` пишется `false`/`true` и восстанавливается. Тест — Task 2, Step 9.
- Settings Information при одном аккаунте: строка по-прежнему скрыта, активный бейдж выключен. Тест — Task 1, Step 9.

## Smoke Harness

Общий harness для Task 1–2. Профиль `/tmp/opencode/smoke-drawer5/tdata` содержит один залогиненный тестовый аккаунт.

```bash
export SMOKE=/tmp/opencode/smoke-drawer-accounts
export DISPLAY=:99
rm -rf "$SMOKE" "$SMOKE-cache"
mkdir -p "$SMOKE" "$SMOKE-cache"
cp -a /tmp/opencode/smoke-drawer5/tdata "$SMOKE/tdata"
pgrep -f "Xvfb :99" >/dev/null || \
  (Xvfb :99 -screen 0 1280x800x24 >/tmp/opencode/xvfb99-accounts.log 2>&1 &)
```

Запуск, открытие drawer и скриншот:

```bash
XDG_DATA_HOME="$SMOKE" XDG_CACHE_HOME="$SMOKE-cache" \
  QT_QPA_PLATFORM=xcb out/bin/Telegram -workdir "$SMOKE" -noupdate \
  >"$SMOKE/app.log" 2>&1 &
echo $! > "$SMOKE/app.pid"
sleep 12
python3 /tmp/opencode/xinput.py click 45 65
sleep 1
import -window root /tmp/opencode/accounts-menu.png
```

Остановка и повторный запуск:

```bash
kill "$(cat "$SMOKE/app.pid")" 2>/dev/null || true
sleep 2
# затем повторить блок запуска выше
```

Конфиг: `$SMOKE/ExteralessDesktop/tdata/exteraless.json`. Читать ключ:

```bash
python3 - <<'EOF'
import json, os
path = os.path.join(os.environ["SMOKE"], "ExteralessDesktop/tdata/exteraless.json")
data = json.load(open(path)).get("exteraconfig", {})
print(data.get("OEDrawerAccountsExpanded", "<absent>"))
EOF
```

Координаты: аватар `(45,65)`, chevron аккаунтов `(245,127)`, строка Settings в раскрытом меню ниже строки аккаунта. Если окно смещено, скорректировать по скриншоту.

---

### Task 1: Опции аккаунтов и строка при одном аккаунте

**Files:**
- Modify: `Telegram/SourceFiles/settings/sections/settings_information.h`
- Modify: `Telegram/SourceFiles/settings/sections/settings_information.cpp`
- Modify: `Telegram/SourceFiles/window/window_main_menu.cpp`

**Interfaces:**
- Produces: `Settings::AccountsOptions`; `SetupAccounts(container, controller, AccountsOptions options = {})`.
- Consumes: существующие `AccountsList`, `Window::SessionController`, `MakeAccountButton`.

- [ ] **Step 1: Зафиксировать базовую линию**

```bash
git rev-parse HEAD
git status --short
```

Ожидается: дерево чистое; хеш HEAD запомнить для финальной проверки.

- [ ] **Step 2: RED — один аккаунт не виден**

Выполнить блоки `Smoke Harness`: setup, запуск, открытие меню.

Ожидается на `/tmp/opencode/accounts-menu.png`: строки аккаунта нет, первым пунктом под именем идёт `My Profile`.

Затем открыть Settings (строка в меню), кликнуть по шапке профиля (открывается Information / Edit profile) и сохранить `/tmp/opencode/accounts-t1-red-info.png`.

Ожидается: в Information строки аккаунта тоже нет. Остановить приложение.

- [ ] **Step 3: Добавить `AccountsOptions` в заголовок**

В `settings/sections/settings_information.h`, в `namespace Settings`, перед `AccountsEvents`:

```cpp
struct AccountsOptions {
	bool showSingle = false;
};
```

Поле `badgeActive` здесь не добавлять: оно появляется вместе с его использованием в Task 2, Step 3.

Изменить сигнатуру:

```cpp
AccountsEvents SetupAccounts(
	not_null<Ui::VerticalLayout*> container,
	not_null<Window::SessionController*> controller,
	AccountsOptions options = {});
```

- [ ] **Step 4: Пробросить опции в `AccountsList`**

В `settings_information.cpp` конструктор принимает `AccountsOptions options`, член `_options` объявить после `_controller`, инициализировать в списке инициализации:

```cpp
	const not_null<Window::SessionController*> _controller;
	const AccountsOptions _options;
	const not_null<Ui::VerticalLayout*> _outer;
```

```cpp
AccountsList::AccountsList(
	not_null<Ui::VerticalLayout*> container,
	not_null<Window::SessionController*> controller,
	AccountsOptions options)
: _controller(controller)
, _options(options)
, _outer(container)
, _outerIndex(container->count()) {
```

- [ ] **Step 5: Показывать единственный аккаунт по опции**

В `AccountsList::rebuild()` заменить условие скрытия на:

```cpp
if (!account->sessionExists()
	|| (list.size() == 1 && !_options.showSingle)) {
	button = nullptr;
}
```

- [ ] **Step 6: Включить опцию в главном меню**

В `window/window_main_menu.cpp`, `MainMenu::setupAccounts()`, передать опцию (пока только `showSingle`):

```cpp
auto events = Settings::SetupAccounts(
	inner,
	_controller,
	{ .showSingle = true });
```

`Settings::SetupAccounts` в `SetupAccountsWrap` оставить без третьего аргумента — дефолт сохраняет прежнее поведение Information.

- [ ] **Step 7: Скомпилировать затронутые единицы**

```bash
nix develop --command ninja -C out \
  Telegram/CMakeFiles/Telegram.dir/SourceFiles/settings/sections/settings_information.cpp.o \
  Telegram/CMakeFiles/Telegram.dir/SourceFiles/window/window_main_menu.cpp.o
```

Ожидается: exit 0.

- [ ] **Step 8: Перелинковать**

```bash
nix develop --command cmake --build out --target Telegram -j2
```

Ожидается: exit 0. Если линковщик убит по памяти, выполнить fallback:

```bash
nix develop --command bash -c 'cd out && g++ -g -Wl,-S -pthread -Wl,--as-needed -Wl,--dependency-file=Telegram/CMakeFiles/Telegram.dir/link.d @CMakeFiles/Telegram.rsp -o bin/Telegram'
```

- [ ] **Step 9: GREEN — строка видна в drawer после раскрытия**

Повторить setup и запуск. Открыть меню.

До Task 2 секция стартует свёрнутой: существующий `ToggleAccountsButton` в конструкторе пишет `settings.setMainMenuAccountsShown(false)` при `accounts().size() < 2`, поэтому в первом кадре строки аккаунта не видно. Это ожидаемое pre-existing поведение, а не регрессия Task 1. Кликнуть chevron `(245,127)`, дождаться анимации и сохранить `/tmp/opencode/accounts-t1-green-menu.png` — скриншот именно в раскрытом состоянии.

Ожидается: под именем появилась строка единственного аккаунта, `My Profile` сдвинулся вниз; бейджа на строке пока нет.

Открыть Settings → шапку профиля, сохранить `/tmp/opencode/accounts-t1-green-info.png`.

Ожидается: в Information строки аккаунта нет. Остановить приложение.

- [ ] **Step 10: Проверить границы изменений**

```bash
git diff --stat
```

Ожидается ровно три файла Task 1; `settings_information.h`, `settings_information.cpp`, `window/window_main_menu.cpp`.

### Task 2: Активный unread-бейдж и сохранение раскрытия

**Files:**
- Modify: `Telegram/SourceFiles/settings/sections/settings_information.h`
- Modify: `Telegram/SourceFiles/settings/sections/settings_information.cpp`
- Modify: `Telegram/SourceFiles/window/window_main_menu.h`
- Modify: `Telegram/SourceFiles/window/window_main_menu.cpp`
- Modify: `exteraless/config.h`
- Modify: `exteraless/config.cpp`

**Interfaces:**
- Consumes: `AccountsOptions` и `SetupAccounts(..., options)` из Task 1.
- Produces: `bool badgeActive = false` в `AccountsOptions`; `Config::drawerAccountsExpanded()` / `setDrawerAccountsExpanded(bool)`; `bool hasUnread` в `MakeAccountButton`; `MainMenu::_accountsExpanded`.

**Перед началом: проверить build state Task 1.**

Правки Task 2 начинать только после того, как полная сборка Task 1 (`Task 1, Step 8`) действительно завершилась успешно и Ninja state консистентен — иначе следующий Ninja начнёт с середины прошлого и пересоберёт дерево целиком.

```bash
pgrep -fa "ninja|cc1plus" || echo "no build running"
nix develop --command ninja -C out -n Telegram | tail -3
```

Ожидается: процессов сборки нет; dry-run печатает `ninja: no work to do.` Если dry-run планирует массовую пересборку или прошлая сборка была прервана — сначала дожать `Task 1, Step 8` до exit 0, и только потом менять код.

Никогда не запускать два Ninja/CMake-билда в `out` одновременно: параллельные процессы портят `.ninja_log` и deps и оставляют build state, требующий полной пересборки.

- [ ] **Step 1: RED — бейджа нет, раскрытие не переживает restart**

На бинарнике Task 1 выполнить setup и запуск, открыть меню.

Ожидается: секция свёрнута (pre-existing `ToggleAccountsButton` ставит `mainMenuAccountsShown=false` при одном аккаунте), строки аккаунта не видно. Кликнуть chevron `(245,127)` — строка появляется, бейджа на ней нет. Кликнуть chevron ещё раз — строка скрывается; в `exteraless.json` ключа `OEDrawerAccountsExpanded` нет (или файла нет). Перезапустить приложение и открыть меню.

Ожидается: строка снова скрыта, потому что старый код принудительно ставит `mainMenuAccountsShown=false` при одном аккаунте. Сохранить `/tmp/opencode/accounts-t2-red-restart.png`. Остановить приложение.

- [ ] **Step 2: Добавить ключ и API конфига**

В `exteraless/config.cpp` рядом с остальными ключами:

```cpp
constexpr auto kDrawerAccountsExpanded = "OEDrawerAccountsExpanded";
```

В `exteraless/config.h`, после `mainMenuOrder`:

```cpp
[[nodiscard]] bool drawerAccountsExpanded() const;
void setDrawerAccountsExpanded(bool value);
```

В `exteraless/config.cpp`:

```cpp
bool Config::drawerAccountsExpanded() const {
	return boolValue(kDrawerAccountsExpanded, true);
}

void Config::setDrawerAccountsExpanded(bool value) {
	setValue(kDrawerAccountsExpanded, value);
}
```

В `Config::fromJson` whitelist добавить `|| key == kDrawerAccountsExpanded`.

- [ ] **Step 3: Добавить `badgeActive` и передавать `hasUnread` в `MakeAccountButton`**

В `settings/sections/settings_information.h` расширить `AccountsOptions` вторым полем:

```cpp
struct AccountsOptions {
	bool showSingle = false;
	bool badgeActive = false;
};
```

В `settings_information.cpp` добавить параметр после `bool locked`:

```cpp
	Fn<void(Qt::KeyboardModifiers)> callback,
	bool locked,
	bool hasUnread) {
```

В `ComposedBadge` передавать `hasUnread` вместо `!active`.

В `AccountsList::rebuild()` перед вызовом вычислить и передать:

```cpp
const auto active = (account == &_controller->session().account());
const auto hasUnread = _options.badgeActive || !active;
button.reset(inner->add(MakeAccountButton(
	inner,
	_controller,
	account,
	std::move(callback),
	nextIsLocked,
	hasUnread)));
```

- [ ] **Step 4: Добавить состояние раскрытия в `MainMenu`**

Сначала прочитать `window/window_main_menu.h` и `window/window_main_menu.cpp` и привести сниппеты ниже к фактическому коду: `ToggleAccountsButton` объявлен внутри `MainMenu` только forward-декларацией в заголовке и определён в `.cpp`, поэтому его сигнатура меняется в `.cpp`, а в заголовок добавляется только член. Сверить с кодом имена `_accounts`, `_accountsExpanded`, `setupAccounts()`, `setupAccountsToggle()`, `toggleAccounts()` и текущий порядок инициализации (`_userpicButton` идёт перед `_toggleAccounts`).

`_toggleAccounts` принимает `rpl::producer<bool>`, а не `bool`: текущее значение — `_accountsExpanded.current()`, поток — `_accountsExpanded.value()`. Не передавать `bool` туда, где требуется producer (и наоборот); если реальная сигнатура в коде отличается от плана — следовать коду, а не плану.

В `window/window_main_menu.h` объявить `rpl::variable<bool> _accountsExpanded;` до `_toggleAccounts`. В `window/window_main_menu.cpp` изменить конструктор `ToggleAccountsButton`:

```cpp
	ToggleAccountsButton(
		QWidget *parent,
		not_null<Main::Account*> current,
		rpl::producer<bool> expanded);
```

- [ ] **Step 5: Переключить `ToggleAccountsButton` на новый producer**

В определении конструктора `ToggleAccountsButton` удалить блок, который принудительно пишет `settings.setMainMenuAccountsShown(false)` при `< 2` аккаунтах. Заменить подписку на `settings.mainMenuAccountsShownValue()` на переданный `expanded` (локальную `auto &settings = Core::App().settings();` удалить, если она больше не используется):

```cpp
	std::move(expanded) | rpl::filter([=](bool value) {
		return (_toggled != value);
	}) | rpl::on_next([=](bool value) {
		_toggled = value;
		_toggledAnimation.start(
			[=] { update(); },
			_toggled ? 0. : 1.,
			_toggled ? 1. : 0.,
			st::slideWrapDuration);
		validateUnreadBadge();
	}, lifetime());
```

- [ ] **Step 6: Инициализировать, переключать и применять раскрытие**

В списке инициализации `MainMenu::MainMenu` между `_userpicButton` и `_toggleAccounts`:

```cpp
, _accountsExpanded(
	Exteraless::Config::Instance().drawerAccountsExpanded())
, _toggleAccounts(
	this,
	&controller->session().account(),
	_accountsExpanded.value())
```

`MainMenu::toggleAccounts()`:

```cpp
void MainMenu::toggleAccounts() {
	const auto shown = !_accountsExpanded.current();
	_accountsExpanded = shown;
	Exteraless::Config::Instance().setDrawerAccountsExpanded(shown);
	Core::App().settings().setMainMenuAccountsShown(shown);
	Core::App().saveSettingsDelayed();
}
```

`MainMenu::setupAccounts()`: `_accounts->toggleOn(_accountsExpanded.value());` вместо `mainMenuAccountsShownValue()`, и передать обе опции:

```cpp
auto events = Settings::SetupAccounts(
	inner,
	_controller,
	{ .showSingle = true, .badgeActive = true });
```

`_accounts->finishAnimating();` оставить.

- [ ] **Step 7: Скомпилировать**

```bash
nix develop --command ninja -C out \
  Telegram/CMakeFiles/Telegram.dir/SourceFiles/settings/sections/settings_information.cpp.o \
  Telegram/CMakeFiles/Telegram.dir/SourceFiles/window/window_main_menu.cpp.o \
  exteraless/libexteraless.a
```

Ожидается: exit 0.

- [ ] **Step 8: Перелинковать**

```bash
nix develop --command cmake --build out --target Telegram -j2
```

Ожидается: exit 0; при OOM использовать fallback из Task 1, Step 8.

- [ ] **Step 9: GREEN — бейдж и persistence**

Выполнить setup, запуск, открыть меню. Сохранить `/tmp/opencode/accounts-t2-green-menu.png`.

Ожидается: строка аккаунта видна, на ней непустой короткий бейдж; число соответствует unread активного аккаунта (не полный длинный счётчик). Сверить с unread-счётчиками в списке чатов.

Проверить отсутствие ключа до первого клика — при чистом профиле `OEDrawerAccountsExpanded` отсутствует, но меню раскрыто (default `true`).

Кликнуть chevron `(245,127)`, сохранить `/tmp/opencode/accounts-t2-green-collapsed.png`.

Ожидается: строка скрыта; в `exteraless.json`:

```bash
python3 - <<'EOF'
import json, os
path = os.path.join(os.environ["SMOKE"], "ExteralessDesktop/tdata/exteraless.json")
print(json.load(open(path))["exteraconfig"]["OEDrawerAccountsExpanded"])
EOF
```

выводит `False`.

Остановить и снова запустить приложение, открыть меню.

Ожидается: строка скрыта (состояние восстановлено).

Кликнуть chevron, проверить, что ключ стал `True`, строка и бейдж вернулись. Остановить и снова запустить, открыть меню.

Ожидается: строка и бейдж на месте после restart.

- [ ] **Step 10: Проверить, что нулевой unread не рисует плашку**

```bash
rg -n -A3 "if \(!state->count\)" \
  Telegram/SourceFiles/settings/sections/settings_information.cpp
```

Ожидается: `Badge::CreateUnread` по-прежнему скрывает виджет при `count == 0`; новый код только передаёт `hasUnread` и не обходит этот helper.

- [ ] **Step 11: Проверить wiring конфига**

```bash
rg -n "kDrawerAccountsExpanded" exteraless/config.cpp
```

Ожидается: объявление ключа, getter, setter и строка whitelist в `fromJson` (четыре вхождения).

- [ ] **Step 12: Проверить границы изменений**

```bash
git diff --stat
```

Ожидается: файлы Task 1 и Task 2, без новых файлов.

### Task 3: Документация, патч и финальная проверка

**Files:**
- Modify: `STATUS.md`
- Modify: `CHANGELOG.md`
- Modify: `docs/feature-mapping.md`
- Modify: `Agents/README.md`
- Modify: `nix/patches/exteraless.patch` (генерируется)

**Interfaces:**
- Consumes: поведение и ключ `OEDrawerAccountsExpanded` из Task 1–2.
- Produces: актуальные STATUS/CHANGELOG/карта, обновлённый Nix-патч.

- [ ] **Step 1: Обновить `STATUS.md`**

Отметить `[x] Drawer: аккаунты и unread-бейджи` и добавить в «Проверки» строку: под Xvfb один аккаунт — строка видна, короткий бейдж активного соответствует unread, collapse/expand сохраняется в `exteraless.json` и после restart, Information при одном аккаунте строку не показывает.

- [ ] **Step 2: Обновить `CHANGELOG.md`**

В раздел `## 2026-11-03` добавить пункт: drawer показывает единственный аккаунт, активный аккаунт получает короткий unread-бейдж, раскрытие хранится в `OEDrawerAccountsExpanded` (default `true`), Information сохраняет прежнее поведение.

- [ ] **Step 3: Обновить карту проекта**

В `docs/feature-mapping.md`, раздел `### Drawer`: заменить описание overlay-drawer на фактическое — main menu tdesktop, аккаунты и unread-бейдж активного, один аккаунт виден, секция сворачивается через `OEDrawerAccountsExpanded`, порядок/логин/логаут остаются upstream.

В `Agents/README.md`, после пункта про `OEMainMenuOrder`, добавить: один аккаунт отображается в drawer, активный бейдж использует `unreadWithMentionsBadge`, состояние секции — `OEDrawerAccountsExpanded` (default `true`) как фактический источник drawer; `mainMenuAccountsShown` пишут главное меню и `Main::Domain::add` при втором аккаунте, а upstream auto-expand и синхронизация уже открытых главных меню в drawer пока не отражаются.

- [ ] **Step 4: Обновить и проверить Nix-патч**

```bash
./nix/update-patch.sh
rm -rf /tmp/opencode/tdesktop-clean-accounts
mkdir -p /tmp/opencode/tdesktop-clean-accounts
git archive v7.3.1 | tar -x -C /tmp/opencode/tdesktop-clean-accounts
patch -p1 --dry-run -d /tmp/opencode/tdesktop-clean-accounts \
  < nix/patches/exteraless.patch
```

Ожидается: `checking file ...` для изменённых файлов, exit 0. Скрипт делает `git add -N` только для `exteraless` и `Telegram`; коммит не создаётся.

- [ ] **Step 5: Проверить flake**

```bash
nix flake check --no-build
```

Ожидается: exit 0.

- [ ] **Step 6: Финальная сборка и линковка**

```bash
nix develop --command cmake --build out --target Telegram -j2
```

Ожидается: exit 0; при OOM использовать fallback из Task 1, Step 8.

- [ ] **Step 7: Повторить smoke-сценарий**

Прогнать Task 2, Step 9 на финальном бинарнике. Он покрывает и сценарий Task 1, Step 9, но с той разницей, что после Task 2 секция стартует раскрытой (свёрнутый старт был только до Task 2), поэтому строка и бейдж видны сразу, без клика по chevron. Ожидается: строка и бейдж в drawer, Information без строки, collapse/expand переживает restart.

- [ ] **Step 8: Финальная проверка состояния**

```bash
git rev-parse HEAD
git status --short
git diff --stat
git diff --check
```

Ожидается: HEAD совпадает с хешем из Task 1, Step 1; изменены ровно 11 файлов:

```text
Agents/README.md
CHANGELOG.md
STATUS.md
Telegram/SourceFiles/settings/sections/settings_information.cpp
Telegram/SourceFiles/settings/sections/settings_information.h
Telegram/SourceFiles/window/window_main_menu.cpp
Telegram/SourceFiles/window/window_main_menu.h
docs/feature-mapping.md
exteraless/config.cpp
exteraless/config.h
nix/patches/exteraless.patch
```

План `docs/superpowers/plans/2026-11-03-drawer-accounts-unread.md` остаётся untracked и в diff не входит: в `git status --short` он виден отдельной строкой

```text
?? docs/superpowers/plans/2026-11-03-drawer-accounts-unread.md
```

а `git diff --stat` его не показывает. `nix/update-patch.sh` его тоже не добавляет — `git add -N` там только для `exteraless` и `Telegram`.

`git diff --check` без вывода; новых tracked-файлов и коммитов нет — единственный untracked файл — этот план.
