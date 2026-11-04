# Python Plugin Host (MVP3 Stage 2) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Добавить контролируемый persistent host-процесс: bwrap-песочница запускает `python3 -I -B -c <embedded script> /plugins`, JSON-lines IPC с `seq`/`ok`, `load`/`unload`, keyring join, best-effort audit hook с deny-набором (не security boundary), сохранение включённости в `OEPluginStates` и UI-тумблеры в боксе Plugins.

**Architecture:** C++ singleton `Exteraless::Plugins::Runtime` держит один `QProcess` на всё приложение. Процесс — это `bwrap` с sandbox-флагами, который запускает `python3 -I -B -c <embedded script> /plugins`; shell-обёрток, `unshare`-обёртки и процесса на плагин нет. Плагин загружается только по явному действию пользователя: открытие бокса Plugins загружает ранее включённые плагины, клик по тумблеру включает/выключает конкретный плагин. Сам по себе запуск приложения никогда не стартует host. Python-сторона встраивается строкой в `plugin_runtime.cpp`, до установки audit hook best-effort входит в отдельный session keyring, читает JSON-строки из stdin, пишет ответы/события в stdout, перехватывает опасные audit-события и загружает модуль через `importlib` из уже проверенного файла. C++ дополнительно канонизирует путь, требует прямой дочерний regular file в `cWorkingDir()/tdata/plugins`, расширение `.plugin`/`.py` и отсутствие symlink.

**Tech Stack:** C++20, Qt 6 Core (`QProcess`, `QJsonDocument`, `QFile`), Python 3 stdlib (`importlib`, `json`, `os`, `sys`, `sysconfig`), bubblewrap 0.12 (namespaces, read-only bind mounts, `--clearenv`, `--die-with-parent`), tdesktop 7.3.1, CMake/Ninja, Xvfb.

**Spec:** stage 2 brief из текущей задачи; stage 1: `docs/superpowers/plans/2026-11-04-python-plugin-manifest.md`, `STATUS.md` (MVP3), `docs/feature-mapping.md` (Plugins).

**Status:** Implemented and verified (Task 1 и Task 2, backend + Xvfb smoke). Документ синхронизирован с фактической реализацией: bwrap, keyring join, `seq`/`ok`, пакетный wrapper без дублирования upstream D-Bus substitution, 213/32 проверки. Known Limitations ниже перечисляют фактические границы этапа: audit hook — best-effort и отключается плагином через `__main__`, потоки сейчас разрешены, один host на все плагины, bwrap только Linux, полной сборки Nix-пакета после этапа 2 ещё не было. Чекбоксы ниже сохранены как исходный порядок работ.

## Global Constraints

- Production-правки только в:
  - `exteraless/config.h`
  - `exteraless/config.cpp`
  - `Telegram/SourceFiles/exteraless/plugin_runtime.h`
  - `Telegram/SourceFiles/exteraless/plugin_runtime.cpp`
  - `Telegram/SourceFiles/exteraless/plugins_box.cpp`
  - `Telegram/SourceFiles/core/application.cpp`
  - `Telegram/CMakeLists.txt`
- Тестовый harness только в `exteraless/tests/plugins/` (CLI, runner, фикстуры).
- Документы и инсталляция — только в Task 3: `CHANGELOG.md`, `STATUS.md`, `docs/feature-mapping.md`, `Agents/README.md`, `nix/package.nix`, `flake.nix`, `nix/patches/exteraless.patch`.
- Ключ конфига ровно `OEPluginStates`, объект `id -> true|false`, default `false`. `fromJson` и загрузка файла принимают только id по регэкспу `^[A-Za-z][A-Za-z0-9_-]{1,31}$` и только boolean `true`; false/невалидные записи не сохраняются.
- Каталог плагинов ровно `cWorkingDir() + "tdata/plugins"`; audit-лог ровно `cWorkingDir() + "tdata/plugins-audit.log"`.
- Разрешены только regular files с расширением `.plugin`/`.py`, лежащие **непосредственно** в каталоге плагинов. Canonical path обязателен; symlink, `..`, вложенные каталоги и не-regular files отклоняются.
- Host запускается ровно одним процессом: `bwrap <sandbox args> -- python3 -I -B -c <embedded host script> /plugins`. Никаких shell-обёрток, `sh -c`, `Popen`, `unshare`-обёртки или отдельных процессов на плагин. Внутри sandbox каталог плагинов смонтирован в `/plugins`, поэтому C++ передаёт путь `/plugins/<file>`.
- bwrap-профиль обязателен: `--unshare-all --unshare-user --disable-userns --die-with-parent --new-session --cap-drop ALL --proc /proc --dev /dev --tmpfs /tmp --clearenv`, read-only bind `/` и `/plugins`, read-only `/nix`, `/run/current-system/sw`, `/usr`, `/bin`, `/lib`, `/lib64` и выбранные `/etc`-файлы, `HOME=/tmp`, `TMPDIR=/tmp`, `LANG=C.UTF-8`, минимальный `PATH`. `bwrap` ищется через `EXTERALESS_BWRAP` или `PATH`; если его нет, host не стартует и плагин не исполняется (`Plugin sandbox is unavailable.`).
- Все синхронные запросы к host ограничены `kCommandTimeout = 10000` мс; старт — `5000` мс; kill/wait — `1000` мс; stop — `3000` мс.
- Audit hook — best-effort policy layer с deny-набором, не security boundary: network (`socket.*`), subprocess/`os.system`/`exec`/`fork`/spawn, `ctypes.*`, `mmap`, `sys.addaudithook`, `gc.get_objects`, `fcntl`, `input`, `chdir`, запись вне каталога плагинов, чтение вне каталога плагинов и Python stdlib, `/proc/keys` и прочие keyring-чтения. Плагин может полностью отключить hook: host-скрипт исполняется как `__main__`, и `import __main__; __main__._IN_AUDIT[0] = True` выключает все проверки. Потоки сейчас разрешены: `threading.Thread` не audit-событие, а Python 3.14 стартует их через `_thread.start_joinable_thread`, которого нет в deny-наборе; запись `threading.Thread` — мёртвая, запрещён только прямой `_thread.start_new_thread`.
- Session keyring: до установки hook host best-effort вызывает `KEYCTL_JOIN_SESSION_KEYRING` через `ctypes` (с fallback на номер syscall по архитектуре) и закрывает оставшиеся fd от `libffi`/`_ctypes`; это снижает наследование host keyring, но не является полной keyring-изоляцией.
- `exec`/`compile` разрешены только для загрузки самого plugin-модуля и импорта stdlib; пользовательский `exec`/`eval`/`compile` с произвольным кодом запрещён.
- Никакого авто-запуска при старте приложения. Host не создаётся, пока пользователь не откроет бокс Plugins (и там есть включённые плагины) или не переключит тумблер.
- Закрытие бокса Plugins **не** останавливает host: включённые плагины продолжают работать. `Runtime::Stop()` вызывается при завершении приложения и убивает host.
- Manifest permissions (`network`, `native`, `files` и т.д.) в stage 2 информационные: audit policy одинакова для всех, permissions не дают дополнительных прав.
- Python audit hook — policy layer; bwrap — фактическая граница (namespaces, read-only mounts, no network, dropped capabilities). Stage 2 не обещает полную OS-изоляцию: нет seccomp/landlock, `/nix` доступен read-only, keyring join best-effort, hook можно выключить через `__main__`, потоки разрешены. Плагин с нативным кодом или найденным fd может обойти audit hook, но остаётся внутри bwrap. Поэтому deny-by-default — это про набор известных audit-событий, а не заявление о security boundary.
- Никаких новых library-зависимостей, `lang.strings`, `#ifdef _DEBUG`, debug-полей и тестовых seam'ов в production. `bwrap`/`python3` — runtime-инструменты, попадают в `PATH` через `wrapProgram` в `nix/package.nix`; `flake.nix` добавляет их в devShell. Только Debug-сборка, параллелизм не выше `-j2`.
- Тексты — LF, UTF-8 без BOM. Комментарии — максимум одна строка и только «почему».
- Не выполнять `git commit`, `git push`, `git reset`, `git rebase`; `git add -N` допустим только внутри `nix/update-patch.sh`.
- Smoke-тесты только с изолированными `-workdir`, `XDG_DATA_HOME`, `XDG_CACHE_HOME`; реальный профиль `~/.local/share/ExteralessDesktop` не трогать; останавливать только записанный PID.

## File Structure

| File | Responsibility |
| --- | --- |
| `exteraless/config.{h,cpp}` | `OEPluginStates`, sanitization, default false, persistence. |
| `Telegram/SourceFiles/exteraless/plugin_runtime.{h,cpp}` | Singleton Runtime, bwrap + QProcess host, JSON-lines, path validation, keyring join, audit log, embedded Python host. |
| `Telegram/SourceFiles/exteraless/plugins_box.cpp` | Rows with toggles/details, auto-load after box shows, status text, Open folder/Close. |
| `Telegram/SourceFiles/core/application.cpp` | `Runtime::Stop()` on application shutdown. |
| `Telegram/CMakeLists.txt` | Add runtime to the Telegram target. |
| `exteraless/tests/plugins/plugin_host_cli.cpp` | Test-only CLI over Config/Runtime. |
| `exteraless/tests/plugins/run_host_checks.py` | Backend regression runner. |
| `exteraless/tests/plugins/fixtures/runtime/` | Valid and malicious plugin fixtures. |

## Design Decisions

### D1. Автозагрузка только при открытии бокса, никогда при старте приложения

`ShowPluginsBox` сначала сканирует каталог (код плагина не исполняется), затем показывает бокс. После `box->showFinishes()` Runtime загружает плагины, у которых `Config::pluginEnabled(id) == true`. Это explicit user gesture: пользователь сам открыл менеджер плагинов. Запуск приложения, восстановление сессии, открытие настроек и любые фоновые пути host не стартуют.

Следствие: после рестарта приложения `OEPluginStates[id] == true` означает «включён, но не запущен», пока бокс не открыт. В боксе до загрузки строка показывает `Enabled (not running)`, после успешной загрузки — `Enabled (running)`. Это же поведение проверяется тестом «state persistence across restart».

### D2. Закрытие бокса не останавливает плагины, завершение приложения — останавливает

Плагины — фоновая функциональность; останавливать их при закрытии окна менеджера бессмысленно. `Runtime` живёт до `Application::~Application`, где вызывается `Exteraless::Plugins::Runtime::Stop()`. Если приложение убито, bwrap получает SIGKILL по `--die-with-parent` и уносит свой PID namespace; EOF на stdin остаётся штатным путём завершения. Явный `shutdown` вызывает `on_unload` для всех загруженных плагинов; при зависании C++ убивает процесс через 3 секунды.

### D3. Тумблер отражает preference, статус отдельно отражает runtime

`OEPluginStates[id]` — это preference пользователя, а не факт «процесс сейчас загружен».

- Toggle ON: сначала вызывается `Runtime::enable(id, path)`, затем `Config::setPluginEnabled(id, true)`. Даже при ошибке preference остаётся `true`, строка показывает `Enabled (not running): <error>`. При следующем открытии бокса будет повторная попытка.
- Toggle OFF: вызывается `Runtime::disable(id)`, затем `Config::setPluginEnabled(id, false)`. Даже если host уже умер, preference сбрасывается.
- Ошибка загрузки не сбрасывает preference молча.
- Сканерные записи с `error` не получают тумблер и никогда не загружаются.

### D4. Один host, рестарт после смерти или таймаута

`Runtime` хранит один `QProcess` (это `bwrap`, который запускает Python) и `QHash<id, canonicalPath>` загруженных плагинов. `enable` идемпотентен: если тот же id уже загружен из того же canonical path и процесс жив, повторного `on_load` нет. Если процесс умер, `_loaded` очищается, следующий `enable` стартует новый host. Таймаут любого запроса убивает host и очищает все загруженные плагины: partial state после зависшего `on_load` не сохраняется.

### D5. Путь проверяется дважды: C++ и host

C++ проверяет путь перед отправкой. Host повторно проверяет root, parent, расширение, `O_NOFOLLOW`, `fstat` и размер 2 MiB, читает исходник из уже открытого fd. Это уменьшает TOCTOU-окно, но не устраняет гонку полностью; остаточный риск фиксируется в Review Focus.

### D6. Audit hook и bwrap: policy layer и фактическая граница

Hook ставится до загрузки любого плагина. Он пишет denied-события в audit-лог и бросает `PermissionError`. Для `load`/`unload`/`log` используется JSON-lines протокол. `sys.stdout`, `sys.stderr` и `sys.stdin` перенаправляются, чтобы `print` плагина не ломал протокол; fd 0/1 заменяются на `/dev/null`, а протокол идёт через сохранённые dup-fd. Host закрывает унаследованные fd (`os.closerange(3, ...)`) и ставит meta-path guard против импорта из каталогов вне read roots.

Фактическая граница — bwrap: отдельные user/pid/net/mount/ipc/uts/cgroup namespaces, `--disable-userns`, `--die-with-parent`, `--new-session`, сброс capabilities, read-only `/` и `/plugins`, скрытый HOME, минимальный environment и минимальный `PATH`. Audit hook остаётся policy layer: нативный код, найденные fd, неаудируемые C-пути и filename-spoofing для stdlib-исключений `compile`/`exec` могут его обойти, но остаются внутри bwrap. Отдельно: host-скрипт исполняется в `__main__`, поэтому плагин может импортировать `__main__` и выставить `_IN_AUDIT[0] = True`, полностью отключив hook (проверено: после этого создаётся socket); потоки сейчас разрешены, потому что `threading.Thread` в Python 3.14 идёт через `_thread.start_joinable_thread` вне deny-набора. Все известные векторы из требований покрываются тестами, но эти два остаются известными дырами policy layer.

### D7. Permissions не являются capability

Stage 2 не выдаёт `network`/`files`/`native` по манифесту. Все плагины получают один best-effort deny-набор audit-событий, который не является security boundary и может быть отключён через `__main__`. UI показывает permissions как информацию и явно пишет, что в этом этапе они не дают прав.

### D8. Протокол валидируется по `seq` и `ok`

Каждый запрос несёт `seq`; ответ обязан вернуть тот же `seq` и `ok: true` для `loaded`/`unloaded`. `audit`/`log` принимаются только с текущим `seq`, устаревшие отбрасываются с `qWarning`. Отсутствующий/чужой `seq`, `ok: false` или неизвестный тип считаются protocol violation: host убивается, все плагины выгружаются, следующий `enable` стартует новый host. Это закрывает фикстуры `protocol-missing-seq`, `protocol-ok-false`, `protocol-wrong-seq` и `protocol-unsolicited-log`.

### D9. Session keyring отделяется best-effort

`bwrap --unshare-all` не создаёт отдельный session keyring, поэтому host до установки audit hook вызывает `KEYCTL_JOIN_SESSION_KEYRING` (libc `keyctl`, иначе syscall по номеру архитектуры) и закрывает fd, оставленные `libffi`/`_ctypes`. Плагину чтение `/proc/keys` запрещено audit hook'ом. Это уменьшает доступ к host keyring, но не является полной keyring-изоляцией.

## Review Focus

1. **Протокол ломается от `print` плагина.** Плагин печатает в stdout во время `on_load`; host обязан перенаправить stdout/stderr, а C++ — либо получить корректный `loaded`, либо явную ошибку, но не hang и не мусорный JSON.
2. **Зависание/смерть плагина.** `while True: pass` или `os._exit(1)` не должны вешать UI дольше 10 секунд; host убивается, все загруженные плагины очищаются, следующий `enable` стартует новый процесс.
3. **Path traversal и symlink.** Файл вне `tdata/plugins`, symlink, вложенный путь, `..`, неверное расширение и подмена файла не должны загружаться; host повторно проверяет путь и открывает с `O_NOFOLLOW`.
4. **Audit bypass и bwrap boundary.** `socket`, `subprocess`, `os.system`, `ctypes`, `exec`, запись маркера вне plugin dir, импорт модуля из `tdata` и чтение `/proc/keys` должны давать denied-события и не исполнять побочный эффект. Отдельно проверяется, что bwrap действительно скрывает HOME, репозиторий и `tdata`, оставляет read-only `/` и `/plugins`, не даёт сеть и mount/remount, а host входит в отдельный session keyring; при этом hook — policy layer, а не полная OS-изоляция. Отдельно фиксируются известные обходы: `import __main__; __main__._IN_AUDIT[0] = True` выключает hook, а `threading.Thread` (Python 3.14, `_thread.start_joinable_thread`) потоки не запрещает.
5. **Скрытое исполнение при старте.** При `OEPluginStates[id] == true` запуск приложения без открытия Plugins не создаёт host и не исполняет код плагина; host появляется только после открытия бокса или клика по тумблеру.

---

### Task 1: Config state, host runtime, embedded Python host and malicious backend tests

**Files:**
- Modify: `exteraless/config.h`
- Modify: `exteraless/config.cpp`
- Create: `Telegram/SourceFiles/exteraless/plugin_runtime.h`
- Create: `Telegram/SourceFiles/exteraless/plugin_runtime.cpp`
- Modify: `Telegram/CMakeLists.txt`
- Create: `exteraless/tests/plugins/plugin_host_cli.cpp`
- Modify: `exteraless/tests/plugins/CMakeLists.txt`
- Create: `exteraless/tests/plugins/run_host_checks.py`
- Create: `exteraless/tests/plugins/fixtures/runtime/valid.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/valid-second.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/deny-write.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/deny-socket.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/deny-subprocess.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/deny-ctypes.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/deny-exec.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/deny-import.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/print-noise.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/infinite-loop.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/crash.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/missing-base.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/id-mismatch.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/sandbox-observe.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/keyring-observe.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/protocol-missing-seq.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/protocol-ok-false.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/protocol-wrong-seq.plugin`
- Create: `exteraless/tests/plugins/fixtures/runtime/protocol-unsolicited-log.plugin`

**Interfaces:**

- Consumes: stage 1 scanner (`Exteraless::Plugins::Entry`, `ScanPlugins`) и `Exteraless::Config`.
- Produces (Config):

```cpp
[[nodiscard]] bool pluginEnabled(const QString &id) const;
void setPluginEnabled(const QString &id, bool enabled);
[[nodiscard]] QJsonObject pluginStates() const;
```

- Produces (runtime):

```cpp
namespace Exteraless::Plugins {

struct Result {
	bool ok = false;
	QString error;
};

struct PathCheck {
	bool ok = false;
	QString canonicalPath;
	QString error;
};

class Runtime final {
public:
	Runtime(QString pluginsDirectory, QString auditLogPath);
	~Runtime();

	Runtime(const Runtime &) = delete;
	Runtime &operator=(const Runtime &) = delete;

	static void Start(QString pluginsDirectory, QString auditLogPath);
	[[nodiscard]] static Runtime *Instance();
	static void Stop();

	[[nodiscard]] Result enable(const QString &id, const QString &path);
	[[nodiscard]] Result disable(const QString &id);
	void stop();

	[[nodiscard]] bool isRunning() const;
	[[nodiscard]] bool isLoaded(const QString &id) const;
	[[nodiscard]] qint64 hostPid() const;
	[[nodiscard]] const QString &pluginsDirectory() const;

	[[nodiscard]] static PathCheck ValidatePath(
		const QString &pluginsDirectory,
		const QString &path);
};

} // namespace Exteraless::Plugins
```

- Produces (host protocol, UTF-8 JSON lines, one object per line):

```text
C++ -> host:
  {"seq":1,"cmd":"load","id":"demo_runtime","path":"/plugins/demo_runtime.plugin"}
  {"seq":2,"cmd":"unload","id":"demo_runtime"}
  {"seq":3,"cmd":"shutdown"}

host -> C++:
  {"type":"ready","api":1}
  {"seq":1,"type":"loaded","ok":true}
  {"seq":1,"type":"error","message":"..."}
  {"seq":2,"type":"unloaded","ok":true}
  {"seq":2,"type":"unloaded","ok":true,"warning":"..."}
  {"seq":3,"type":"bye"}
  {"seq":1,"type":"audit","id":"demo_runtime","event":"socket.connect","detail":"..."}
  {"seq":1,"type":"log","id":"demo_runtime","message":"..."}
```

  Ответы `loaded`/`unloaded` обязаны нести `seq` исходного запроса и `ok: true`; отсутствующий/чужой `seq` и `ok: false` — protocol violation с убийством host. `audit`/`log` с устаревшим `seq` отбрасываются.

- Produces (audit log, one JSON object per line):

```json
{"time":"2026-11-04T12:34:56.789Z","kind":"denied","plugin":"demo_runtime","event":"socket.connect","detail":"..."}
{"time":"2026-11-04T12:34:56.789Z","kind":"log","plugin":"demo_runtime","event":"log","detail":"demo loaded"}
```

- Produces (test CLI):

```text
out/exteraless/tests/plugins/exteraless-plugin-host-cli <pluginsRoot> <auditLog> <command> [args...]

validate <path>
enable <id> <path>
disable <id>
stop
state-get <id>
state-set <id> <true|false>
import-state <json-file>
multi <json-array>
```

  CLI всегда выводит один JSON-объект/массив в stdout и возвращает `0` даже при `ok:false`; `2` — только за ошибку использования. `multi` выполняет операции в одном `Runtime` и после каждой добавляет `pid` и `loaded`.

- [ ] **Step 1: Добавить фикстуры и failing runner**

  `fixtures/runtime/valid.plugin`:

```python
from base_plugin import BasePlugin

__id__ = "demo_runtime"
__name__ = "Demo Runtime"
__version__ = "1.0.0"
__permissions__ = ["ui"]

class Demo(BasePlugin):
    def on_load(self):
        self.log("demo loaded")

    def on_unload(self):
        self.log("demo unloaded")

plugin = Demo()
```

  `valid-second.plugin` — тот же контракт с `__id__ = "demo_second"` и `self.log("second loaded")`. Каждая фикстура обязана начинаться с `from base_plugin import BasePlugin`, нужных импортов (`os`, `socket`, `subprocess`, `ctypes`, `sys`) и валидных `__id__`/`__name__` с известными permissions, иначе scanner не покажет её в UI и тест загрузит не тот файл.

  `deny-write.plugin` пытается записать маркер **вне** корня плагинов:

```python
class DenyWrite(BasePlugin):
    def on_load(self):
        target = os.path.join(os.path.dirname(__file__), "..", "marker-denied")
        try:
            with open(target, "w") as marker:
                marker.write("bad")
        except PermissionError:
            self.log("write denied")
```

  `deny-socket.plugin` создаёт `socket.socket()` и пытается `connect(("127.0.0.1", 9))`, ловит `PermissionError`/`OSError` и логирует `socket denied`. `deny-subprocess.plugin` вызывает `subprocess.run(["sh", "-c", "echo bad"])`, ловит `PermissionError`, логирует `subprocess denied`. `deny-ctypes.plugin` вызывает `ctypes.CDLL(None)`, ловит `PermissionError`, логирует `ctypes denied`. `deny-exec.plugin` вызывает `exec("open(...)", {})`, ловит `PermissionError`, логирует `exec denied`. `deny-import.plugin` добавляет `os.path.dirname(os.path.dirname(__file__))` в `sys.path` (в UI это `tdata`, в backend temp root — корень work-каталога), пытается `import evilmod`, ловит `PermissionError`, логирует `import denied`; runner кладёт `evilmod.py` именно в этот родительский каталог.

  `print-noise.plugin` печатает `noise from plugin` в `on_load` и логирует `print survived`. `infinite-loop.plugin` делает `while True: pass` в `on_load`. `crash.plugin` вызывает `os._exit(1)` в `on_load`. `missing-base.plugin` не объявляет ни instance, ни subclass. `id-mismatch.plugin` объявляет `__id__ = "other_id"`, но получает id `demo_runtime`.

  `sandbox-observe.plugin` логирует uid, наличие `/home` и `/plugins`, `PYTHONPATH` и cwd; `keyring-observe.plugin` пытается прочитать `/proc/keys`, ловит отказ и логирует `keyring read denied`. `protocol-missing-seq.plugin`, `protocol-ok-false.plugin` и `protocol-wrong-seq.plugin` отвечают на `load` без `seq`, с `ok:false` и с чужим `seq`; `protocol-unsolicited-log.plugin` отправляет log со старым `seq` перед корректным `loaded`.

  `run_host_checks.py` должен проверять как минимум:

  | Проверка | Условие |
  | --- | --- |
  | host script syntax | извлечь `R"PY(... )PY"` из `plugin_runtime.cpp`, `compile(source, "<host>", "exec")` без ошибок |
  | valid load | `ok=true`, `loaded=true`, `pid != 0` |
  | valid log | audit log содержит `kind == "log"` и `detail == "demo loaded"` |
  | unload | `disable` даёт `ok=true`, audit log содержит `demo unloaded` |
  | bwrap profile | найдены обязательные флаги; read-only `/` и `/plugins`, `/nix` read-only, HOME скрыт, `/tmp` writable, capabilities `0`, `no_new_privs`, сеть недоступна, mount/remount denied, репозиторий и `tdata` скрыты, cwd `/` |
  | bwrap unavailable | без bwrap `enable` даёт `ok=false`, ровно `Plugin sandbox is unavailable.`, `pid=0`, плагин не исполняется |
  | bwrap override | `EXTERALESS_BWRAP` указывает на рабочий bwrap; невалидный путь fail closed |
  | bwrap failure | подменённый bwrap с ошибкой не запускает плагин и не оставляет host |
  | keyring join | join вызывается до hook и загрузки плагина, join succeeds, session keyring отличается от host, host keyring отсутствует в sandbox |
  | keyring plugin | чтение `/proc/keys` даёт denied-событие `open`, плагин логирует `keyring read denied` |
  | write denied | `marker-denied` отсутствует, audit log содержит `kind == "denied"` и `event == "open"` |
  | escape write/xattr | запись и xattr за пределы `/plugins` не меняют host-файлы |
  | fork escape | fork/exec и дочерние маркеры не покидают bwrap |
  | socket denied | audit log содержит событие с `socket` |
  | subprocess denied | audit log содержит `subprocess.Popen` |
  | ctypes denied | audit log содержит `ctypes` |
  | exec/eval denied | audit log содержит `compile` или `exec`, побочный файл не создан |
  | import outside | `evilmod.py` не исполняется, audit log содержит `import`; foreign import даёт точный deny |
  | print isolation | `print-noise` загружается без invalid JSON, шум не попадает в audit log |
  | path outside | `validate` возвращает `ok=false`, ошибка содержит `outside` |
  | symlink | `validate` возвращает `ok=false`, ошибка содержит `symlink` |
  | wrong extension | `validate` возвращает `ok=false`, ошибка содержит `Unsupported plugin format` |
  | nested path | `validate` возвращает `ok=false`, ошибка содержит `directly inside` |
  | missing file | `validate` возвращает `ok=false`, ошибка содержит `missing` |
  | canonical plugins root | symlink на корень плагинов резолвится, загрузка проходит |
  | protocol seq/ok | ответ без `seq`, с чужим `seq` или `ok:false` даёт `Plugin host protocol violation.`, `pid=0`; unsolicited log отбрасывается, следующий плагин загружается в новом host |
  | fd hygiene | унаследованный fd закрыт, открыты только 0–4, shutdown чистый |
  | timeout | `infinite-loop` даёт `ok=false`, `timed out`, elapsed `8..20` секунд |
  | restart after timeout | следующий `valid` получает новый `pid != 0` |
  | startup no host | в startup-путях нет `Runtime::Start`/`Runtime::Instance`; `application.cpp` вызывает `Runtime::Stop` при выходе |
  | single host | `multi` с двумя `enable` возвращает одинаковый `pid` |
  | stop | `stop` даёт `pid == 0`, `/proc/<pid>` отсутствует |
  | state default | `state-get unknown_id` даёт `false` |
  | state persistence | `state-set` в одном процессе, `state-get` в другом даёт `true` |
  | config whitelist | `import-state` с валидным true и мусорными id/значениями оставляет только валидный true |
  | missing python | при `PATH=/nonexistent` `enable` даёт ошибку `Python 3 was not found.` |
  | host crash | `crash` даёт `ok=false`, следующий `valid` снова загружается |
  | id mismatch | `id-mismatch` даёт `ok=false`, ошибка содержит `id` |

  Итог: `run_host_checks.py` — 213 проверок (включая bwrap/keyring/escape/protocol), stage 1 `run_checks.py` — 32 проверки.

- [ ] **Step 2: Запустить runner и убедиться, что он падает**

```bash
python3 exteraless/tests/plugins/run_host_checks.py
```

  Ожидается: `FAIL host CLI is missing at .../out/exteraless/tests/plugins/exteraless-plugin-host-cli` и код возврата `2`.

- [ ] **Step 3: Добавить `OEPluginStates` в Config**

  В `config.h` добавить три метода из блока Interfaces после `drawerAccountsExpanded/setDrawerAccountsExpanded`.

  В `config.cpp` (добавить `<QRegularExpression>`):

  - Константа `constexpr auto kPluginStates = "OEPluginStates";`.
  - Helper `IsValidPluginId(const QString&)` с `QRegularExpression(QStringLiteral("^[A-Za-z][A-Za-z0-9_-]{1,31}$"))`.
  - Helper `SanitizePluginStates(const QJsonValue&)`: принимает только object; оставляет только пары, где ключ валиден и значение — boolean `true`; всё остальное отбрасывает.
  - `pluginEnabled(id)` возвращает `false` для невалидного id, иначе `true` только если `SanitizePluginStates(_values.value(kPluginStates))[id]` — boolean `true`.
  - `setPluginEnabled(id, enabled)` валидирует id, берёт `SanitizePluginStates`, добавляет/удаляет id, вызывает `setValue(kPluginStates, states.isEmpty() ? QJsonValue(QJsonValue::Undefined) : QJsonValue(states))`.
  - `pluginStates()` возвращает `SanitizePluginStates(_values.value(kPluginStates))`.
  - В `fromJson` добавить `|| key == kPluginStates` в whitelist и после цикла, если `accepted.contains(kPluginStates)`, заменить `accepted[kPluginStates]` на результат `SanitizePluginStates` (пустой результат — удалить ключ).
  - В `ensureLoaded` после `_values = Unwrap(...)`, если `_values.contains(kPluginStates)`, также применить `SanitizePluginStates` и удалить ключ при пустом результате, чтобы старый/испорченный файл не сохранял мусорные записи.
  - `kPluginStates` не добавлять в `backup.cpp`: backup stage 1 покрывает только три ключа, расширение backup — вне задачи.

- [ ] **Step 4: Добавить `plugin_runtime.h`**

  Заголовок содержит ровно API из блока Interfaces, включает `<QByteArray>`, `<QHash>`, `<QJsonObject>`, `<QString>`, `<memory>`, вперёд объявляет `class QProcess;`, хранит `std::unique_ptr<QProcess> _process`, `QByteArray _readBuffer`, `QHash<QString, QString> _loaded`, `int _seq = 0`, `QString _pluginsDirectory`, `QString _auditLogPath`. Копирование запрещено.

- [ ] **Step 5: Встроить host script в `plugin_runtime.cpp`**

  Скрипт хранить как `constexpr auto kHostScript = R"PY(... )PY";` (без пробела перед `)PY`; разделитель ровно `R"PY(` / `)PY"`). Скрипт обязан:

  1. Импортировать stdlib (`importlib.machinery`, `importlib.util`, `io`, `json`, `os`, `re`, `stat`, `sys`, `sysconfig`, `types`) до установки hook; вычислить `_FD_LIMIT` из `os.sysconf('SC_OPEN_MAX')`.
  2. Сохранить `PROTOCOL = os.fdopen(os.dup(1), "wb", buffering=0)`, `INPUT = os.fdopen(os.dup(0), "rb", buffering=0)`, вызвать `os.closerange(3, _FD_LIMIT)`, открыть `/dev/null`, сделать `dup2` на fd 0/1/2, закрыть исходный devnull-fd. Заменить `sys.stdin` на `io.StringIO()` (плагин читает EOF), а `sys.stdout`/`sys.stderr` — на ограниченный sink-объект с `write()`/`flush()`/`encoding = "utf-8"`, который отбрасывает вывод. Плагинный `print` не должен попадать в протокол, в stderr и не должен накапливаться в памяти. Протокол идёт только через сохранённые dup-fd.
  3. Взять `ROOT = os.path.realpath(sys.argv[1])` (в sandbox это `/plugins`); `STDLIB` и `PLATSTDLIB` — из `sysconfig.get_paths()["stdlib"]`/`["platstdlib"]`; включить также `os.path.dirname(os.__file__)`. НЕ включать `purelib`/`platlib`/site-packages.
  4. До установки hook вызвать `_join_session_keyring()`: best-effort `KEYCTL_JOIN_SESSION_KEYRING` через `ctypes.CDLL(None)`/`keyctl`, иначе `libc.syscall` по номеру архитектуры, затем закрыть оставшиеся fd от `libffi`/`_ctypes`.
  5. Установить `sys.addaudithook(_audit)`, поставить meta-path guard `_ImportGuard` (deny импорта из каталогов вне read roots) и инжектировать `base_plugin`.
  6. Отправить `{"type":"ready","api":1}` и читать stdin в цикле. Каждую строку писать как `json.dumps(..., ensure_ascii=False)` + `\n` в UTF-8 с `errors="replace"`. На EOF выйти без вызова плагинного кода (родитель исчез); на `shutdown` вызвать `on_unload` всех, отправить `bye`, выйти.
  7. `load`: проверить id регэкспом, путь абсолютный, `dirname(realpath(path)) == ROOT`, расширение `.plugin`/`.py`, `not os.path.islink(path)`, `stat.S_ISREG(os.stat(path, follow_symlinks=False).st_mode)`. Открыть `os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_BINARY", 0)`, проверить `fstat` и размер `<= 2 * 1024 * 1024`, прочитать байты из fd. Ошибки — `{"type":"error","message":"..."}`.
  8. Загрузить модуль через `importlib.util.spec_from_file_location` и кастомный `SourceFileLoader`, чей `get_data(path)` возвращает уже прочитанные байты для canonical path, а `get_code(fullname)` не читает файл повторно: декодирует сохранённые байты через `importlib.util.decode_source`, под флагом `_ALLOW_COMPILE[0]` вызывает `compile(source, path, "exec")`, сохраняет code object в `_LOAD_CODE[0]` и возвращает его. Это исключает чтение `.pyc` и повторное открытие пути. Имя модуля — `exteraless_plugin_<id>`. Записать модуль в `sys.modules` на время загрузки; после `exec_module` очистить `_LOAD_CODE[0]`.
  9. Найти plugin: сначала `module.plugin`, если это `BasePlugin`; иначе ровно один `BasePlugin`-instance в `vars(module)`; иначе ровно один subclass `BasePlugin` (не сам `BasePlugin`), создать без аргументов. Больше одного кандидата — ошибка. Проверить `module.__id__ == id`; при несовпадении/отсутствии — ошибка.
  10. До загрузки запомнить `set(sys.modules)`, после `exec_module` собрать новые модули, чей `__file__` под `ROOT`, в `record.modules`. Вызвать `on_load` с `_CURRENT_ID[0] = id`; при исключении попробовать `on_unload`, удалить `record.modules` из `sys.modules`, вернуть `error`. При успехе сохранить record `{module, instance, path, modules}` и вернуть `loaded`.
  11. `unload`: вызвать `on_unload`, перехватить исключение в `warning`, удалить основной модуль и `record.modules`, вернуть `unloaded`.
  12. `BasePlugin` инжектировать как модуль `base_plugin` в `sys.modules` до загрузки плагина:

```python
class BasePlugin:
    def on_load(self):
        pass

    def on_unload(self):
        pass

    def log(self, message):
        _host_log(_CURRENT_ID[0], str(message)[:500])
```

  Audit hook:

```python
DENY_EXACT = {
    "os.system", "os.exec", "os.fork", "os.forkpty", "os.posix_spawn",
    "os.spawn", "os.kill", "os.killpg", "subprocess.Popen", "pty.spawn",
    "socket.__new__", "socket.connect", "socket.bind", "socket.getaddrinfo",
    "socket.gethostbyname", "socket.gethostbyname_ex", "socket.gethostbyaddr",
    "socket.sendto", "socket.sendmsg",
    "sys.addaudithook", "sys._getframe", "sys._current_frames",
    "sys.settrace", "sys.setprofile", "builtins.input", "os.chdir",
    "gc.get_objects", "gc.get_referrers", "gc.get_referents",
    "fcntl.fcntl", "fcntl.ioctl", "mmap.__new__",
    "_thread.start_new_thread", "threading.Thread",
}
DENY_PREFIX = ("ctypes.",)
MUTATING = {
    "os.remove", "os.rename", "os.replace", "os.rmdir", "os.mkdir",
    "os.link", "os.symlink", "os.chmod", "os.chown", "os.truncate",
    "os.utime", "os.mkfifo", "os.mknod",
    "shutil.copyfile", "shutil.copymode", "shutil.copystat",
    "shutil.move", "shutil.rmtree",
}
```

  Правила hook:

  - `DENY_EXACT`, `DENY_PREFIX`, `sys.addaudithook` и любые `ctypes.*` — deny.
  - `open`: если `file` — int fd, deny; иначе определить write по `mode` (`w`, `a`, `x`, `+`) или по `flags` (`O_WRONLY`, `O_RDWR`, `O_CREAT`, `O_TRUNC`, `O_APPEND`). Read разрешён только под `ROOT`/stdlib/platstdlib; write — только под `/plugins`. Поэтому `/proc/keys` и прочие keyring-файлы отклоняются.
  - `os.listdir`/`os.scandir`: path должен быть под read roots.
  - `MUTATING`: все строковые/bytes path-аргументы должны быть под `ROOT`; int fd, `None` и boolean-аргументы игнорировать; иначе deny. `os.setxattr`/`os.removexattr` — write roots, `os.getxattr`/`os.listxattr` — read roots.
  - `import`: `filename is None` или модуль из `sys.builtin_module_names` — разрешить; иначе filename realpath должен быть под read roots. Дополнительно `_ImportGuard` в `sys.meta_path` запрещает модуль, найденный в каталоге вне read roots, даже если audit-событие не пришло.
  - `compile`: разрешить при `_ALLOW_COMPILE[0] == True` (флаг вокруг `loader.get_code`), если `filename` под stdlib/platstdlib, или если `_stdlib_caller()` подтверждает импорт из frozen importlib; иначе deny.
  - `exec`: разрешить только если `args[0] is _LOAD_CODE[0]`, `co_filename` под stdlib/platstdlib или `_stdlib_caller()` подтверждает импорт; иначе deny. Filename-исключение для stdlib является best-effort и может быть spoofed; это записано в Known Limitations.
  - Deny пишет audit-событие через `PROTOCOL` и бросает `PermissionError("Exteraless audit: <event> denied")`.

  `_CURRENT_ID`, `_ALLOW_COMPILE`, `_LOAD_CODE`, `_IN_AUDIT` — списки/`threading`-безопасные mutable-ячейки, потому что hook и код плагина работают в одном потоке. В начале `_audit` при `_IN_AUDIT[0]` немедленно вернуться, иначе выставить `True` и снять в `finally`, чтобы внутренние проверки hook не рекурсировали. Любой `OSError` при записи в `PROTOCOL` завершает host через `os._exit(1)`, чтобы не оставить полуживой процесс.

- [ ] **Step 6: Реализовать C++ Runtime**

  Подключить `<QDateTime>`, `<QDir>`, `<QElapsedTimer>`, `<QFile>`, `<QFileInfo>`, `<QJsonDocument>`, `<QJsonObject>`, `<QJsonParseError>`, `<QProcess>`, `<QStandardPaths>`, `<QStringList>`.

  Константы в anonymous namespace `plugin_runtime.cpp`:

```cpp
constexpr auto kStartTimeout = 5000;
constexpr auto kCommandTimeout = 10000;
constexpr auto kWriteTimeout = 2000;
constexpr auto kStopTimeout = 3000;
constexpr auto kKillTimeout = 1000;
constexpr auto kMaxReadBuffer = 1024 * 1024;
```

  `ValidatePath(root, path)`:

  - `QFileInfo(root)` существует и директория; `canonicalRoot = canonicalFilePath()`.
  - `path` непустой, `QFileInfo info(path)` существует, `!info.isSymLink()`, `info.isFile()`.
  - `canonical = info.canonicalFilePath()` непустой; `QFileInfo(canonical).isFile()` и `!QFileInfo(canonical).isSymLink()`.
  - `QDir(canonicalRoot).relativeFilePath(canonical)` непустой, не начинается с `..`, не содержит `/` и `\` — только прямой дочерний файл.
  - lower-case canonical заканчивается на `.plugin` или `.py`.
  - Возвращать `{ok, canonical, ""}` или `{false, "", error}` с понятными ошибками из Global Constraints.

  `startHost()`:

  - `QStandardPaths::findExecutable(QStringLiteral("python3"))`; пусто — `"Python 3 was not found."`; взять `canonicalFilePath()`.
  - `FindBwrap()`: `EXTERALESS_BWRAP` (через `findExecutable`), иначе `QStandardPaths::findExecutable("bwrap")`; пусто — `"Plugin sandbox is unavailable."`.
  - `canonicalRoot = QFileInfo(_pluginsDirectory).canonicalFilePath()`; пусто или не директория — `"Plugins directory is missing."`.
  - `SandboxPath()`: первый существующий из `/run/current-system/sw/bin`, `/usr/local/bin`, `/usr/bin`, `/bin`; fallback — каталог `canonicalPython`; пусто/не директория — `"Plugin sandbox is unavailable."`.
  - `_process = std::make_unique<QProcess>()`; `setProcessEnvironment(QProcessEnvironment())`; `setProgram(bwrap)`; `setArguments(SandboxArguments(canonicalRoot, canonicalPython, kHostScript, sandboxPath))`; `start()`; `waitForStarted(kStartTimeout)`.
  - `SandboxArguments` ровно: sandbox-флаги из Global Constraints, read-only bind'ы, `--ro-bind <canonicalRoot> /plugins`, `--remount-ro /`, `--chdir /`, `--clearenv`, `PATH`/`HOME`/`TMPDIR`/`LANG`, `--`, `python -I -B -c <script> /plugins`.
  - Прочитать ровно `{"type":"ready"}` в пределах `kStartTimeout`; audit/log до ready логировать; ошибка/exit — `"Plugin host failed to start."` + stderr.
  - При неудаче kill/wait и `clearHost()`.

  `readResponse(seq, &response, &error)`:

  - Писать request как compact JSON + `\n`; `waitForBytesWritten(kWriteTimeout)`.
  - Читать `readAllStandardOutput()` в `_readBuffer`; выдавать строки по `\n`; при `_readBuffer.size() > kMaxReadBuffer` — kill/error.
  - Для каждой строки: parse JSON; invalid JSON — kill host и `"Plugin host returned invalid output."`.
  - `audit`/`log` принимаются только при совпадении `seq`; устаревшие отбрасываются с `qWarning` и в audit log не пишутся.
  - `type == ready` вне старта, чужой/отсутствующий `seq`, `ok:false` и неизвестный тип — protocol violation, kill host.
  - `loaded`/`unloaded`/`error` с нужным seq — вернуть.
  - Если процесс не Running — `"Plugin host exited."` + stderr.
  - Дедлайн `kCommandTimeout`; по истечении kill/wait и `"Plugin host timed out."`.

  `enable(id, path)`:

  - `ValidatePath`; ошибка — вернуть без запуска host.
  - Если `_loaded[id] == canonical` и `isRunning()`, вернуть `{true, ""}`.
  - Если id загружен из другого пути — `disable(id)`; ошибка — вернуть.
  - Если host не running, `clearHost()` и `startHost()`.
  - Отправить `load` с `path = "/plugins/" + fileName(canonical)`; на `loaded` с `ok:true` записать `_loaded[id] = canonical`; на `error` вернуть `message`; на timeout/exit/violation — `clearHost()` и ошибку.
  - `isLoaded(id)` возвращает `isRunning() && _loaded.contains(id)`, чтобы stale-запись умершего host не считалась загруженной.

  `disable(id)`:

  - Если host не running — очистить `_loaded` и вернуть `{true, ""}` (stale state после смерти процесса не должен выглядеть загруженным).
  - Если id не загружен — вернуть `{true, ""}`.
  - Отправить `unload`; timeout/exit/invalid — `clearHost()` и ошибка (conservative: не оставлять подозрительный plugin).
  - На `unloaded` удалить `_loaded[id]`; `warning` записать в audit как `kind="log"`; вернуть `{true, ""}`. Warning не является ошибкой disable.

  `stop()`:

  - Если процесса нет — очистить state и выйти.
  - Отправить `shutdown` (без ожидания ответа), `waitForBytesWritten`, `waitForFinished(kStopTimeout)`; иначе `kill()` + `waitForFinished(kKillTimeout)`.
  - `clearHost()`: kill при необходимости, `_process.reset()`, `_readBuffer.clear()`, `_loaded.clear()`, `_seq = 0`.

  `Start`/`Instance`/`Stop`:

  - `Start` идемпотентен по паре `(pluginsDirectory, auditLogPath)`; при смене пути — `Stop()` и новый `Runtime`.
  - `Stop()` удаляет singleton; `~Runtime()` вызывает `stop()`.
  - `Instance()` возвращает `nullptr`, если `Start` ещё не вызывался.

  `writeAuditRecord(kind, id, event, detail)`:

  - `QDir().mkpath(QFileInfo(_auditLogPath).absolutePath())`.
  - Открыть `QFile(_auditLogPath)` в `WriteOnly | Append` (без `QIODevice::Text`, чтобы не получить CRLF), записать compact JSON + `\n`.
  - Поля ровно `time` (UTC `Qt::ISODateWithMs`), `kind`, `plugin`, `event`, `detail`. Ошибку открытия — `qWarning`, не ронять runtime.

- [ ] **Step 7: Добавить test CLI и CMake**

  `plugin_host_cli.cpp`:

  - `QCoreApplication app(argc, argv); QCoreApplication::setApplicationName(Exteraless::AppFileName());` до первого `Config::Instance()`.
  - Аргументы: root, audit, command, args. Неверное число — `2`.
  - `validate`, `enable`, `disable`, `stop`, `state-get`, `state-set`, `import-state`, `multi` — ровно по таблице Interfaces.
  - `import-state` читает JSON-файл, вызывает `Config::Instance().fromJson(object, &unknown)`, печатает `{"ok":bool,"unknown":[...]}`.
  - `multi` парсит JSON-массив операций; каждая операция — object с `op`; поддерживаются `enable`, `disable`, `stop`, `pid`; результат каждой операции — object с `op`, `ok`, `error`, `loaded`, `pid`.
  - Все строки — `QStringLiteral` (цель не подключает `_q`).

  В `exteraless/tests/plugins/CMakeLists.txt` добавить:

```cmake
add_executable(exteraless-plugin-host-cli EXCLUDE_FROM_ALL
    plugin_host_cli.cpp
    ${CMAKE_SOURCE_DIR}/Telegram/SourceFiles/exteraless/plugin_runtime.cpp
    ${CMAKE_SOURCE_DIR}/Telegram/SourceFiles/exteraless/plugin_runtime.h
)

target_include_directories(exteraless-plugin-host-cli
PRIVATE
    ${CMAKE_SOURCE_DIR}/Telegram/SourceFiles
)

target_link_libraries(exteraless-plugin-host-cli
PRIVATE
    exteraless
)
```

  В `Telegram/CMakeLists.txt` в `target_sources(Telegram PRIVATE ...)` добавить `SourceFiles/exteraless/plugin_runtime.cpp` и `.h` рядом с `plugins_box.*`. `exteraless/CMakeLists.txt` не менять: runtime компилируется в Telegram target и напрямую в test CLI.

- [ ] **Step 8: Переконфигурировать и собрать**

```bash
nix develop --command cmake -S . -B out
nix develop --command ninja -C out \
  Telegram/CMakeFiles/Telegram.dir/SourceFiles/exteraless/plugin_runtime.cpp.o
nix develop --command cmake --build out --target exteraless-plugin-host-cli -j2
```

  Ожидается: оба target компилируются без ошибок и предупреждений, новых зависимостей нет.

- [ ] **Step 9: Запустить backend checks**

```bash
python3 exteraless/tests/plugins/run_host_checks.py \
  out/exteraless/tests/plugins/exteraless-plugin-host-cli
```

  Ожидается: все проверки PASS, включая timeout около 10 секунд, single host PID и state persistence. Runner не должен оставлять процессы `bwrap`/`python3`; после завершения проверить `pgrep -af '^/[^ ]*bwrap '` и добить только PID, записанные runner'ом (runner обязан чистить их сам).

- [ ] **Step 10: Точечно проверить path/state команды**

```bash
CLI=out/exteraless/tests/plugins/exteraless-plugin-host-cli
TMP=$(mktemp -d /tmp/opencode/plugin-host-manual.XXXXXX)
mkdir -p "$TMP/plugins" "$TMP/data" "$TMP/tdata"
cp exteraless/tests/plugins/fixtures/runtime/valid.plugin "$TMP/plugins/"
XDG_DATA_HOME="$TMP/data" "$CLI" "$TMP/plugins" "$TMP/tdata/plugins-audit.log" \
  validate "$TMP/plugins/valid.plugin"
XDG_DATA_HOME="$TMP/data" "$CLI" "$TMP/plugins" "$TMP/tdata/plugins-audit.log" \
  state-set demo_runtime true
XDG_DATA_HOME="$TMP/data" "$CLI" "$TMP/plugins" "$TMP/tdata/plugins-audit.log" \
  state-get demo_runtime
cat "$TMP/data/ExteralessDesktop/tdata/exteraless.json"
```

  Ожидается: `validate` с `ok:true` и canonical path; `state-get` с `enabled:true`; в `exteraless.json` есть `OEPluginStates: {"demo_runtime": true}`. Не удалять `$TMP` до проверки, затем удалить вручную.

---

### Task 2: Plugins box toggle integration, auto-load on open and Xvfb smoke

**Files:**
- Modify: `Telegram/SourceFiles/exteraless/plugins_box.cpp`
- Modify: `Telegram/SourceFiles/core/application.cpp`

**Interfaces:**

- Consumes: `Exteraless::Plugins::ScanPlugins`, `Exteraless::Plugins::Runtime`, `Exteraless::Config::pluginEnabled/setPluginEnabled`.
- Produces: `Exteraless::ShowPluginsBox(not_null<Window::SessionController*>)` сохраняет прежнюю сигнатуру; UI-контракт ниже.

- [ ] **Step 1: Собрать строки бокса**

  В `plugins_box.cpp` добавить `#include "exteraless/config.h"`, `#include "exteraless/plugin_runtime.h"`, `#include "ui/widgets/buttons.h"`, `#include "styles/style_settings.h"` и `<memory>`/`<vector>`:

  - `PluginsDirectory()` оставить `cWorkingDir() + u"tdata/plugins"_q`; добавить `AuditLogPath() = cWorkingDir() + u"tdata/plugins-audit.log"_q`.
  - `QDir().mkpath(directory)`, затем `Runtime::Start(directory, AuditLogPath())` **до** создания бокса. `Start` не запускает процесс.
  - Для каждой записи сканера:
    - `entry.error` непустой — оставить `FlatLabel` с текущим `RowText` и не давать toggle.
    - Иначе — `SettingsButton` с заголовком `name version` (fallback `file`), `st::settingsButtonNoIcon`, `toggleOn(enabled->value(), true)`, `setClickedCallback`.
    - Ниже — `FlatLabel` с деталями: author, `permissions: ...`, `file: ...`, `status`.
  - Заголовок — `Plugins (%1)`.
  - Сверху — `FlatLabel` `hostStatus` с текстом `Plugin host: stopped` и пояснением: `Enabled plugins start when this window opens. The host keeps running until Exteraless exits. Manifest permissions are informational in this stage; network, subprocess, native and out-of-directory file access are denied for every plugin.`
  - Кнопки — только `Open folder` (left) и `Close`.
  - Состояние строк хранить в `box->lifetime().make_state<std::vector<std::unique_ptr<Row>>>()`, где `Row` содержит `Plugins::Entry entry`, `rpl::variable<bool> *enabled`, `bool loaded`, `QString error`, `Ui::FlatLabel *details`, `Ui::SettingsButton *button`.

- [ ] **Step 2: Автозагрузка после появления бокса**

  В конце init-лямбды:

```cpp
box->showFinishes() | rpl::take(1) | rpl::on_next([=] {
    const auto runtime = Plugins::Runtime::Instance();
    if (!runtime) {
        return;
    }
    for (const auto &row : *rows) {
        if (!row->enabled->current() || !row->entry.error.isEmpty()) {
            if (runtime->isLoaded(row->entry.id)) {
                runtime->disable(row->entry.id);
            }
            continue;
        }
        row->details->setText(DetailsText(
            row->entry,
            u"Enabled (not running)"_q));
        const auto result = runtime->enable(
            row->entry.id,
            directory + u"/"_q + row->entry.file);
        row->loaded = result.ok;
        row->error = result.error;
        row->details->setText(DetailsText(
            row->entry,
            StateText(*row)));
    }
    hostStatus->setText(HostStatusText(*runtime));
}, box->lifetime());
```

  `StateText`: `Disabled`, `Disabled: <error>`, `Enabled (running)`, `Enabled (not running)`, `Enabled (not running): <error>`. `HostStatusText`: `Plugin host: stopped` или `Plugin host: running`. После загрузки обновить `hostStatus`.

  Автозагрузка не должна выполнять плагин, если `Config::pluginEnabled(id) == false`, и не должна менять config. Если `enable` упал, preference остаётся `true`, строка показывает ошибку; следующее открытие повторит попытку.

- [ ] **Step 3: Реализовать toggle**

  В `setClickedCallback`:

  - `const auto runtime = Runtime::Instance(); if (!runtime) return;`
  - `const auto enable = !row->enabled->current();`
  - При enable: `details->setText(DetailsText(entry, u"Loading..."_q))`, вызвать `runtime->enable(id, path)`, затем `Config::Instance().setPluginEnabled(id, true)`, выставить `row->loaded = result.ok`, `row->error = result.error`.
  - При disable: вызвать `runtime->disable(id)`, затем `Config::Instance().setPluginEnabled(id, false)`, `row->loaded = false`, `row->error = result.error`.
  - `row->enabled->force_assign(enable)`; обновить `details` и `hostStatus`.
  - UI остаётся синхронным: клик может блокировать main thread до 10 секунд. Это осознанное ограничение stage 2; не добавлять фоновые потоки/таймеры.
  - Toggle не должен писать config до вызова runtime, иначе состояние разъедется при ошибке.

- [ ] **Step 4: Остановить host при завершении приложения**

  В `Telegram/SourceFiles/core/application.cpp`:

  - Добавить `#include "exteraless/plugin_runtime.h"` рядом с другими `exteraless` include.
  - В `Application::~Application()` перед `Local::finish()` вызвать `Exteraless::Plugins::Runtime::Stop();`.
  - Убедиться, что в `Application::run`/`startSettingsAndBackground`/`ShowPluginsBox` нет другого вызова, который стартует host.

- [ ] **Step 5: Собрать**

```bash
nix develop --command cmake -S . -B out
nix develop --command ninja -C out \
  Telegram/CMakeFiles/Telegram.dir/SourceFiles/exteraless/plugins_box.cpp.o \
  Telegram/CMakeFiles/Telegram.dir/SourceFiles/core/application.cpp.o
nix develop --command cmake --build out --target Telegram -j2
```

  Для запуска host нужен `bwrap` в `PATH`: `nix develop` даёт `bubblewrap` и `python3`, а `nix/package.nix` добавляет их в `PATH` через `wrapProgram` (сам wrapper и `flake.nix` в патч не входят).

  Если финальный линк падает на известном GNU ld debug-info overflow (>4 GiB), выполнить один LLD-релинк, не меняя закоммиченный CMake-конфиг. Нужны оба флага — `-fuse-ld=lld` и `-Wl,-S`:```bash
nix develop --command bash -c '
  lld=$(nix build --no-link --print-out-paths nixpkgs#lld)
  link=$(ninja -C out -t commands bin/Telegram | tail -1)
  PATH="$lld/bin:$PATH" bash -c "${link/g++/g++ -fuse-ld=lld -Wl,-S}"
'
```

- [ ] **Step 6: Xvfb smoke — нет исполнения при старте**

  Подготовить изолированный профиль и включить оба валидных плагина через test CLI **до** запуска приложения:

```bash
export SMOKE=/tmp/opencode/plugins-host-ui
export DISPLAY=:99
export XDG_DATA_HOME="$SMOKE"
export XDG_CACHE_HOME="$SMOKE-cache"
host_pids() {
  pgrep -af '^/[^ ]*bwrap ' \
    | grep -F -- "$SMOKE/tdata/plugins" \
    | awk '{print $1}' || true
}
rm -rf "$SMOKE" "$SMOKE-cache"
mkdir -p "$SMOKE/tdata/plugins" "$SMOKE-cache"
cp exteraless/tests/plugins/fixtures/runtime/valid.plugin \
   exteraless/tests/plugins/fixtures/runtime/valid-second.plugin \
   exteraless/tests/plugins/fixtures/runtime/deny-write.plugin \
   "$SMOKE/tdata/plugins/"
out/exteraless/tests/plugins/exteraless-plugin-host-cli \
  "$SMOKE/tdata/plugins" "$SMOKE/tdata/plugins-audit.log" \
  state-set demo_runtime true
out/exteraless/tests/plugins/exteraless-plugin-host-cli \
  "$SMOKE/tdata/plugins" "$SMOKE/tdata/plugins-audit.log" \
  state-set demo_second true
pgrep -f "Xvfb :99" >/dev/null || \
  (Xvfb :99 -screen 0 1280x800x24 >/tmp/opencode/xvfb99-plugin-host.log 2>&1 &)
QT_QPA_PLATFORM=xcb out/bin/Telegram -workdir "$SMOKE" -noupdate \
  >"$SMOKE/app.log" 2>&1 &
echo $! > "$SMOKE/app.pid"
sleep 12
test ! -e "$SMOKE/tdata/marker-denied" && echo "no marker at startup"
test -z "$(host_pids)" && echo "no host at startup"
```

  Ожидается: оба сообщения напечатаны; host-процесса нет, маркер отсутствует. Если `pgrep` показывает scanner из другого теста — использовать PID-файл и точный шаблон, не убивать чужие процессы.

- [ ] **Step 7: Xvfb smoke — открытие бокса загружает state=true**

  Открыть drawer, Exteraless, прокрутить до Plugins, кликнуть. Координаты stage 1: `(45,65)` меню, `(87,541)` Exteraless, затем скролл и клик по строке Plugins; при другом профиле скорректировать по скриншоту. Использовать `/tmp/opencode/xinput.py`.

```bash
python3 /tmp/opencode/xinput.py click 45 65
sleep 1
python3 /tmp/opencode/xinput.py click 87 541
sleep 2
for i in $(seq 1 15); do
  python3 /tmp/opencode/xinput.py click 640 400 5
  sleep 0.2
done
import -window root /tmp/opencode/plugin-host-01-list.png
```

  Найти и кликнуть строку `Plugins`, подождать 3 секунды, снять `plugin-host-02-box.png`. Проверить:

  - заголовок `Plugins (3)`;
  - две строки с тумблерами ON и статусом `Enabled (running)`;
  - строка `deny-write` существует, но её toggle не включали (state false), статус `Disabled`;
  - верхний статус `Plugin host: running`;
  - нет install/remove/run кнопок.
  - `host_pids` показывает ровно один `bwrap` host (Python — его дочерний процесс); записать PID в `$SMOKE/host.pid`:

```bash
test "$(host_pids | wc -l)" -eq 1
host_pids | head -1 > "$SMOKE/host.pid"
test -s "$SMOKE/host.pid"
```

  - audit log содержит `demo loaded` и `second loaded`; маркер `$SMOKE/tdata/marker-denied` отсутствует.

- [ ] **Step 8: Xvfb smoke — toggle, close/reopen, app exit**

  - Кликнуть toggle `valid-second` OFF; снять скриншот; проверить `Disabled`, второй плагин остался `Enabled (running)`, host PID не изменился.
  - Кликнуть toggle ON; проверить `Enabled (running)`, в audit log ровно одна новая запись `second loaded`, host PID тот же.
  - Закрыть бокс кнопкой `Close`; `kill -0 "$(cat "$SMOKE/host.pid")"` — host жив.
  - Открыть бокс снова; подождать 2 секунды; проверить, что host PID тот же, новых `demo loaded`/`second loaded` нет (идемпотентный `enable`), статусы `Enabled (running)`.
  - Завершить приложение: `kill "$(cat "$SMOKE/app.pid")"`; подождать 3 секунды; `host_pids` пуст; `kill -0 "$(cat "$SMOKE/host.pid")"` возвращает ошибку. Если host остался — записать PID и причину, не убивать чужие процессы.
  - Проверить `$SMOKE/app.log` на отсутствие crash/assert.

- [ ] **Step 9: Xvfb smoke — timeout в UI**

  Отдельный профиль `$SMOKE-timeout`, только `infinite-loop.plugin`, state true. Открыть бокс; зафиксировать, что окно отвечает не дольше ~10 секунд, затем строка показывает `Enabled (not running): Plugin host timed out.`, верхний статус `Plugin host: stopped`, host PID 0. Закрыть/открыть бокс — снова попытка и снова timeout; это ожидаемо, preference остаётся true. Проверить, что приложение не зависло навсегда и его можно закрыть. После закрытия приложения host отсутствует. Известное следствие: timeout убивает host для всех плагинов, поэтому ранее загруженный соседний плагин в этом же боксе останется `Enabled (not running)` до повторного открытия.

---

### Task 3: Docs, patch, flake and final verification

**Files:**
- Modify: `CHANGELOG.md`
- Modify: `STATUS.md`
- Modify: `docs/feature-mapping.md`
- Modify: `Agents/README.md`
- Modify: `nix/package.nix`
- Modify: `flake.nix`
- Regenerate: `nix/patches/exteraless.patch`

- [ ] **Step 1: Обновить `CHANGELOG.md`**

  В секцию `## 2026-11-04` добавить пункты: persistent `OEPluginStates`, host `bwrap` + `python3 -I -B -c`, JSON-lines с `seq`/`ok`, load/unload, audit `tdata/plugins-audit.log`, keyring join, UI-тумблеры, автозагрузка только при открытии Plugins, отсутствие автозапуска при старте, backend/UI проверки, пакетный wrapper. bwrap называть песочницей (namespaces, read-only mounts), но явно писать, что audit hook — policy layer, а не полная OS-изоляция.

- [ ] **Step 2: Обновить `STATUS.md`**

  В `### MVP3` добавить отдельный закрытый пункт после stage 1:

```markdown
- [x] Плагины, этап 2: контролируемый host-процесс `bwrap` + `python3 -I -B -c`,
  JSON-lines с `seq`/`ok`, load/unload, audit-лог, `OEPluginStates`, тумблеры
  и автозагрузка при открытии Plugins; без автозапуска при старте приложения,
  host останавливается при выходе.
```

  В `### Проверки` добавить фактические результаты: `run_host_checks.py` (213 проверок), `run_checks.py` (32), bwrap-профиль, keyring, timeout ~10s, single host PID, path/symlink, state persistence, Xvfb no-startup и running/toggle/exit, применение патча. Оставить открытыми install/remove, audit UI и Android shim. Если какой-то пункт не проверен — не отмечать `[x]`.

- [ ] **Step 3: Обновить `docs/feature-mapping.md`**

  В `## Plugins` заменить «Stage 1» на stage 2:

  - runtime `bwrap` + `python3 -I -B -c` и JSON-lines `seq`/`ok`;
  - `OEPluginStates`, default false, прямой дочерний regular file, `.plugin`/`.py`, canonical/no-symlink;
  - bwrap profile: read-only `/` и `/plugins`, скрытый HOME, пустой environment, без сети, capabilities dropped, `/nix` read-only;
  - keyring join best-effort и запрет `/proc/keys`;
  - opening Plugins loads persisted enabled plugins; app startup does not;
  - toggles enable/disable; close box keeps host; app exit stops;
  - audit log path and denied classes;
  - permissions are informational, not capabilities;
  - security boundaries: bwrap — фактическая граница, audit hook best-effort и отключается плагином через `__main__`, потоки разрешены, нет seccomp/landlock, keyring не изолирован полностью; bwrap только Linux; один host; пакетный wrapper без дублирования upstream D-Bus substitution; полной сборки пакета ещё не было.

- [ ] **Step 4: Обновить `Agents/README.md`**

  В структуру добавить:

  - `Telegram/SourceFiles/exteraless/plugin_runtime.*` — singleton Runtime, bwrap + QProcess, JSON-lines, path validation, keyring join, audit log;
  - `exteraless/tests/plugins/plugin_host_cli.cpp` — test-only CLI;
  - `exteraless/tests/plugins/run_host_checks.py` — backend checks (213);
  - `exteraless/tests/plugins/fixtures/runtime/` — valid/malicious/sandbox/keyring/protocol fixtures;
  - `nix/package.nix` wrapper и `flake.nix` devShell для `bwrap`/`python3`;
  - поведение: no startup execution, load on Plugins box open, close box keeps host, app exit stops, permissions advisory, bwrap is the boundary and the audit hook is not a full sandbox.

- [ ] **Step 5: Перегенерировать патч**

```bash
nix/update-patch.sh
```

  Скрипт делает `git add -N exteraless Telegram` и пишет `nix/patches/exteraless.patch`. Новые production-файлы `plugin_runtime.*`, test CLI, runner и фикстуры должны попасть в патч; docs/changelog — нет. `nix/package.nix` и `flake.nix` не входят в патч by design: wrapper/devShell применяются в flake, а не к upstream-исходникам.

- [ ] **Step 6: Dry-run и real apply на чистом tdesktop 7.3.1**

```bash
check=/tmp/opencode/tdesktop-clean-host
rm -rf "$check" && mkdir -p "$check"
git archive v7.3.1 | tar -x -C "$check"
git -C "$check" init -q
git -C "$check" apply --check "$PWD"/nix/patches/exteraless.patch
git -C "$check" apply "$PWD"/nix/patches/exteraless.patch
git -C "$check" apply --stat "$PWD"/nix/patches/exteraless.patch \
  | grep -E 'plugin_runtime|plugin_host_cli|run_host_checks|fixtures/runtime'
diff -r --brief "$check/Telegram/SourceFiles/exteraless" \
  Telegram/SourceFiles/exteraless
diff -r --brief "$check/exteraless" exteraless
```

  Ожидается успешный `apply --check`, успешный real apply, строки для новых файлов и отсутствие отличий `plugin_runtime`/`plugins_box`/`settings_exteraless`/CMake/config от рабочего дерева (кроме `__pycache__`). Затем удалить `$check`. Проверить, что в патче есть bwrap/keyring строки, а `nix/package.nix`/`flake.nix` — нет by design.

- [ ] **Step 7: Проверить flake и wrapper**

```bash
nix flake check --no-build
nix eval --raw .#packages.x86_64-linux.default.postFixup | bash -n
```

  Ожидается успех обеих команд: `postFixup` сохраняет upstream и содержит `wrapProgram` с `bubblewrap`/`python3`. Если flake не видит новые файлы — проверить, что они не в `.gitignore` и попали в `nix/patches/exteraless.patch`.

- [ ] **Step 8: Финальная сборка и regression**

```bash
nix develop --command cmake -S . -B out
nix develop --command cmake --build out --target exteraless-plugins-scan -j2
nix develop --command cmake --build out --target exteraless-plugin-host-cli -j2
nix develop --command cmake --build out --target Telegram -j2
python3 exteraless/tests/plugins/run_checks.py
python3 exteraless/tests/plugins/run_host_checks.py
```

  Ожидается: stage 1 runner (32 проверки) PASS, stage 2 runner (213 проверок) PASS, сборка без новых предупреждений. При повторном GNU ld overflow — LLD-релинк из Task 2 Step 5 и запись об этом в отчёт.

- [ ] **Step 9: Финальный Xvfb smoke**

  Повторить Task 2 Steps 6–9 на финальном бинарнике. Проверить: no host at startup, один host после открытия, загрузка state=true, toggle OFF/ON, close box keeps host, app exit kills host, timeout UI не вешает приложение. Сохранить скриншоты и audit log в `/tmp/opencode/plugin-host-final-*`.

- [ ] **Step 10: Независимый review diff**

  Передать отдельному агенту `git diff` и `git status` без права редактирования. Проверить:

  - нет `git commit`/`push`/изменения истории;
  - host не стартует при запуске приложения;
  - один процесс: `bwrap` напрямую запускает Python, нет shell-обёрток, `unshare`-обёрток и процессов на плагин;
  - bwrap-профиль соответствует Global Constraints: read-only `/` и `/plugins`, скрытый HOME, пустой environment, нет сети, capabilities dropped, `--die-with-parent`;
  - keyring join вызывается до hook, `/proc/keys` плагину запрещён;
  - audit hook закрывает известные события (`socket`/`subprocess`/`ctypes`/`exec`/write outside), но честно документирован как best-effort policy layer без claims security boundary: известны отключение через `__main__` и разрешённые потоки;
  - path validation direct child + canonical + no-symlink + host recheck `O_NOFOLLOW`;
  - state key whitelist и default false;
  - comments не превышают лимит, LF/no-BOM, нет `#ifdef _DEBUG`;
  - новые файлы попали в патч, `nix flake check --no-build` и `postFixup | bash -n` проходят.

- [ ] **Step 11: Итоговый отчёт**

  Записать: пути изменённых файлов; результаты `run_host_checks.py` (213 проверок) и `run_checks.py` (32), timeout/single-host/state/symlink/bwrap/keyring; результаты Xvfb; способ линка (GNU ld или LLD); известные ограничения (bwrap только Linux, audit hook best-effort и отключается через `__main__`, потоки разрешены, один host, синхронный UI до 10s, нет seccomp, `/nix` read-only доступен, keyring не изолирован полностью, пакетный wrapper, полной Nix-сборки ещё не было). Не делать commit/push.

## Out of Scope

- Установка/удаление плагинов, repository/catalog, обновления.
- События чатов, API сообщений, UI-хуки, permissions enforcement.
- Дополнительный seccomp/landlock профиль, ограничение памяти/CPU; bwrap уже даёт namespaces, `--die-with-parent` и read-only mounts.
- Ротация audit-лога и его UI-просмотр.
- Многофайловые plugin-пакеты и sibling imports внутри plugin dir в stage 2 не поддерживаются: компилируется только основной файл, остальное — stdlib.
- Android `.elyx`/`.eaf` runtime и shim.
- Коммиты, push, изменение истории git.

## Known Limitations

- bwrap доступен только на Linux; на других платформах host fail closed (`Plugin sandbox is unavailable.`), плагины не запускаются. Это единственная фактическая граница этапа; без bwrap плагины не запускаются вообще.
- Python audit hook — best-effort policy layer, не security boundary: нативный код, найденные fd, неаудируемые C-пути и filename-spoofing для stdlib-исключений `compile`/`exec` могут его обойти. Deny-by-default относится только к набору известных audit-событий и не является гарантией изоляции.
- Плагин может полностью отключить audit hook: host-скрипт исполняется как `__main__`, поэтому `import __main__; __main__._IN_AUDIT[0] = True` выключает все проверки (проверено: после этого создаётся socket, а файловые проверки не срабатывают). Плагин также может менять другие глобалы host-скрипта.
- Потоки сейчас разрешены: `threading.Thread` не является audit-событием, а Python 3.14 стартует потоки через `_thread.start_joinable_thread`, которого нет в deny-наборе. Запись `threading.Thread` в deny-наборе — мёртвая; запрещён только прямой `_thread.start_new_thread`.
- Нет seccomp/landlock: bwrap ограничивает namespaces, mounts, network и capabilities, но не syscall-поверхность.
- `/nix` примонтирован read-only: плагин может читать Nix store (бинарники, исходники, данные пакетов), хотя не может туда писать.
- Session keyring отделяется best-effort через `KEYCTL_JOIN_SESSION_KEYRING`; это не полная keyring-изоляция, а `/proc/keys` лишь запрещён audit hook'ом.
- Один host на приложение: timeout или crash убивает его и выгружает все плагины; соседний загруженный плагин останется `Enabled (not running)` до повторного открытия бокса.
- UI-тумблер синхронный: `on_load`/`on_unload` могут заморозить main thread до 10 секунд; авто-загрузка при открытии бокса — до 10 секунд на каждый включённый плагин.
- Все плагины имеют доступ на запись ко всему каталогу `/plugins`, а не к отдельной песочнице; вредоносный плагин может изменить соседний файл.
- Audit-лог пишется best-effort: ошибка открытия/записи даёт только `qWarning`, ротации и UI-просмотра нет. После отключения hook через `__main__` denied-события туда не попадают вовсе.
- Sibling imports внутри plugin dir не поддерживаются: `compile` для plugin-root файлов разрешён только загрузчику основного файла, поэтому helper-модули надо либо встроить в один файл, либо дождаться следующего этапа.
- Manifest permissions не enforced; `network`/`native` в манифесте не дают прав и не меняют audit policy.
- `OEPluginStates` хранится в `exteraless.json`, который привязан к `QStandardPaths::AppLocalDataLocation` и не учитывает `-workdir` (известное ограничение проекта); smoke-тесты обязаны задавать `XDG_DATA_HOME`.
- Nix-пакет добавляет `bubblewrap` и `python3` в `PATH` через `wrapProgram` в `postFixup`, не дублируя upstream D-Bus `substituteInPlace`. Полная сборка пакета после этапа 2 ещё не выполнялась: проверены только `nix eval` для `postFixup`, `bash -n`/shellcheck, `nix flake check --no-build` и применение патча.
