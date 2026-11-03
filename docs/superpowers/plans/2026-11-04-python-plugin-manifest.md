# Python Plugin Manifest (MVP3 Stage 1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Безопасный первый этап MVP3: каталог `tdata/plugins`, чтение манифестов `.plugin`/`.py` через один `python3 -I -B -c` AST-проход без исполнения кода плагина, отображение списка и ошибок в боксе, открытие каталога из настроек Exteraless.

**Status:** Completed (stage 1), backend/UI/LLD проверены. План остаётся durable-записью исходного проекта, а не чеклистом к исполнению; никакого исполнения, установки, включения или выключения плагинов на этом этапе нет.

**Architecture:** `exteraless` (без UI) получает сканер: C++ запускает один изолированный Python-процесс на каталог, Python читает файлы, вызывает только `ast.parse` + `ast.literal_eval`, печатает JSON-массив. C++ разбирает JSON в `Exteraless::Plugins::ScanResult`. UI-бокс в `Telegram/SourceFiles/exteraless/` вызывает сканер при открытии; настройки Exteraless дают кнопку входа. Тестовый CLI и фикстуры живут в `exteraless/tests/plugins/` и не входят в приложение.

**Spec:** `STATUS.md` (MVP3: Python-плагины), `docs/feature-mapping.md` (раздел Plugins). Этот план закрывает только манифест и просмотр.

**Tech Stack:** C++20, Qt 6 Core (`QProcess`, `QJsonDocument`, `QStandardPaths`), Python 3 stdlib, tdesktop 7.3.1, CMake/Ninja, Xvfb.

## Global Constraints

- Правки production-кода только в: `exteraless/plugins/scanner.{h,cpp}`, `exteraless/CMakeLists.txt`, `Telegram/SourceFiles/exteraless/plugins_box.{h,cpp}`, `Telegram/CMakeLists.txt`, `Telegram/SourceFiles/settings/sections/settings_exteraless.cpp`.
- Тестовый harness только в: `exteraless/tests/plugins/` (CLI, CMake, фикстуры).
- Документы — только в Task 3.
- Никакого исполнения кода плагина: ни `exec`, ни `eval`, ни `compile`, ни импорта файла плагина; stdlib-импорты в embedded-скрипте (`ast`, `json`, `os`, `re`, `sys`) разрешены, но к коду плагина не применяются.
- Один процесс `python3` на весь каталог; никаких процессов на файл, никаких shell-обёрток.
- Каталог плагинов ровно `cWorkingDir() + "tdata/plugins"`; расширения `.plugin`, `.py`; `.elyx` и `.eaf` дают per-file `error = "Unsupported plugin format: .<ext>."`; прочие расширения игнорируются.
- Известные разрешения: `ui`, `messages.read`, `messages.send`, `network`, `files`, `intents`, `settings`, `hooks`, `native`. Неизвестное разрешение — ошибка записи, не глобальная.
- Лимит файла 2 MiB (`2 * 1024 * 1024`); больше — per-file ошибка. Симлинки не читаются.
- id валидируется регэкспом `^[A-Za-z][A-Za-z0-9_-]{1,31}$` (2–32 символа). `__id__` и `__name__` обязательны; `__description__`, `__author__`, `__version__`, `__permissions__` опциональны.
- Один JSON-массив в stdout при успехе. Ошибки процесса (нет `python3`, таймаут, невалидный JSON, ненулевой exit) — глобальный `ScanResult::error`.
- Никаких новых ключей `exteraless.json`, никаких `lang.strings`, новых зависимостей, `#ifdef _DEBUG` и тестовых seam'ов в production-коде.
- Только Debug-сборка; параллелизм не выше `-j2` (16 ГБ без свапа).
- Не выполнять `git commit`, `git push`, `git reset`, `git rebase`; `git add` допустим только как `git add -N` внутри `nix/update-patch.sh`.
- Текстовые файлы — LF, UTF-8 без BOM. Комментарии — максимум одна строка и только «почему».
- Smoke-тесты запускать только с изолированными `-workdir`, `XDG_DATA_HOME`, `XDG_CACHE_HOME`; реальный профиль `~/.local/share/ExteralessDesktop` не трогать; останавливать только записанный PID.

## Review Focus

- Сканер не исполняет код плагина: в embedded-скрипте нет `exec`/`eval`/`compile`, файл плагина не импортируется; динамический манифест (`__name__ = "a" + "b"`) даёт ошибку, а не значение.
- `.elyx`/`.eaf` попадают в список как unsupported error; `.txt` и каталоги не попадают.
- Неизвестное разрешение даёт ошибку только этой записи; остальные записи сканируются.
- Нет `python3` — бокс открывается и показывает глобальную ошибку; приложение не падает.
- Таймаут убивает процесс, ждёт завершения и возвращает `Plugin scan timed out.`
- Бокс показывает количество, имя, версию, автора, разрешения и ошибку; кнопки только «Open folder» и «Close»; нет install/uninstall/enable/run.
- Настройки Exteraless не меняют порядок существующих секций и работают при `controller == nullptr` (кнопка есть, клик ничего не делает).
- Повторное открытие бокса пересканирует каталог; кэша и фонового watcher'а нет.

---

## Implemented deviations

Реализация строже исходного проекта в задачах ниже. Фактические усиления:

- Embedded-сканер открывает файл через `os.open` с `O_NOFOLLOW`/`O_BINARY` и проверяет `os.fstat(...)` + `stat.S_ISREG`, поэтому симлинк или подмена файла между листингом и открытием не читаются (TOCTOU закрыт). Листинг каталога тоже фильтрует через `entry.stat(follow_symlinks=False)` и `S_ISREG`, а не через `entry.is_file()`.
- Чтение ограничено `source.read(MAX_BYTES + 1)` с повторной проверкой `len(data) > MAX_BYTES`: лимит 2 MiB держится и для файла, выросшего после `fstat`.
- `RecursionError` при `ast.parse` даёт per-file `File is nested too deeply to scan.`, `MemoryError` — per-file `File is too large to parse.`; остальные записи каталога продолжают сканироваться.
- Повторяемый прогон backend-проверок: `python3 exteraless/tests/plugins/run_checks.py [path/to/exteraless-plugins-scan]` — 32 проверки (фикстуры, рекурсия, границы 2 MiB, отсутствие `python3`, битый JSON, ненулевой exit, таймаут, один процесс на каталог, отсутствие исполнения, пропуск каталогов/симлинков/`.txt`). CLI по умолчанию берётся из `out/`.
- Debug-линк потребовал LLD: `-fuse-ld=lld` вместе с `-Wl,-S`, потому что и GNU ld, и LLD без strip переполняют debug-info (>4 GiB). Это разовое действие при сборке; закоммиченный CMake-конфиг не менялся.
- Скан в UI синхронный: бокс открывается после завершения сканера, худший случай — таймаут. Осознанное ограничение этапа 1, а не недосмотр.
- Установки, включения, запуска и удаления плагинов нет: бокс только читает и показывает записи.
- Подписи в настройках остались `Plugins` — и подсекция, и кнопка (`settings_exteraless.cpp`), то есть план в этой части совпадает с кодом; варианты `Python plugins`/`Manage plugins` не вводились.

Чекбоксы `- [ ]` в задачах ниже оставлены как исходная история плана; фактическое состояние — Completed (stage 1).

---

### Task 1: Scanner, CLI harness, fixtures and negative tests

**Files:**
- Create: `exteraless/plugins/scanner.h`
- Create: `exteraless/plugins/scanner.cpp`
- Modify: `exteraless/CMakeLists.txt`
- Create: `exteraless/tests/plugins/CMakeLists.txt`
- Create: `exteraless/tests/plugins/scan_cli.cpp`
- Create: `exteraless/tests/plugins/fixtures/valid.plugin`
- Create: `exteraless/tests/plugins/fixtures/dynamic.plugin`
- Create: `exteraless/tests/plugins/fixtures/unknown-permission.plugin`
- Create: `exteraless/tests/plugins/fixtures/broken.plugin`
- Create: `exteraless/tests/plugins/fixtures/invalid-id.plugin`
- Create: `exteraless/tests/plugins/fixtures/legacy.elyx`
- Create: `exteraless/tests/plugins/fixtures/notes.txt`

**Interfaces:**
- Produces: `Exteraless::Plugins::Entry`, `Exteraless::Plugins::ScanResult`, `Exteraless::Plugins::ScanPlugins(const QString&)`.
- Produces (test only): `out/exteraless/tests/plugins/exteraless-plugins-scan <directory>`, печатает `{"error": "...", "entries": [...]}` в stdout.

- [ ] **Step 1: Добавить `exteraless/plugins/scanner.h`**

```cpp
#pragma once

#include <QString>
#include <QStringList>

#include <vector>

namespace Exteraless::Plugins {

struct Entry {
	QString file;
	QString id;
	QString name;
	QString description;
	QString author;
	QString version;
	QStringList permissions;
	QString error;
};

struct ScanResult {
	std::vector<Entry> entries;
	QString error;
};

[[nodiscard]] ScanResult ScanPlugins(const QString &directory);

} // namespace Exteraless::Plugins
```

- [ ] **Step 2: Добавить `exteraless/plugins/scanner.cpp`**

> **Примечание:** сниппет ниже — исходный проект, а не текущий код. Авторитетна реализация в `exteraless/plugins/scanner.cpp`: там фактический embedded-скрипт, а его отличия перечислены в `## Implemented deviations`. Актуальные строки сюда не дублируются.

Константы в анонимном namespace:

```cpp
constexpr auto kStartTimeout = 5000;
constexpr auto kScanTimeout = 10000;
constexpr auto kKillTimeout = 1000;
```

Embedded-скрипт — ровно такой (никаких импортов плагина и никакого `exec`):

```python
import ast
import json
import os
import re
import sys

ID_RE = re.compile(r'^[A-Za-z][A-Za-z0-9_-]{1,31}$')
KNOWN = {
    'ui', 'messages.read', 'messages.send', 'network', 'files',
    'intents', 'settings', 'hooks', 'native',
}
MAX_BYTES = 2 * 1024 * 1024
SUPPORTED = ('.plugin', '.py')
UNSUPPORTED = ('.elyx', '.eaf')
STRINGS = {
    '__id__': 'id',
    '__name__': 'name',
    '__description__': 'description',
    '__author__': 'author',
    '__version__': 'version',
}


def empty(name):
    return {
        'file': name,
        'id': '',
        'name': '',
        'description': '',
        'author': '',
        'version': '',
        'permissions': [],
        'error': '',
    }


def failed(name, error):
    result = empty(name)
    result['error'] = error
    return result


def scan(path, name):
    try:
        size = os.path.getsize(path)
    except OSError as error:
        return failed(name, 'Could not stat file: %s' % error)
    if size > MAX_BYTES:
        return failed(name, 'File is larger than 2 MiB.')
    try:
        with open(path, 'rb') as source:
            text = source.read().decode('utf-8')
    except (OSError, UnicodeDecodeError) as error:
        return failed(name, 'Could not read file: %s' % error)
    try:
        tree = ast.parse(text, filename=name)
    except SyntaxError as error:
        return failed(name, 'Syntax error: %s' % (error.msg,))
    except ValueError as error:
        return failed(name, 'Syntax error: %s' % error)

    values = {}
    permissions = None
    for node in tree.body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1:
            target = node.targets[0]
        elif isinstance(node, ast.AnnAssign):
            target = node.target
        else:
            continue
        if not isinstance(target, ast.Name):
            continue
        key = target.id
        if key not in STRINGS and key != '__permissions__':
            continue
        try:
            value = ast.literal_eval(node.value)
        except (ValueError, TypeError, SyntaxError, RecursionError):
            return failed(name, 'Metadata %s is not a literal.' % key)
        if key == '__permissions__':
            permissions = value
        else:
            values[STRINGS[key]] = value

    if 'id' not in values:
        return failed(name, 'Missing literal __id__.')
    if not isinstance(values['id'], str):
        return failed(name, 'Metadata __id__ is not a string.')
    if not ID_RE.match(values['id']):
        return failed(name, 'Invalid id: %s.' % values['id'])
    if 'name' not in values:
        return failed(name, 'Missing literal __name__.')
    if not isinstance(values['name'], str) or not values['name']:
        return failed(name, 'Metadata __name__ is not a non-empty string.')
    for key in ('description', 'author', 'version'):
        if not isinstance(values.get(key, ''), str):
            return failed(name, 'Metadata %s is not a string.' % key)
    if permissions is None:
        permissions = []
    if not isinstance(permissions, (list, tuple)):
        return failed(name, '__permissions__ is not a list of strings.')
    cleaned = []
    for permission in permissions:
        if not isinstance(permission, str):
            return failed(name, '__permissions__ is not a list of strings.')
        if permission not in KNOWN:
            return failed(name, 'Unknown permission: %s.' % permission)
        cleaned.append(permission)

    result = empty(name)
    result['id'] = values['id']
    result['name'] = values['name']
    result['description'] = values.get('description', '')
    result['author'] = values.get('author', '')
    result['version'] = values.get('version', '')
    result['permissions'] = cleaned
    return result


def main():
    directory = sys.argv[1]
    try:
        with os.scandir(directory) as iterator:
            files = sorted(
                (entry for entry in iterator
                 if entry.is_file(follow_symlinks=False)),
                key=lambda entry: entry.name)
    except OSError as error:
        print('Could not read directory: %s' % error, file=sys.stderr)
        sys.exit(1)

    results = []
    for entry in files:
        name = entry.name
        lower = name.lower()
        if lower.endswith(UNSUPPORTED):
            results.append(failed(
                name,
                'Unsupported plugin format: %s.' % os.path.splitext(name)[1]))
        elif lower.endswith(SUPPORTED):
            results.append(scan(entry.path, name))
    print(json.dumps(results))


main()
```

Скрипт хранить как `constexpr auto kScanScript = R"PY(...)PY";`.

- [ ] **Step 3: Реализовать `ScanPlugins`**

Порядок:

```cpp
auto result = ScanResult();
const auto python = QStandardPaths::findExecutable(QStringLiteral("python3"));
if (python.isEmpty()) {
	result.error = QStringLiteral("Python 3 was not found.");
	return result;
}
auto process = QProcess();
process.start(python, {
	QStringLiteral("-I"),
	QStringLiteral("-B"),
	QStringLiteral("-c"),
	QString::fromUtf8(kScanScript),
	directory,
});
if (!process.waitForStarted(kStartTimeout)) {
	result.error = QStringLiteral("Plugin scan failed to start.");
	return result;
}
if (!process.waitForFinished(kScanTimeout)) {
	process.kill();
	process.waitForFinished(kKillTimeout);
	result.error = QStringLiteral("Plugin scan timed out.");
	return result;
}
const auto output = process.readAllStandardOutput();
if (process.exitStatus() != QProcess::NormalExit
	|| process.exitCode() != 0) {
	const auto error = QString::fromUtf8(
		process.readAllStandardError()).trimmed();
	result.error = error.isEmpty()
		? QStringLiteral("Plugin scan failed.")
		: QStringLiteral("Plugin scan failed: %1").arg(error);
	return result;
}
```

Разбор: `QJsonDocument::fromJson`; ошибка парсинга, не-массив — `result.error = QStringLiteral("Plugin scan returned invalid output.")`. Для каждого элемента требовать объект; строки `file`, `id`, `name`, `description`, `author`, `version`, `error` читать через `toString()`, `permissions` — массив строк; любое несоответствие типов — та же глобальная ошибка. Записи складывать в `result.entries` в порядке JSON.

В `scanner.cpp` использовать `QStringLiteral`, а не `u"..."_q`: цель `exteraless` не подключает заголовки `lib_base`, поэтому литерал `_q` там недоступен.

Подключения: `<QJsonArray>`, `<QJsonDocument>`, `<QJsonObject>`, `<QJsonParseError>`, `<QProcess>`, `<QStandardPaths>`, `<QStringList>`.

- [ ] **Step 4: Подключить сканер к библиотеке**

В `exteraless/CMakeLists.txt` добавить в `add_library(exteraless STATIC ...)`:

```cmake
    plugins/scanner.cpp
    plugins/scanner.h
```

и в конец файла:

```cmake
add_subdirectory(tests/plugins)
```

- [ ] **Step 5: Добавить тестовый CLI**

`exteraless/tests/plugins/CMakeLists.txt`:

```cmake
add_executable(exteraless-plugins-scan EXCLUDE_FROM_ALL scan_cli.cpp)

target_link_libraries(exteraless-plugins-scan
PRIVATE
    exteraless
)
```

`scan_cli.cpp`: `QCoreApplication`, ровно один аргумент-каталог (иначе exit code 2), вызвать `ScanPlugins`, напечатать в stdout JSON вида `{"error": "<global>", "entries": [ ... ]}` (поля записи те же, `permissions` — массив), вернуть 0. Строки собирать через `QStringLiteral`; `_q` в этой цели недоступен.

- [ ] **Step 6: Добавить фикстуры**

`valid.plugin`:

```python
__id__ = "demo_plugin"
__name__ = "Demo Plugin"
__description__ = "Shows a greeting."
__author__ = "Exteraless"
__version__ = "1.0.0"
__permissions__ = ["ui", "messages.read"]

def greet():
    return "hello"
```

`dynamic.plugin`:

```python
__id__ = "dynamic_plugin"
__name__ = "Dynamic " + "Plugin"
__version__ = "1.0.0"
```

`unknown-permission.plugin`:

```python
__id__ = "bad_permission"
__name__ = "Bad Permission"
__permissions__ = ["ui", "root"]
```

`broken.plugin`:

```python
__id__ = "broken"
__name__ = "Broken"
def broken(:
```

`invalid-id.plugin`:

```python
__id__ = "1bad"
__name__ = "Invalid Id"
```

`legacy.elyx`: любое содержимое, например `__id__ = "legacy"`.
`notes.txt`: произвольный текст; должен игнорироваться.

Ожидаемый результат:

| file | ожидание |
| --- | --- |
| `valid.plugin` | `error == ""`, `id == "demo_plugin"`, permissions `ui`, `messages.read` |
| `dynamic.plugin` | `Metadata __name__ is not a literal.` |
| `unknown-permission.plugin` | `Unknown permission: root.` |
| `broken.plugin` | начинается с `Syntax error:` |
| `invalid-id.plugin` | `Invalid id: 1bad.` |
| `legacy.elyx` | `Unsupported plugin format: .elyx.` |
| `notes.txt` | отсутствует в `entries` |

- [ ] **Step 7: Переконфигурировать CMake**

```bash
nix develop --command cmake -S . -B out
```

- [ ] **Step 8: Скомпилировать новые TU и CLI**

```bash
nix develop --command ninja -C out \
  exteraless/CMakeFiles/exteraless.dir/plugins/scanner.cpp.o
nix develop --command cmake --build out --target exteraless-plugins-scan -j2
```

- [ ] **Step 9: Backend smoke по фикстурам**

```bash
out/exteraless/tests/plugins/exteraless-plugins-scan \
  exteraless/tests/plugins/fixtures > /tmp/opencode/plugins-scan.json
python3 - <<'EOF'
import json

data = json.load(open('/tmp/opencode/plugins-scan.json'))
assert data['error'] == '', data['error']
entries = {entry['file']: entry for entry in data['entries']}
assert set(entries) == {
    'valid.plugin',
    'dynamic.plugin',
    'unknown-permission.plugin',
    'broken.plugin',
    'invalid-id.plugin',
    'legacy.elyx',
}, sorted(entries)
valid = entries['valid.plugin']
assert valid['id'] == 'demo_plugin', valid
assert valid['name'] == 'Demo Plugin', valid
assert valid['version'] == '1.0.0', valid
assert valid['author'] == 'Exteraless', valid
assert valid['permissions'] == ['ui', 'messages.read'], valid
assert valid['error'] == '', valid
assert 'not a literal' in entries['dynamic.plugin']['error']
assert 'Unknown permission: root' in entries['unknown-permission.plugin']['error']
assert entries['broken.plugin']['error'].startswith('Syntax error:')
assert 'Invalid id: 1bad' in entries['invalid-id.plugin']['error']
assert 'Unsupported plugin format: .elyx' in entries['legacy.elyx']['error']
print('backend fixtures OK')
EOF
```

- [ ] **Step 10: Негативные проверки лимита и fallback'ов**

Размер 2 MiB:

```bash
rm -rf /tmp/opencode/plugins-oversize && mkdir -p /tmp/opencode/plugins-oversize
python3 - <<'EOF'
path = '/tmp/opencode/plugins-oversize/too-big.plugin'
with open(path, 'w') as file:
    file.write('__id__ = "too_big"\n__name__ = "Too Big"\n')
    file.write('#' * (2 * 1024 * 1024))
EOF
out/exteraless/tests/plugins/exteraless-plugins-scan /tmp/opencode/plugins-oversize \
  | python3 -c 'import json,sys; e=json.load(sys.stdin)["entries"][0]; assert "larger than 2 MiB" in e["error"], e; print("size limit OK")'
```

Нет `python3`:

```bash
env -i HOME=/tmp/opencode PATH=/nonexistent \
  "$PWD"/out/exteraless/tests/plugins/exteraless-plugins-scan \
  "$PWD"/exteraless/tests/plugins/fixtures \
  | python3 -c 'import json,sys; d=json.load(sys.stdin); assert "not found" in d["error"], d; print("missing python OK")'
```

Невалидный JSON и ненулевой exit:

```bash
PY=$(command -v python3)
rm -rf /tmp/opencode/fake-python && mkdir -p /tmp/opencode/fake-python/bin
printf '#!/bin/sh\nprintf "not json"\n' > /tmp/opencode/fake-python/bin/python3
chmod +x /tmp/opencode/fake-python/bin/python3
PATH=/tmp/opencode/fake-python/bin:$PATH \
  out/exteraless/tests/plugins/exteraless-plugins-scan \
  exteraless/tests/plugins/fixtures \
  | "$PY" -c 'import json,sys; d=json.load(sys.stdin); assert "invalid output" in d["error"], d; print("bad json OK")'
printf '#!/bin/sh\necho boom >&2\nexit 1\n' > /tmp/opencode/fake-python/bin/python3
PATH=/tmp/opencode/fake-python/bin:$PATH \
  out/exteraless/tests/plugins/exteraless-plugins-scan \
  exteraless/tests/plugins/fixtures \
  | "$PY" -c 'import json,sys; d=json.load(sys.stdin); assert "boom" in d["error"], d; print("exit code OK")'
```

Таймаут (ожидать ~10 секунд):

```bash
PY=$(command -v python3)
printf '#!/bin/sh\nsleep 30\n' > /tmp/opencode/fake-python/bin/python3
chmod +x /tmp/opencode/fake-python/bin/python3
time PATH=/tmp/opencode/fake-python/bin:$PATH \
  out/exteraless/tests/plugins/exteraless-plugins-scan \
  exteraless/tests/plugins/fixtures \
  | "$PY" -c 'import json,sys; d=json.load(sys.stdin); assert "timed out" in d["error"], d; print("timeout OK")'
```

---

### Task 2: Plugins box, settings entry and CMake wiring

**Files:**
- Create: `Telegram/SourceFiles/exteraless/plugins_box.h`
- Create: `Telegram/SourceFiles/exteraless/plugins_box.cpp`
- Modify: `Telegram/CMakeLists.txt`
- Modify: `Telegram/SourceFiles/settings/sections/settings_exteraless.cpp`

**Interfaces:**
- Consumes: `Exteraless::Plugins::ScanPlugins`.
- Produces: `Exteraless::ShowPluginsBox(not_null<Window::SessionController*>)`.

- [ ] **Step 1: Добавить заголовок бокса**

```cpp
#pragma once

#include "base/basic_types.h"

namespace Window {
class SessionController;
} // namespace Window

namespace Exteraless {

void ShowPluginsBox(not_null<Window::SessionController*> controller);

} // namespace Exteraless
```

- [ ] **Step 2: Реализовать `plugins_box.cpp`**

```cpp
#include "exteraless/plugins_box.h"

#include "exteraless/plugins/scanner.h"
#include "core/file_utilities.h"
#include "settings.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"

#include <QDir>
#include <rpl/rpl.h>

namespace Exteraless {
namespace {

[[nodiscard]] QString PluginsDirectory() {
	return cWorkingDir() + u"tdata/plugins"_q;
}

[[nodiscard]] QString RowText(const Plugins::Entry &entry) {
	auto text = entry.name.isEmpty() ? entry.file : entry.name;
	if (!entry.version.isEmpty()) {
		text += u" "_q + entry.version;
	}
	if (!entry.author.isEmpty()) {
		text += u"\n"_q + entry.author;
	}
	text += u"\npermissions: "_q
		+ (entry.permissions.isEmpty()
			? u"none"_q
			: entry.permissions.join(u", "_q));
	if (!entry.error.isEmpty()) {
		text += u"\nerror: "_q + entry.error;
	}
	return text;
}

} // namespace

void ShowPluginsBox(not_null<Window::SessionController*> controller) {
	const auto directory = PluginsDirectory();
	QDir().mkpath(directory);
	const auto result = Plugins::ScanPlugins(directory);
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(u"Plugins (%1)"_q.arg(int(result.entries.size())));
		const auto layout = box->verticalLayout();
		if (!result.error.isEmpty()) {
			layout->add(
				object_ptr<Ui::FlatLabel>(
					box,
					rpl::single(result.error),
					st::boxLabel),
				st::boxRowPadding);
		} else if (result.entries.empty()) {
			layout->add(
				object_ptr<Ui::FlatLabel>(
					box,
					rpl::single(
						u"No plugins in "_q + directory + u"."_q),
					st::boxLabel),
				st::boxRowPadding);
		} else {
			for (const auto &entry : result.entries) {
				layout->add(
					object_ptr<Ui::FlatLabel>(
						box,
						rpl::single(RowText(entry)),
						st::boxLabel),
					st::boxRowPadding);
			}
		}

		box->addLeftButton(rpl::single(u"Open folder"_q), [=] {
			QDir().mkpath(directory);
			File::Launch(directory);
		});
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

} // namespace Exteraless
```

Сканирование синхронное и выполняется один раз при открытии бокса; худший случай — таймаут сканера. Это осознанное ограничение этапа.

- [ ] **Step 3: Добавить кнопку в настройки Exteraless**

В `settings_exteraless.cpp` подключить `#include "exteraless/plugins_box.h"` и после подсекции backup добавить:

```cpp
	builder.addSubsectionTitle({
		.id = u"exteraless/plugins"_q,
		.title = rpl::single(u"Plugins"_q),
	});

	builder.addButton({
		.id = u"exteraless/plugins/open"_q,
		.title = rpl::single(u"Plugins"_q),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			if (!controller) {
				return;
			}
			Exteraless::ShowPluginsBox(controller);
		},
		.keywords = { u"plugins"_q, u"plugin"_q },
	});
```

`controller` — уже существующая переменная `const auto controller = builder.controller();` перед backup-кнопками; если её область видимости не покрывает новый блок, объявить кнопку внутри того же блока после backup.

- [ ] **Step 4: Добавить бокс в CMake Telegram**

В `Telegram/CMakeLists.txt` в список `target_sources(Telegram PRIVATE ...)` добавить:

```cmake
    SourceFiles/exteraless/plugins_box.cpp
    SourceFiles/exteraless/plugins_box.h
```

- [ ] **Step 5: Переконфигурировать и скомпилировать точечно**

```bash
nix develop --command cmake -S . -B out
nix develop --command ninja -C out \
  Telegram/CMakeFiles/Telegram.dir/SourceFiles/exteraless/plugins_box.cpp.o \
  Telegram/CMakeFiles/Telegram.dir/SourceFiles/settings/sections/settings_exteraless.cpp.o
```

- [ ] **Step 6: Собрать приложение**

```bash
nix develop --command cmake --build out --target Telegram -j2
```

Если финальный линк падает на известном GNU ld debug-info overflow (>4 GiB), выполнить один LLD-релинк, не меняя закоммиченный конфиг. Нужны оба флага: `-fuse-ld=lld` и `-Wl,-S`, потому что без strip переполняются и GNU ld, и LLD:

```bash
nix develop --command bash -c '
  lld=$(nix build --no-link --print-out-paths nixpkgs#lld)
  link=$(ninja -C out -t commands bin/Telegram | tail -1)
  PATH="$lld/bin:$PATH" bash -c "${link/g++/g++ -fuse-ld=lld -Wl,-S}"
'
```

Записать в отчёт, что линк выполнен LLD со strip debug-info.

- [ ] **Step 7: UI smoke с непустым каталогом**

```bash
export SMOKE=/tmp/opencode/plugins-ui
export DISPLAY=:99
rm -rf "$SMOKE" "$SMOKE-cache"
mkdir -p "$SMOKE" "$SMOKE-cache"
cp -a /tmp/opencode/smoke-drawer5/tdata "$SMOKE/tdata"
mkdir -p "$SMOKE/tdata/plugins"
cp exteraless/tests/plugins/fixtures/* "$SMOKE/tdata/plugins/"
pgrep -f "Xvfb :99" >/dev/null || \
  (Xvfb :99 -screen 0 1280x800x24 >/tmp/opencode/xvfb99-plugins.log 2>&1 &)
XDG_DATA_HOME="$SMOKE" XDG_CACHE_HOME="$SMOKE-cache" \
  QT_QPA_PLATFORM=xcb out/bin/Telegram -workdir "$SMOKE" -noupdate \
  >"$SMOKE/app.log" 2>&1 &
echo $! > "$SMOKE/app.pid"
sleep 12
python3 /tmp/opencode/xinput.py click 45 65
sleep 1
import -window root /tmp/opencode/plugins-ui-01-menu.png
python3 /tmp/opencode/xinput.py click 87 541
sleep 2
import -window root /tmp/opencode/plugins-ui-02-settings.png
for i in $(seq 1 15); do
  python3 /tmp/opencode/xinput.py click 640 400 5
  sleep 0.2
done
import -window root /tmp/opencode/plugins-ui-03-bottom.png
```

Координаты `(87, 541)` — пункт «Exteraless» в drawer на 1280x800 (baseline `final-smoke-02-drawer-first-open.png`); если профиль другой, скорректировать по скриншоту `plugins-ui-01-menu.png`. После скролла найти строку «Plugins» на `plugins-ui-03-bottom.png` и кликнуть её, затем снять `plugins-ui-04-box.png`. Проверить на скриншоте: заголовок `Plugins (6)`, строки `Demo Plugin 1.0.0`, `Exteraless`, `permissions: ui, messages.read`, а также `error: ...` у dynamic/unknown-permission/broken/invalid-id/legacy; кнопки «Open folder» и «Close»; никаких install/toggle/run.

Остановка только записанного PID:

```bash
kill "$(cat "$SMOKE/app.pid")" 2>/dev/null || true
sleep 2
```

- [ ] **Step 8: UI smoke пустого состояния**

```bash
rm -rf "$SMOKE/tdata/plugins" && mkdir -p "$SMOKE/tdata/plugins"
XDG_DATA_HOME="$SMOKE" XDG_CACHE_HOME="$SMOKE-cache" \
  QT_QPA_PLATFORM=xcb out/bin/Telegram -workdir "$SMOKE" -noupdate \
  >"$SMOKE/app-empty.log" 2>&1 &
echo $! > "$SMOKE/app.pid"
sleep 12
python3 /tmp/opencode/xinput.py click 45 65
sleep 1
python3 /tmp/opencode/xinput.py click 87 541
sleep 2
for i in $(seq 1 15); do
  python3 /tmp/opencode/xinput.py click 640 400 5
  sleep 0.2
done
```

Кликнуть «Plugins» и снять `plugins-ui-05-empty.png`; ожидать `Plugins (0)` и `No plugins in /tmp/opencode/plugins-ui/tdata/plugins.`

```bash
kill "$(cat "$SMOKE/app.pid")" 2>/dev/null || true
```

- [ ] **Step 9: Проверить, что плагин не исполнялся**

Добавить в `$SMOKE/tdata/plugins/exec-check.plugin` маркер:

```python
__id__ = "exec_check"
__name__ = "Exec Check"
open("/tmp/opencode/plugin-executed", "w").write("bad")
```

Перезапустить приложение, открыть бокс, убедиться, что `/tmp/opencode/plugin-executed` не существует, а запись `Exec Check` показана без глобальной ошибки:

```bash
test ! -e /tmp/opencode/plugin-executed && echo "plugin code not executed"
```

---

### Task 3: Docs, patch, flake and final verification

**Files:**
- Modify: `CHANGELOG.md`
- Modify: `STATUS.md`
- Modify: `docs/feature-mapping.md`
- Modify: `Agents/README.md`
- Regenerate: `nix/patches/exteraless.patch`

- [ ] **Step 1: Обновить `CHANGELOG.md`**

В существующую секцию `## 2026-11-04` добавить пункт: каталог `tdata/plugins`, AST-сканер манифестов `.plugin`/`.py` без исполнения кода, лимит 2 MiB, известные разрешения, `.elyx`/`.eaf` как unsupported, бокс «Plugins» и кнопка в настройках Exteraless, тестовый CLI и фикстуры.

- [ ] **Step 2: Обновить `STATUS.md`**

В `### MVP3` оставить общий пункт host-процесса незакрытым и добавить отдельный закрытый подпункт:

```markdown
- [x] Плагины, этап 1: манифест и просмотр (`tdata/plugins`, AST без исполнения, разрешения, ошибки, бокс).
```

В `### Проверки` добавить фактические результаты: backend fixtures/negative, Xvfb UI smoke (непустой и пустой каталог), подтверждение отсутствия исполнения, LLD-релинк при необходимости.

- [ ] **Step 3: Обновить `docs/feature-mapping.md`**

В разделе `## Plugins` заменить «Post-MVP» на описание этапа 1: каталог, расширения, AST-сканер без исполнения, известные разрешения, лимит, бокс, отсутствие install/toggle/execute. Полный host-процесс, аудит и sandbox оставить как будущее.

- [ ] **Step 4: Обновить `Agents/README.md`**

В структуру добавить:

- `exteraless/plugins/scanner.*` — запуск `python3 -I -B` и разбор JSON;
- `Telegram/SourceFiles/exteraless/plugins_box.*` — бокс «Plugins»;
- `exteraless/tests/plugins/` — CLI и фикстуры;
- каталог `cWorkingDir()/tdata/plugins`, `.plugin`/`.py`, `.elyx`/`.eaf` как unsupported, отсутствие исполнения на этапе 1.

- [ ] **Step 5: Перегенерировать патч**

```bash
nix/update-patch.sh
```

Скрипт делает `git add -N exteraless Telegram` и пишет `nix/patches/exteraless.patch`; новых путей вне `exteraless`/`Telegram`/`CMakeLists.txt` нет, менять скрипт не нужно.

- [ ] **Step 6: Dry-run патча на чистом tdesktop 7.3.1**

```bash
check=$(mktemp -d /tmp/opencode/patch-check.XXXXXX)
git archive v7.3.1 | tar -x -C "$check"
git -C "$check" init -q
git -C "$check" apply --check \
  "$PWD"/nix/patches/exteraless.patch
git -C "$check" apply --stat \
  "$PWD"/nix/patches/exteraless.patch \
  | grep -E 'plugins|scanner|tests/plugins'
```

Ожидать успешный `apply --check` и строки для новых файлов. Сам патч не применять.

- [ ] **Step 7: Проверить flake**

```bash
nix flake check --no-build
```

- [ ] **Step 8: Финальная сборка и smoke**

```bash
nix develop --command cmake -S . -B out
nix develop --command cmake --build out --target Telegram -j2
```

Если GNU ld снова упирается в >4 GiB debug-info, повторить LLD-релинк из Task 2 Step 6 и указать это в отчёте. Затем повторить backend smoke (Task 1 Steps 9–10) и UI smoke (Task 2 Steps 7–9) на финальном бинарнике.

- [ ] **Step 9: Независимый review diff**

Передать отдельному агенту `git diff` и `git status` без права редактирования. Проверить: нет исполнения кода плагина, нет новых зависимостей, нет debug-кода, нет commit/push, все новые файлы попали в патч, комментарии не превышают лимит, LF/no-BOM.

- [ ] **Step 10: Итоговый отчёт**

Записать в ответ: пути изменённых файлов, результаты backend/UI smoke, результат `apply --check`, `nix flake check --no-build`, способ финального линка (GNU ld или LLD), известные ограничения (синхронный скан, нет install/toggle/execute).

## Known limitations

- Один бюджет `kScanTimeout = 10000` мс на весь каталог, а не на файл: большой каталог или медленный диск дают глобальный `Plugin scan timed out.` без частичных результатов.
- Строковые метаданные (`__name__`, `__description__`, `__author__`, `__version__`) не санитизируются и не ограничены по длине: управляющие символы и очень длинные значения попадают в UI как есть.
- `.eaf` не покрыт фикстурой, но обрабатывается тем же кодом, что и `.elyx` (`UNSUPPORTED`), поэтому отдельная проверка не заведена.

## Out of Scope

- Установка, удаление, включение/выключение плагинов.
- Запуск кода плагина, host-процесс, IPC, аудит, sandbox.
- Новые ключи конфига и записи в `lang.strings`.
- Кэширование результатов сканирования и watcher каталога.
- Android-совместимость и `from java import ...`.
- Коммиты, push, изменение истории git.
