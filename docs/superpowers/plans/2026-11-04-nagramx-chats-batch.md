# Nagram X Chats Batch Implementation Plan

**Status:** Completed on 2026-11-04. The batch is implemented, its runner is
green (`exteraless/tests/chats/run_checks.py`, now 93 checks), the Xvfb smoke
is recorded and `nix/patches/exteraless.patch` applies to a clean v7.3.1.

**Follow-up:** paste-time stripping (`OEChatsStripTrackingOnPaste`) was
implemented in a later task, so the original “paste is untouched” notes below
describe the batch scope, not the current tree.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Перенести в Exteraless Desktop пять настроек Nagram X для чатов: убрать хвост пузыря, скрыть реакции в каналах/группах/личных чатах и вырезать известные tracking-параметры из http/https-ссылок при открытии.

**Architecture:** Новые чистые помощники живут в статической библиотеке `exteraless`: `chats.{h,cpp}` хранит снимок настроек и правило скрытия реакций, `links.{h,cpp}` — строковую очистку URL без пересборки `QUrl`. `Config` кэширует пять boolean-значений, чтобы горячие пути отрисовки не читали JSON. UI-настройки добавляют подсекцию Chats, отрисовка сообщений и reactions list читают кэш, а `UiIntegration::handleUrlClick()` очищает ссылку до существующей обработки web-auth токенов.

**Tech Stack:** C++20, Qt 6 Core (`QString`, `QStringList`, `QJsonObject`), tdesktop 7.3.1, CMake/Ninja, Xvfb, Python 3 stdlib для regression runner'а.

**Spec:** `docs/feature-mapping.md`, раздел `### Settings` → `Chats`; постановка батча Nagram X от 2026-11-04.

## Global Constraints

- Production-правки только в: `exteraless/chats.{h,cpp}`, `exteraless/links.{h,cpp}`, `exteraless/config.{h,cpp}`, `exteraless/backup.cpp`, `exteraless/CMakeLists.txt`, `Telegram/SourceFiles/history/view/history_view_message.cpp`, `Telegram/SourceFiles/history/view/history_view_element.cpp`, `Telegram/SourceFiles/core/ui_integration.cpp`, `Telegram/SourceFiles/settings/sections/settings_exteraless.cpp`.
- Тесты только в: `exteraless/tests/chats/` (CMake, CLI, runner, fixtures).
- Документы: `docs/feature-mapping.md`, `CHANGELOG.md`; сгенерированный патч: `nix/patches/exteraless.patch`.
- Ключи и значения по умолчанию ровно такие: `OEChatsRemoveMessageTail=true`, `OEChatsHideReactionsChannels=false`, `OEChatsHideReactionsGroups=false`, `OEChatsHideReactionsPrivate=false`, `OEChatsStripTrackingOnOpen=true`.
- Android-ключи backup ровно такие: `removeMessageTail`, `hideReactionsInChannels`, `hideReactionsInGroups`, `hideReactionsInPrivateChats`; `OEChatsStripTrackingOnOpen` в Android backup отсутствует и не экспортируется.
- `HiddenReactionsFor(const ChatsSettings&, bool, bool, bool)` и `StripTracking(const QString&)` — чистые функции без UI, без I/O и без чтения `Config`. Удобный overload `HiddenReactionsFor(bool, bool, bool)` читает только кэш `Config::chatsSettings()`.
- Горячие пути (`Message::countMessageRounding`, `Element::refreshReactions`) читают только кэш `Config::chatsSettings()`, без `QJsonObject` lookup и без файловых операций.
- `StripTracking` трогает только `http://` и `https://` (case-insensitive по схеме), сохраняет исходный регистр, percent-encoding и fragment. Параметры `ref`, `source`, `s`, `si` не удаляются ни при каких условиях. Percent-encoded имена параметров не декодируются.
- Никаких `lib_ui`-правок, `lang.strings`, новых зависимостей, `#ifdef _DEBUG`, debug-полей и тестовых seam'ов в production-коде.
- UI-подписи — inline English literals, как в существующей `settings_exteraless.cpp`; новые размеры и отступы не хардкодятся.
- Только Debug-сборка; Release не собирать. Параллелизм сборки не выше `-j2`.
- Текстовые файлы — LF, UTF-8 без BOM. Комментарии — максимум одна строка и только «почему».
- Не выполнять `git commit`, `git push`, `git reset`, `git rebase`; `git add -N` допустим только внутри `nix/update-patch.sh`.
- Перед первой правкой сохранить baseline: `git status --short > /tmp/opencode/nagramx-chats-baseline.txt`. В рабочем дереве уже может быть незавершённый WIP других задач; в отчёте перечислять только пути батча, а не присваивать предсуществующие изменения.
- Smoke запускать только с изолированными `-workdir`, `XDG_DATA_HOME`, `XDG_CACHE_HOME`; реальный профиль `~/.local/share/ExteralessDesktop` не трогать; останавливать только записанный PID.
- Изменения настроек применяются после закрытия и повторного открытия чата (или перезапуска). Это ограничение документируется, а не маскируется live-refresh.

## File Structure

| Путь | Действие | Ответственность |
|---|---|---|
| `exteraless/chats.h` | создать | `ChatsSettings`, два overload'а `HiddenReactionsFor` |
| `exteraless/chats.cpp` | создать | правило скрытия реакций; convenience-overload читает `Config` |
| `exteraless/links.h` | создать | `StripTracking(const QString&)` |
| `exteraless/links.cpp` | создать | строковая очистка query с сохранением encoding/fragment |
| `exteraless/config.h` | изменить | `chatsSettings()`, пять setter'ов, кэш `_chats` |
| `exteraless/config.cpp` | изменить | пять ключей, `refreshChats()`, whitelist `fromJson` |
| `exteraless/backup.cpp` | изменить | Android-ключи export/import |
| `exteraless/CMakeLists.txt` | изменить | новые TUs и `tests/chats` |
| `exteraless/tests/chats/CMakeLists.txt` | создать | CLI `exteraless-chats-cli` |
| `exteraless/tests/chats/chats_cli.cpp` | создать | режимы `strip` и `hidden` |
| `exteraless/tests/chats/run_checks.py` | создать | fixtures + negative argument checks |
| `exteraless/tests/chats/fixtures/strip.tsv` | создать | URL-кейсы |
| `exteraless/tests/chats/fixtures/reactions.tsv` | создать | true/false-матрица |
| `Telegram/SourceFiles/settings/sections/settings_exteraless.cpp` | изменить | подсекция Chats и пять тумблеров |
| `Telegram/SourceFiles/history/view/history_view_message.cpp` | изменить | хвост пузыря |
| `Telegram/SourceFiles/history/view/history_view_element.cpp` | изменить | скрытие reactions list |
| `Telegram/SourceFiles/core/ui_integration.cpp` | изменить | очистка URL при клике |
| `docs/feature-mapping.md` | изменить | Chats keys + disclaimer |
| `CHANGELOG.md` | изменить | запись батча |
| `nix/patches/exteraless.patch` | regenerate | итоговый diff для Nix-пакета |

## Review Focus

- `?si=`, `?ref=`, `?source=`, `?s=` не удаляются: строка URL остаётся байт-в-байт прежней (fixtures `strip.tsv`).
- Не-http схемы (`tg://`, `tonsite://`, `mailto:`, `ftp://`) не меняются (fixtures `strip.tsv`).
- Percent-encoding, плюсы и fragment сохраняются; меняется только query до `#` (fixtures `strip.tsv`).
- Классификация peer'а: broadcast channel, megagroup/basic group и user не путаются; скрытие срабатывает только для включённой категории (fixtures `reactions.tsv`).
- Удаление хвоста не ломает attached bubbles и right layout: пропадает только несоединённый нижний угол (Xvfb-снимки Task 4).

## Known Risks

- Точные Android-ключи backup могут отличаться в конкретной версии exteraGram. Если исходник Android подтвердит другое имя, менять только `backup.cpp`, не `OE*`-ключи; расхождение проявится как unknown key при импорте.
- Скрытие реакций убирает reactions list и возможность нажать на реакцию в этом чате; серверные реакции и счётчики не меняются.
- Настройки не рассылают live-сигнал: уже открытый чат сохраняет старый tail/reactions до повторного открытия. Это задокументировано и является осознанным ограничением.
- `StripTracking` консервативен: неизвестные трекеры и percent-encoded имена остаются. Это лучше ложного удаления функционального параметра.
- Очистка выполняется и для `keepWebAuthTokens`-ссылок; список не содержит `token`/`hash`, поэтому login-flow не затрагивается, но это нужно перепроверить review'ом.
- `Config::fromJson` заменяет `_values` только принятыми ключами (существующее поведение); новый ключ, забытый в whitelist, будет потерян при импорте.
- `nix/update-patch.sh` включает все текущие незакоммиченные изменения `CMakeLists.txt`, `Telegram`, `exteraless`; перед запуском проверить `git status`, чтобы в патч не попал посторонний WIP.
- В тестовом Xvfb-профиле может не быть канала/группы с реакциями; тогда снимки для этих категорий неполные, это фиксируется в отчёте, а не подменяется фиктивным выводом.
- Полная сборка Nix-пакета не входит в план (дорогая); проверяются regenerated patch на чистом `v7.3.1` и `nix flake check --no-build`.

---

### Task 1: Config, cached chats snapshot, pure helpers and CLI

**Files:**
- Create: `exteraless/chats.h`
- Create: `exteraless/chats.cpp`
- Create: `exteraless/links.h`
- Create: `exteraless/links.cpp`
- Create: `exteraless/tests/chats/CMakeLists.txt`
- Create: `exteraless/tests/chats/chats_cli.cpp`
- Create: `exteraless/tests/chats/run_checks.py`
- Create: `exteraless/tests/chats/fixtures/strip.tsv`
- Create: `exteraless/tests/chats/fixtures/reactions.tsv`
- Modify: `exteraless/config.h`
- Modify: `exteraless/config.cpp`
- Modify: `exteraless/backup.cpp`
- Modify: `exteraless/CMakeLists.txt`

**Interfaces:**
- Produces: `Exteraless::ChatsSettings { bool removeMessageTail; bool hideReactionsChannels; bool hideReactionsGroups; bool hideReactionsPrivate; bool stripTrackingOnOpen; }`.
- Produces: `bool Exteraless::HiddenReactionsFor(const ChatsSettings&, bool channel, bool group, bool user)`.
- Produces: `bool Exteraless::HiddenReactionsFor(bool channel, bool group, bool user)`.
- Produces: `QString Exteraless::StripTracking(const QString &url)`.
- Produces: `ChatsSettings Exteraless::Config::chatsSettings() const`.
- Produces: `void Config::setChatsRemoveMessageTail(bool)`, `setChatsHideReactionsChannels(bool)`, `setChatsHideReactionsGroups(bool)`, `setChatsHideReactionsPrivate(bool)`, `setChatsStripTrackingOnOpen(bool)`.
- Produces (test only): `out/exteraless/tests/chats/exteraless-chats-cli strip <url>` и `... hidden <hideChannels> <hideGroups> <hidePrivate> <channel> <group> <user>`.

- [x] **Step 1: Добавить CLI, fixtures и runner**

`exteraless/tests/chats/CMakeLists.txt`:

```cmake
add_executable(exteraless-chats-cli EXCLUDE_FROM_ALL chats_cli.cpp)

target_link_libraries(exteraless-chats-cli
PRIVATE
    exteraless
)
```

`chats_cli.cpp` создаёт `QCoreApplication`, требует ровно один режим и печатает результат в stdout:

- `strip <url>` — `StripTracking(url)` и `\n`;
- `hidden <hideChannels> <hideGroups> <hidePrivate> <channel> <group> <user>` — собрать `ChatsSettings` с тремя флагами и вызвать explicit overload `HiddenReactionsFor`, напечатать `hidden` или `shown`;
- неверное число аргументов — usage в stderr и exit code `2`.

`fixtures/strip.tsv` (разделитель — tab, строки с `#` — комментарии):

```text
# input	expected
https://example.com/a?utm_source=x	https://example.com/a
https://example.com/a?utm_source=x&id=7#frag	https://example.com/a?id=7#frag
https://example.com/a?id=7&UTM_Medium=y	https://example.com/a?id=7
https://example.com/a?fbclid=abc&gclid=d	https://example.com/a
https://example.com/a?utm_source=x#section?utm_source=y	https://example.com/a#section?utm_source=y
https://example.com/a?si=abc&ref=nav&source=web&s=1	https://example.com/a?si=abc&ref=nav&source=web&s=1
https://example.com/a?q=a%2Fb&utm_term=x	https://example.com/a?q=a%2Fb
https://example.com/a?q=a+b&utm_campaign=x	https://example.com/a?q=a+b
https://example.com/a?utm_source=	https://example.com/a
http://example.com/?utm_source=x	http://example.com/
tg://resolve?utm_source=x	tg://resolve?utm_source=x
tonsite://example.com?utm_source=x	tonsite://example.com?utm_source=x
ftp://example.com/?utm_source=x	ftp://example.com/?utm_source=x
mailto:a@b.c?subject=x	mailto:a@b.c?subject=x
https://example.com/a#utm_source=x	https://example.com/a#utm_source=x
https://example.com/a?%75tm_source=x	https://example.com/a?%75tm_source=x
https://example.com/a?utm_source=x&utm_source=y&id=1	https://example.com/a?id=1
https://example.com/a?utm_source	https://example.com/a
HTTPS://Example.com/A?utm_source=x	HTTPS://Example.com/A
```

Итоговый `fixtures/strip.tsv` расширен до 38 кейсов: полный список click-id в
одном URL, no-op кейсы (`not a url`, `http:/example.com/?utm_source=x`,
`https://?utm_source=x`, `//example.com/?utm_source=x`), пустой query,
верхний регистр ключей и консервативно сохранённые неоднозначные параметры.

`fixtures/reactions.tsv`:

```text
# hideChannels	hideGroups	hidePrivate	channel	group	user	expected
0	0	0	1	0	0	shown
1	0	0	1	0	0	hidden
0	1	0	0	1	0	hidden
0	0	1	0	0	1	hidden
1	0	0	0	1	0	shown
0	1	0	1	0	0	shown
0	0	1	1	0	0	shown
1	1	1	0	0	1	hidden
1	1	1	1	0	0	hidden
1	1	1	0	1	0	hidden
```

`run_checks.py` принимает путь к CLI (по умолчанию `out/exteraless/tests/chats/exteraless-chats-cli`), читает оба TSV, запускает CLI через `subprocess.run(..., text=True, capture_output=True)`, сравнивает `stdout.rstrip("\n")` с ожиданием и печатает `strip: N passed`, `reactions: N passed`, `cli: 2 passed`. Два negative-кейса: `strip` без URL и `hidden` с пятью значениями вместо шести должны дать exit code `2`.

- [x] **Step 2: Подключить тесты и убедиться, что сборка падает**

В `exteraless/CMakeLists.txt` добавить `add_subdirectory(tests/chats)` после `add_subdirectory(tests/plugins)`.

Run:

```bash
nix develop --command cmake -S . -B out
nix develop --command ninja -C out exteraless-chats-cli
```

Expected: FAIL на `fatal error: 'exteraless/chats.h' file not found` или `'exteraless/links.h' file not found`.

- [x] **Step 3: Создать `exteraless/links.h` и `exteraless/links.cpp`**

```cpp
#pragma once

#include <QString>

namespace Exteraless {

[[nodiscard]] QString StripTracking(const QString &url);

} // namespace Exteraless
```

Алгоритм в `links.cpp` (нужен `#include <QStringList>`):

```cpp
QString StripTracking(const QString &url) {
	const auto colon = url.indexOf(u':');
	if (colon <= 0) {
		return url;
	}
	const auto scheme = url.left(colon);
	if (scheme.compare(QStringLiteral("http"), Qt::CaseInsensitive) != 0
		&& scheme.compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0) {
		return url;
	}
	const auto fragment = url.indexOf(u'#', colon + 1);
	const auto query = url.indexOf(u'?', colon + 1);
	if (query < 0 || (fragment >= 0 && query > fragment)) {
		return url;
	}
	const auto end = (fragment >= 0) ? fragment : int(url.size());
	const auto parts = url.mid(query + 1, end - query - 1)
		.split(u'&', Qt::KeepEmptyParts);
	auto kept = QStringList();
	for (const auto &part : parts) {
		const auto equal = part.indexOf(u'=');
		const auto key = (equal >= 0) ? part.left(equal) : part;
		if (!IsTrackingKey(key)) {
			kept.push_back(part);
		}
	}
	if (kept.size() == parts.size()) {
		return url;
	}
	auto result = url.left(query);
	if (!kept.isEmpty()) {
		result += u'?';
		result += kept.join(QStringLiteral("&"));
	}
	result += url.mid(end);
	return result;
}
```

`IsTrackingKey` сравнивает ключ case-insensitive: любой ключ с префиксом
`utm_` считается tracking, остальные сверяются с точным статическим списком:

```text
fbclid, gclid, dclid, gbraid, wbraid, msclkid, yclid, twclid, ttclid,
igshid, li_fat_id, mc_cid, mc_eid, _hsenc, _hsmi, mkt_tok, rb_clickid,
s_cid, vero_id, wickedid, oly_anon_id, oly_enc_id
```

Список консервативный: неоднозначные параметры (`ref`, `source`, `s`, `si`,
`start`, `feature`, `share`, `trk`, `ns_campaign`, `pk_campaign`,
`at_medium`, `spm`, `sc_cid`, `WT.mc_id`) остаются в URL, это закреплено в
`fixtures/strip.tsv`. Не декодировать `%XX` в ключе. Пустой ключ сохраняется.
Scheme проверяется case-insensitive, но в результате сохраняется исходный
регистр; authority должен существовать (`https://?utm_source=x` не
меняется). В `exteraless`-библиотеке нет include-пути `lib_base`, поэтому в
`links.cpp` и CLI использовать `QStringLiteral`, а не `_q`.

- [x] **Step 4: Создать `exteraless/chats.h` и `exteraless/chats.cpp`**

```cpp
#pragma once

namespace Exteraless {

struct ChatsSettings {
	bool removeMessageTail = true;
	bool hideReactionsChannels = false;
	bool hideReactionsGroups = false;
	bool hideReactionsPrivate = false;
	bool stripTrackingOnOpen = true;
};

[[nodiscard]] bool HiddenReactionsFor(
	const ChatsSettings &settings,
	bool channel,
	bool group,
	bool user);

[[nodiscard]] bool HiddenReactionsFor(
	bool channel,
	bool group,
	bool user);

} // namespace Exteraless
```

`chats.cpp` включает `exteraless/config.h` для convenience-overload'а:

```cpp
bool HiddenReactionsFor(
		const ChatsSettings &settings,
		bool channel,
		bool group,
		bool user) {
	return (channel && settings.hideReactionsChannels)
		|| (group && settings.hideReactionsGroups)
		|| (user && settings.hideReactionsPrivate);
}

bool HiddenReactionsFor(bool channel, bool group, bool user) {
	return HiddenReactionsFor(
		Config::Instance().chatsSettings(),
		channel,
		group,
		user);
}
```

- [x] **Step 5: Расширить `Config` ключами и кэшем**

В `config.cpp` добавить константы:

```cpp
constexpr auto kChatsRemoveMessageTail = "OEChatsRemoveMessageTail";
constexpr auto kChatsHideReactionsChannels = "OEChatsHideReactionsChannels";
constexpr auto kChatsHideReactionsGroups = "OEChatsHideReactionsGroups";
constexpr auto kChatsHideReactionsPrivate = "OEChatsHideReactionsPrivate";
constexpr auto kChatsStripTrackingOnOpen = "OEChatsStripTrackingOnOpen";
```

В `config.h` подключить `exteraless/chats.h`, добавить публичные методы:

```cpp
	[[nodiscard]] ChatsSettings chatsSettings() const;
	void setChatsRemoveMessageTail(bool value);
	void setChatsHideReactionsChannels(bool value);
	void setChatsHideReactionsGroups(bool value);
	void setChatsHideReactionsPrivate(bool value);
	void setChatsStripTrackingOnOpen(bool value);
```

и приватные:

```cpp
	void refreshChats() const;
	mutable ChatsSettings _chats;
```

`chatsSettings()` вызывает `ensureLoaded()` и возвращает `_chats` по значению. `refreshChats()` заполняет все пять полей через `boolValue(key, default)`.

Вызывать `refreshChats()`:
- в `ensureLoaded()` после успешного разбора `_values`;
- в `fromJson()` сразу после `_values = accepted; save();`;
- в каждом `setChats*` после `setValue`.

Значения по умолчанию задаются инициализаторами `ChatsSettings`, поэтому отсутствие файла или ошибка парсинга оставляют корректные defaults.

В `fromJson()` добавить все пять ключей в существующий whitelist. Порядок проверок сохранить, стиль — как у `kRelativeLastSeen`.

- [x] **Step 6: Добавить Android-ключи в backup**

В `backup.cpp` добавить константы:

```cpp
constexpr auto kRemoveMessageTail = "removeMessageTail";
constexpr auto kHideReactionsInChannels = "hideReactionsInChannels";
constexpr auto kHideReactionsInGroups = "hideReactionsInGroups";
constexpr auto kHideReactionsInPrivateChats = "hideReactionsInPrivateChats";
```

`BuildBackup()` записывает четыре boolean-значения из `Config::Instance().chatsSettings()`. `ApplyBackup()` принимает их только при `value.isBool()` и вызывает соответствующие `setChats*`; иначе ключ попадает в `unknown`. `OEChatsStripTrackingOnOpen` в backup не добавлять.

- [x] **Step 7: Подключить новые TUs в библиотеку**

В `exteraless/CMakeLists.txt` добавить в `add_library(exteraless STATIC ...)`:

```cmake
    chats.cpp
    chats.h
    links.cpp
    links.h
```

`Telegram/CMakeLists.txt` не менять: `Telegram` уже линкует `exteraless`.

- [x] **Step 8: Собрать и прогнать проверки**

```bash
nix develop --command cmake -S . -B out
nix develop --command ninja -C out exteraless-chats-cli
python3 exteraless/tests/chats/run_checks.py
```

Expected: PASS, `strip: 38 passed`, `reactions: 10 passed`, `cli: 2 passed`,
`config: 7 passed`, `backup: 22 passed`, `RESULT: PASS (79 checks)`.

- [x] **Step 9: Проверить defaults и отсутствие commit**

```bash
git diff --check -- exteraless/config.h exteraless/config.cpp exteraless/backup.cpp exteraless/CMakeLists.txt
git status --short
```

Expected: whitespace-ошибок в изменённых tracked-файлах Task 1 нет (новые файлы проверить вручную); в статусе появились только перечисленные файлы Task 1 поверх baseline. Не выполнять `git commit`/`git push`.

---

### Task 2: Chats settings, message tail and reactions integration

**Files:**
- Modify: `Telegram/SourceFiles/settings/sections/settings_exteraless.cpp`
- Modify: `Telegram/SourceFiles/history/view/history_view_message.cpp`
- Modify: `Telegram/SourceFiles/history/view/history_view_element.cpp`

**Interfaces:**
- Consumes: `Exteraless::Config::chatsSettings()`, `Exteraless::Config::setChats*`.
- Consumes: `Exteraless::HiddenReactionsFor(bool channel, bool group, bool user)`.
- Produces: подсекция `exteraless/chats` с четырьмя тумблерами; пятый добавляется в Task 3.
- Produces: `Message::countMessageRounding()` учитывает `removeMessageTail`.
- Produces: `Element::refreshReactions()` очищает список при скрытии.

- [x] **Step 1: Добавить подсекцию Chats и четыре тумблера**

В `settings_exteraless.cpp` после подсекции Ghost и перед Pill Stack добавить:

```cpp
	builder.addSubsectionTitle({
		.id = u"exteraless/chats"_q,
		.title = rpl::single(u"Chats"_q),
	});

	const auto addChatsToggle = [&](
			QString id,
			QString title,
			QStringList keywords,
			bool current,
			Fn<void(bool)> setter) {
		const auto toggle = builder.addButton({
			.id = std::move(id),
			.title = rpl::single(std::move(title)),
			.st = &st::settingsButtonNoIcon,
			.toggled = rpl::single(current),
			.keywords = std::move(keywords),
		});
		if (toggle) {
			toggle->toggledChanges(
			) | rpl::on_next([=](bool value) {
				setter(value);
			}, toggle->lifetime());
		}
	};
	addChatsToggle(
		u"exteraless/chats/remove-tail"_q,
		u"Remove message tail"_q,
		{ u"chat"_q, u"tail"_q, u"bubble"_q },
		Exteraless::Config::Instance().chatsSettings().removeMessageTail,
		[](bool value) {
			Exteraless::Config::Instance().setChatsRemoveMessageTail(value);
		});
	addChatsToggle(
		u"exteraless/chats/hide-reactions-channels"_q,
		u"Hide reactions in channels"_q,
		{ u"chat"_q, u"reactions"_q, u"channel"_q },
		Exteraless::Config::Instance().chatsSettings().hideReactionsChannels,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsHideReactionsChannels(value);
		});
	addChatsToggle(
		u"exteraless/chats/hide-reactions-groups"_q,
		u"Hide reactions in groups"_q,
		{ u"chat"_q, u"reactions"_q, u"group"_q },
		Exteraless::Config::Instance().chatsSettings().hideReactionsGroups,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsHideReactionsGroups(value);
		});
	addChatsToggle(
		u"exteraless/chats/hide-reactions-private"_q,
		u"Hide reactions in private chats"_q,
		{ u"chat"_q, u"reactions"_q, u"private"_q },
		Exteraless::Config::Instance().chatsSettings().hideReactionsPrivate,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsHideReactionsPrivate(value);
		});
```

Не добавлять новых `lang.strings`-ключей и не менять порядок существующих подсекций.

- [x] **Step 2: Учесть `removeMessageTail` в `Message::countMessageRounding()`**

В `history_view_message.cpp` подключить `exteraless/config.h`. В `Message::countMessageRounding()` заменить начало вычисления `skipTail`:

```cpp
	const auto skipTail = Exteraless::Config::Instance()
		.chatsSettings().removeMessageTail
		|| smallBottom
		|| (media && media->skipBubbleTail())
		|| (rich && richMediaAtBottom())
		|| (keyboard != nullptr)
		|| item->isFakeAboutView()
		|| isCommentsRootView();
```

Tail — только `Ui::BubbleCornerRounding::Tail`; при `removeMessageTail=true` нижние несоединённые углы становятся `Large`. Attached bubbles (`smallBottom`) и right layout не ломаются, потому что их ветки уже выбирают `Small`/`Large` до проверки tail.

- [x] **Step 3: Скрыть reactions list в `Element::refreshReactions()`**

В `history_view_element.cpp` подключить `exteraless/chats.h`. В самом начале `Element::refreshReactions()`, до `InlineListDataFromMessage`, добавить:

```cpp
	const auto peer = history()->peer;
	if (Exteraless::HiddenReactionsFor(
			peer->isBroadcast(),
			peer->isChat() || peer->isMegagroup(),
			peer->isUser())) {
		setReactions(nullptr);
		return;
	}
```

`isBroadcast()` — канал, `isChat() || isMegagroup()` — группа/супергруппа, `isUser()` — личный чат или бот. `setReactions(nullptr)` обязателен, чтобы уже построенный список исчез при следующем refresh.

- [x] **Step 4: Скомпилировать затронутые TUs**

```bash
nix develop --command ninja -C out \
  exteraless/CMakeFiles/exteraless.dir/config.cpp.o \
  exteraless/CMakeFiles/exteraless.dir/chats.cpp.o \
  exteraless/CMakeFiles/exteraless.dir/links.cpp.o \
  Telegram/CMakeFiles/Telegram.dir/SourceFiles/settings/sections/settings_exteraless.cpp.o \
  Telegram/CMakeFiles/Telegram.dir/SourceFiles/history/view/history_view_message.cpp.o \
  Telegram/CMakeFiles/Telegram.dir/SourceFiles/history/view/history_view_element.cpp.o
```

Expected: PASS без предупреждений о неиспользуемых include/переменных.

- [x] **Step 5: Проверить, что горячий путь не читает JSON**

```bash
rg -n "boolValue|QJsonObject|_values" Telegram/SourceFiles/history/view/history_view_message.cpp \
  Telegram/SourceFiles/history/view/history_view_element.cpp
```

Expected: совпадений нет; оба файла используют только `Config::chatsSettings()` / `HiddenReactionsFor`.

- [x] **Step 6: Проверить границы и не коммитить**

```bash
git diff --check -- Telegram/SourceFiles/settings/sections/settings_exteraless.cpp Telegram/SourceFiles/history/view/history_view_message.cpp Telegram/SourceFiles/history/view/history_view_element.cpp
git status --short
```

Expected: изменены только три файла Task 2 поверх baseline. Не выполнять commit/push.

---

### Task 3: Tracking link integration, fifth setting, docs

**Files:**
- Modify: `Telegram/SourceFiles/core/ui_integration.cpp`
- Modify: `Telegram/SourceFiles/settings/sections/settings_exteraless.cpp`
- Modify: `docs/feature-mapping.md`
- Modify: `CHANGELOG.md`

**Interfaces:**
- Consumes: `Exteraless::StripTracking(const QString&)`.
- Consumes: `Exteraless::Config::chatsSettings().stripTrackingOnOpen`.
- Produces: очистка URL в `UiIntegration::handleUrlClick()` до `UrlWithoutWebAuthTokens`.
- Produces: тумблер `exteraless/chats/strip-tracking`.
- Produces: документационный disclaimer про повторное открытие чата.

- [x] **Step 1: Встроить очистку в `UiIntegration::handleUrlClick()`**

В `ui_integration.cpp` подключить `exteraless/config.h` и `exteraless/links.h`. Заменить начало функции:

```cpp
bool UiIntegration::handleUrlClick(
		const QString &original,
		const QVariant &context) {
	const auto clickUrl = Exteraless::Config::Instance()
		.chatsSettings().stripTrackingOnOpen
		? Exteraless::StripTracking(original)
		: original;
	const auto url = context.value<ClickHandlerContext>().keepWebAuthTokens
		? clickUrl
		: UrlWithoutWebAuthTokens(clickUrl);
```

Порядок важен: `StripTracking` работает до `UrlWithoutWebAuthTokens` и до `TryConvertUrlToLocal`. Внутренние `tg://`, `tonsite://`, `internal:` не меняются, потому что helper пропускает всё, кроме `http`/`https`. `keepWebAuthTokens` сохраняет токены: список tracking-параметров не содержит `token`/`hash`.

- [x] **Step 2: Добавить пятый тумблер в Chats**

После четырёх вызовов `addChatsToggle` из Task 2 добавить:

```cpp
	addChatsToggle(
		u"exteraless/chats/strip-tracking"_q,
		u"Strip tracking parameters from links"_q,
		{ u"chat"_q, u"links"_q, u"tracking"_q, u"utm"_q },
		Exteraless::Config::Instance().chatsSettings().stripTrackingOnOpen,
		[](bool value) {
			Exteraless::Config::Instance()
				.setChatsStripTrackingOnOpen(value);
		});
```

- [x] **Step 3: Обновить `docs/feature-mapping.md`**

В разделе `Chats` заменить строку:

```text
  - strip tracking on open/paste (`OEChatsStripTrackingOnOpen/Paste`, default `true`).
```

на:

```text
  - strip tracking on open (`OEChatsStripTrackingOnOpen`, default `true`).
```

Сразу после списка ключей Chats добавить абзац:

```text
`OEChatsRemoveMessageTail` and `OEChatsHideReactions*` are read when a chat
view is built: close and reopen the chat, or restart the app, for a change to
take effect. `OEChatsStripTrackingOnOpen` applies only to clicked http/https
links; paste-time stripping is not implemented.
```

- [x] **Step 4: Добавить запись в `CHANGELOG.md`**

В секцию `## 2026-11-04` добавить:

```markdown
- Chats: перенесены `OEChatsRemoveMessageTail` (default `true`) и
  `OEChatsHideReactionsChannels/Groups/Private` (default `false`); настройки
  читаются при построении чата, поэтому применяются после его повторного
  открытия. Добавлен `OEChatsStripTrackingOnOpen` (default `true`): при клике
  по http/https удаляются известные tracking-параметры, `ref`/`source`/`s`/`si`
  не трогаются. Android backup: `removeMessageTail`, `hideReactionsInChannels`,
  `hideReactionsInGroups`, `hideReactionsInPrivateChats`; strip tracking в Android
  backup отсутствует.
```

- [x] **Step 5: Скомпилировать затронутые TUs**

```bash
nix develop --command ninja -C out \
  Telegram/CMakeFiles/Telegram.dir/SourceFiles/core/ui_integration.cpp.o \
  Telegram/CMakeFiles/Telegram.dir/SourceFiles/settings/sections/settings_exteraless.cpp.o
```

Expected: PASS.

- [x] **Step 6: Проверить документацию и не коммитить**

```bash
rg -n "OnOpen/Paste|stripTrackingOnOpen|OEChatsHideReactions" docs/feature-mapping.md
git diff --check -- Telegram/SourceFiles/core/ui_integration.cpp Telegram/SourceFiles/settings/sections/settings_exteraless.cpp docs/feature-mapping.md CHANGELOG.md
git status --short
```

Expected: в docs только `OEChatsStripTrackingOnOpen` без `/Paste`; whitespace чистый; изменены только файлы Task 2–3 поверх baseline. Не выполнять commit/push.

---

### Task 4: Full build, Xvfb smoke, patch regeneration and review

**Files:**
- Regenerate: `nix/patches/exteraless.patch`
- No source edits in this task; failures return to the owning task.

**Interfaces:**
- Consumes: все интерфейсы Task 1–3.
- Produces: Debug-бинарник, CLI-отчёт, Xvfb-скриншоты on/off, regenerated patch, flake check, итоговый review-отчёт.

- [x] **Step 1: Переконфигурировать и собрать Debug**

```bash
nix develop --command cmake -S . -B out
nix develop --command cmake --build out --target Telegram -j2
```

Expected: PASS, `out/bin/Telegram` обновлён.

- [x] **Step 2: Применить LLD-релинк только при известном debug-info overflow**

Если финальный линк GNU ld падает на overflow (>4 GiB), выполнить один LLD-релинк, не меняя закоммиченный конфиг. Нужны оба флага — `-fuse-ld=lld` и `-Wl,-S`:

```bash
nix develop --command bash -c '
  lld=$(nix build --no-link --print-out-paths nixpkgs#lld)
  link=$(ninja -C out -t commands bin/Telegram | tail -1)
  PATH="$lld/bin:$PATH" bash -c "${link/g++/g++ -fuse-ld=lld -Wl,-S}"
'
```

Записать в отчёт, что линк выполнен LLD со strip.

- [x] **Step 3: Прогнать CLI-проверки на итоговой сборке**

```bash
nix develop --command ninja -C out exteraless-chats-cli
python3 exteraless/tests/chats/run_checks.py
```

Expected: `strip: 38 passed`, `reactions: 10 passed`, `cli: 2 passed`,
`config: 7 passed`, `backup: 22 passed`, `RESULT: PASS (79 checks)`.

- [x] **Step 4: Подготовить изолированный Xvfb-профиль**

```bash
export SMOKE=/tmp/opencode/nagramx-chats-ui
export DISPLAY=:99
rm -rf "$SMOKE" "$SMOKE-cache"
mkdir -p "$SMOKE" "$SMOKE-cache"
cp -a /tmp/opencode/smoke-drawer5/tdata "$SMOKE/tdata"
pgrep -f "Xvfb :99" >/dev/null || \
  (Xvfb :99 -screen 0 1280x800x24 >/tmp/opencode/xvfb99-chats.log 2>&1 &)
```

Если указанного локального smoke-профиля нет, взять другой локальный профиль с залогиненным тестовым аккаунтом. Не копировать и не изменять реальный `~/.local/share/ExteralessDesktop`.

- [x] **Step 5: Снять settings on/off**

Записать конфиг с выключенными функциями:

```bash
mkdir -p "$SMOKE/ExteralessDesktop/tdata"
cat > "$SMOKE/ExteralessDesktop/tdata/exteraless.json" <<'JSON'
{"version":1,"exteraconfig":{"OEChatsRemoveMessageTail":false,"OEChatsHideReactionsChannels":false,"OEChatsHideReactionsGroups":false,"OEChatsHideReactionsPrivate":false,"OEChatsStripTrackingOnOpen":false}}
JSON
XDG_DATA_HOME="$SMOKE" XDG_CACHE_HOME="$SMOKE-cache" QT_QPA_PLATFORM=xcb \
  out/bin/Telegram -workdir "$SMOKE" -noupdate >"$SMOKE/app.log" 2>&1 &
echo $! > "$SMOKE/app.pid"
sleep 12
python3 /tmp/opencode/xinput.py click 45 65
sleep 1
python3 /tmp/opencode/xinput.py click 87 541
sleep 2
import -window root "$SMOKE/chats-settings-off.png"
kill "$(cat "$SMOKE/app.pid")"
wait "$(cat "$SMOKE/app.pid")" 2>/dev/null || true
```

Координаты меню/настроек взяты из предыдущего smoke; если раскладка отличается, скорректировать по первому снимку и записать фактические в отчёт. `chats-settings-off.png`: все пять тумблеров выключены. Затем записать тот же JSON с `true` для всех пяти, перезапустить тем же способом, открыть ту же страницу и снять `chats-settings-on.png`: все пять включены. Проверить, что подсекция называется `Chats` и что порядок: General, Ghost Mode, Chats, Pill Stack, Proxy, Backup, Plugins.

- [x] **Step 6: Проверить tail on/off**

При выключенном `removeMessageTail` открыть личный чат с короткими сообщениями и снять `tail-off.png`: у последнего несоединённого пузыря виден нижний хвост. Перезапустить с `removeMessageTail=true`, открыть тот же чат и снять `tail-on.png`: хвоста нет, нижний угол `Large`. Проверить attached-пару сообщений: верхний пузырь остаётся `Small` снизу, right layout не ломается.

- [x] **Step 7: Проверить reactions on/off**

При всех `hideReactions* = false` открыть канал, группу и личный чат с реакциями, снять по одному снимку с видимым reactions list. Перезапустить с тремя `true`, открыть те же чаты, снять снимки без reactions list. Если в профиле нет канала/группы с реакциями, зафиксировать это в отчёте как непроверенную категорию, не подменять результат.

- [x] **Step 8: Проверить tracking-очистку и отсутствие paste-интеграции**

CLI уже покрывает `StripTracking`. Дополнительно:

```bash
rg -n "StripTracking" Telegram/SourceFiles exteraless
rg -n "StripTracking" Telegram/SourceFiles | rg -v "core/ui_integration.cpp"
```

Expected: единственная production-точка — `core/ui_integration.cpp`; в paste/input path вызовов нет. End-to-end клик по ссылке не запускать, чтобы smoke не открывал внешний браузер; helper-поведение подтверждено fixtures.

- [x] **Step 9: Перегенерировать патч**

Перед запуском сверить `git status --short` с baseline и понять, какие пути попадут в патч:

```bash
git status --short
nix/update-patch.sh
```

Expected: `nix/patches/exteraless.patch` обновлён и содержит новые файлы `chats.*`, `links.*`, `tests/chats/*`, правки config/backup/UI/docs. Если в дереве оставался предсуществующий WIP, патч включает и его: это нормальное поведение `nix/update-patch.sh`, но в отчёте нельзя называть такой патч чистым diff'ом только этого батча.

- [x] **Step 10: Проверить патч на чистом `v7.3.1`**

```bash
patch_dir=/tmp/opencode/tdesktop-clean-nagram
rm -rf "$patch_dir"
git worktree add --detach "$patch_dir" v7.3.1
git -C "$patch_dir" apply --check "$PWD/nix/patches/exteraless.patch"
git -C "$patch_dir" apply "$PWD/nix/patches/exteraless.patch"
git -C "$patch_dir" status --short | wc -l
diff -q "$patch_dir/exteraless/chats.cpp" exteraless/chats.cpp
diff -q "$patch_dir/exteraless/links.cpp" exteraless/links.cpp
diff -q "$patch_dir/Telegram/SourceFiles/exteraless/plugin_runtime.cpp" \
  Telegram/SourceFiles/exteraless/plugin_runtime.cpp
git worktree remove --force "$patch_dir"
```

Expected: `apply --check` PASS; изменённые пути содержат `exteraless/chats.*`,
`exteraless/links.*`, `exteraless/tests/chats/*`, `exteraless/tests/plugins/*`,
`Telegram/SourceFiles/exteraless/plugin_runtime.*` и правки config/backup/UI;
`diff -q` по ключевым файлам молчит; временный worktree удалён. Не коммитить.

- [x] **Step 11: Проверить flake**

```bash
nix flake check --no-build
```

Expected: PASS. Полную сборку Nix-пакета не запускать; записать это ограничение.

- [x] **Step 12: Финальный review и отчёт**

Проверить и записать:

- пять `OE*`-ключей и defaults совпадают с Global Constraints;
- `fromJson` принимает все пять, `BuildBackup`/`ApplyBackup` — четыре Android-ключа, strip tracking в backup отсутствует;
- `countMessageRounding()` и `refreshReactions()` читают только кэш;
- `ui_integration.cpp` очищает URL до `UrlWithoutWebAuthTokens`;
- `ref/source/s/si` не входят в tracking list;
- `git status --short Telegram/lib_ui` пуст, `lib_ui` не изменён;
- нет `#ifdef _DEBUG`, debug-полей, новых `lang.strings`, hardcoded размеров;
- `git diff --check -- exteraless/chats.h exteraless/chats.cpp exteraless/links.h exteraless/links.cpp exteraless/config.h exteraless/config.cpp exteraless/backup.cpp exteraless/CMakeLists.txt exteraless/tests/chats Telegram/SourceFiles/history/view/history_view_message.cpp Telegram/SourceFiles/history/view/history_view_element.cpp Telegram/SourceFiles/core/ui_integration.cpp Telegram/SourceFiles/settings/sections/settings_exteraless.cpp docs/feature-mapping.md CHANGELOG.md` чистый (untracked-файлы проверить вручную); файлы LF/UTF-8 без BOM;
- CLI: 38 + 10 + 2 + 7 + 22 = 79 проверок, `RESULT: PASS (79 checks)`; плагины
  без изменений — 32 и 213 проверок; Xvfb: settings on/off, tail on/off,
  reactions on/off (private-реакции в профиле отсутствовали, проверены только
  CLI);
- patch apply на `v7.3.1` и `nix flake check --no-build` прошли;
- commit/push не выполнялись.

Итоговый отчёт — пути изменённых файлов, результаты проверок, непроверенные категории и known risks. Не выполнять `git commit`/`git push`.

## Result and Remaining Limitations

- Implemented keys and defaults: `OEChatsRemoveMessageTail=true`,
  `OEChatsHideReactionsChannels/Groups/Private=false`,
  `OEChatsStripTrackingOnOpen=true`; Settings → Exteraless → Chats has five
  toggles and keeps the subsection order General, Ghost Mode, Chats, Pill
  Stack, Proxy, Backup, Plugins.
- `.extera` backup uses `removeMessageTail`, `hideReactionsInChannels`,
  `hideReactionsInGroups`, `hideReactionsInPrivateChats`; the old
  `hideReactionsInPrivate` name is reported as unknown on import, and strip
  tracking is desktop-only and absent from the backup.
- Tail and reactions are cached when a chat view is built; an already open chat
  keeps the old state until it is reopened, and reactions in the pinned bar can
  require an app restart.
- The tracking allowlist stays conservative: `utm_*` plus the fixed click-id
  list. Ambiguous parameters (`ref`, `source`, `s`, `si`, `start`, `feature`,
  `share`, `trk`, `ns_campaign`, `pk_campaign`, `at_medium`, `spm`, `sc_cid`,
  `WT.mc_id`) are kept and percent-encoded names are not decoded. Stripping is
  click-time only; pasted text is untouched.
- Xvfb verified the Chats subsection, the bubble tail on/off and hidden
  reactions in a channel and in a group. The smoke profile had no private chat
  with reactions, so private-chat reactions were verified only by the CLI
  fixtures.
- No overlay or extra box is added: the message view paints no reactions list
  and the settings page only gains the Chats subsection.
- `OEChatsWideChannelPosts` remains unported, and the Appearance keys (section
  radius, separate headers, square FAB, avatar corners) have no desktop analog
  and are not implemented.
- Verification: chats 79 checks, plugin manifest 32, plugin host 213,
  `nix flake check --no-build`, `ninja -C out -n Telegram` without compile or
  link edges, and patch `apply --check` plus apply on a clean v7.3.1.
- A full Nix package build was not run; the `out/bin/Telegram` binary is the
  Debug build relinked with LLD because GNU ld hits debug-info overflow.
