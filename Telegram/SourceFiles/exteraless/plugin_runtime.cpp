#include "exteraless/plugin_runtime.h"

#include <QDateTime>
#include <QDeadlineTimer>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QStringList>

#include <algorithm>

namespace Exteraless::Plugins {
namespace {

constexpr auto kStartTimeout = 5000;
constexpr auto kCommandTimeout = 10000;
constexpr auto kWriteTimeout = 2000;
constexpr auto kStopTimeout = 3000;
constexpr auto kKillTimeout = 1000;
constexpr auto kMaxReadBuffer = 1024 * 1024;

constexpr auto kSandboxUnavailable =
	"Plugin sandbox is unavailable.";
constexpr auto kProtocolViolation =
	"Plugin host protocol violation.";
constexpr auto kInvalidOutput =
	"Plugin host returned invalid output.";

constexpr auto kHostScript = R"PY(
import importlib.machinery
import importlib.util
import io
import json
import os
import re
import stat
import sys
import sysconfig
import types

try:
    _FD_LIMIT = min(int(os.sysconf('SC_OPEN_MAX')), 1 << 20)
except (OSError, ValueError):
    _FD_LIMIT = 65536
os.closerange(3, _FD_LIMIT)

ID_RE = re.compile(r'^[A-Za-z][A-Za-z0-9_-]{1,31}$')
MAX_BYTES = 2 * 1024 * 1024
SUPPORTED = ('.plugin', '.py')

DENY_EXACT = {
    'os.system', 'os.exec', 'os.fork', 'os.forkpty', 'os.posix_spawn',
    'os.spawn', 'os.kill', 'os.killpg', 'subprocess.Popen', 'pty.spawn',
    'socket.__new__', 'socket.connect', 'socket.bind', 'socket.getaddrinfo',
    'socket.gethostbyname', 'socket.gethostbyname_ex', 'socket.gethostbyaddr',
    'socket.sendto', 'socket.sendmsg',
    'sys.addaudithook', 'sys._getframe', 'sys._current_frames',
    'sys.settrace', 'sys.setprofile', 'builtins.input', 'os.chdir',
    'gc.get_objects', 'gc.get_referrers', 'gc.get_referents',
    'fcntl.fcntl', 'fcntl.ioctl', 'mmap.__new__',
    '_thread.start_new_thread', 'threading.Thread',
}
DENY_PREFIX = ('ctypes.',)
MUTATING = {
    'os.remove', 'os.rename', 'os.replace', 'os.rmdir', 'os.mkdir',
    'os.link', 'os.symlink', 'os.chmod', 'os.chown', 'os.truncate',
    'os.utime', 'os.mkfifo', 'os.mknod',
    'shutil.copyfile', 'shutil.copymode', 'shutil.copystat',
    'shutil.move', 'shutil.rmtree',
}

_CURRENT_ID = ['']
_CURRENT_SEQ = [0]
_ALLOW_COMPILE = [False]
_LOAD_CODE = [None]
_IN_AUDIT = [False]
_PLUGINS = {}


class _NullSink:
    encoding = 'utf-8'
    errors = 'replace'

    def write(self, text):
        return len(text) if isinstance(text, str) else 0

    def writelines(self, lines):
        for line in lines:
            self.write(line)

    def flush(self):
        pass

    def isatty(self):
        return False

    def writable(self):
        return True

    def readable(self):
        return False

    def fileno(self):
        raise OSError('plugin output is not available')


class BasePlugin:
    def on_load(self):
        pass

    def on_unload(self):
        pass

    def log(self, message):
        _host_log(_CURRENT_ID[0], str(message)[:500])


PROTOCOL = os.fdopen(os.dup(1), 'wb', buffering=0)
INPUT = os.fdopen(os.dup(0), 'rb', buffering=0)
_DEVNULL = os.open(os.devnull, os.O_RDWR)
os.dup2(_DEVNULL, 0)
os.dup2(_DEVNULL, 1)
os.dup2(_DEVNULL, 2)
os.close(_DEVNULL)
sys.stdin = io.StringIO('')
sys.stdout = _NullSink()
sys.stderr = _NullSink()

ROOT = os.path.realpath(sys.argv[1])
_PATHS = sysconfig.get_paths()
STDLIB = os.path.realpath(_PATHS['stdlib'])
PLATSTDLIB = os.path.realpath(_PATHS['platstdlib'])
READ_ROOTS = tuple(sorted({
    ROOT,
    STDLIB,
    PLATSTDLIB,
    os.path.realpath(os.path.dirname(os.__file__)),
}))
WRITE_ROOTS = (ROOT,)


def _send(message):
    try:
        PROTOCOL.write((json.dumps(message, ensure_ascii=False) + '\n')
                       .encode('utf-8', 'replace'))
    except OSError:
        os._exit(1)


def _host_log(plugin_id, message):
    _send({
        'seq': _CURRENT_SEQ[0],
        'type': 'log',
        'id': plugin_id,
        'message': message,
    })


def _text(value):
    if isinstance(value, bytes):
        return os.fsdecode(value)
    return value if isinstance(value, str) else None


def _real(path):
    try:
        return os.path.realpath(path)
    except (OSError, ValueError):
        return os.path.abspath(path)


def _under(path, roots):
    real = _real(path)
    for root in roots:
        if real == root or real.startswith(root + os.sep):
            return True
    return False


def _stdlib_caller():
    try:
        frame = sys._getframe(3)
    except (AttributeError, ValueError):
        return False
    if frame is None:
        return False
    filename = frame.f_code.co_filename
    if not (isinstance(filename, str)
            and _under(filename, (STDLIB, PLATSTDLIB))):
        return False
    frame = frame.f_back
    for _ in range(8):
        if frame is None:
            return False
        if frame.f_code.co_filename in (
                '<frozen importlib._bootstrap>',
                '<frozen importlib._bootstrap_external>'):
            return True
        frame = frame.f_back
    return False


def _deny(event, detail):
    _send({
        'seq': _CURRENT_SEQ[0],
        'type': 'audit',
        'id': _CURRENT_ID[0],
        'event': event,
        'detail': str(detail)[:500],
    })
    raise PermissionError('Exteraless audit: %s denied' % event)


def _check_open(args):
    if not args:
        _deny('open', 'missing arguments')
    target = args[0]
    if isinstance(target, int):
        _deny('open', 'file descriptor %d' % target)
    text = _text(target)
    if text is None:
        _deny('open', repr(target)[:500])
    mode = args[1] if len(args) > 1 else None
    flags = args[2] if len(args) > 2 else 0
    writing = False
    if isinstance(mode, str):
        writing = any(ch in mode for ch in 'wax+')
    if isinstance(flags, int):
        writing = writing or bool(flags & (
            os.O_WRONLY | os.O_RDWR | os.O_CREAT | os.O_TRUNC | os.O_APPEND))
    roots = WRITE_ROOTS if writing else READ_ROOTS
    if not _under(text, roots):
        _deny('open', '%s %s' % (
            text, mode if isinstance(mode, str) else flags))


def _audit_check(event, args):
    if event in DENY_EXACT or event.startswith(DENY_PREFIX):
        _deny(event, repr(args)[:500])
    if event == 'open':
        _check_open(args)
    elif event in ('os.listdir', 'os.scandir'):
        target = args[0] if args else None
        if target is None:
            target = ROOT
        text = _text(target)
        if text is None or not _under(text, READ_ROOTS):
            _deny(event, repr(target)[:500])
    elif event in MUTATING:
        for value in args:
            if (value is None or isinstance(value, bool)
                    or isinstance(value, int)):
                continue
            text = _text(value)
            if text is None or not _under(text, WRITE_ROOTS):
                _deny(event, repr(args)[:500])
    elif event in ('os.setxattr', 'os.removexattr'):
        target = args[0] if args else None
        text = _text(target)
        if text is None or not _under(text, WRITE_ROOTS):
            _deny(event, repr(args)[:500])
    elif event in ('os.getxattr', 'os.listxattr'):
        target = args[0] if args else None
        text = _text(target)
        if text is None or not _under(text, READ_ROOTS):
            _deny(event, repr(args)[:500])
    elif event == 'import':
        module = args[0] if args else ''
        filename = args[1] if len(args) > 1 else None
        if filename is None or module in sys.builtin_module_names:
            return
        text = _text(filename)
        if text is None or not _under(text, READ_ROOTS):
            _deny(event, 'module %s from %r' % (module, filename))
    elif event == 'compile':
        filename = args[1] if len(args) > 1 else ''
        text = _text(filename)
        if _ALLOW_COMPILE[0]:
            return
        if text is not None and _under(text, (STDLIB, PLATSTDLIB)):
            return
        if _stdlib_caller():
            return
        _deny(event, repr(filename)[:500])
    elif event == 'exec':
        code = args[0] if args else None
        if code is _LOAD_CODE[0]:
            return
        filename = getattr(code, 'co_filename', None)
        text = _text(filename)
        if text is not None and _under(text, (STDLIB, PLATSTDLIB)):
            return
        if _stdlib_caller():
            return
        _deny(event, repr(filename)[:500])


def _audit(event, args):
    # Best-effort only: fork_exec or a fresh interpreter bypasses it; bwrap is the boundary.
    if _IN_AUDIT[0]:
        return
    _IN_AUDIT[0] = True
    try:
        _audit_check(event, args)
    finally:
        _IN_AUDIT[0] = False


class _ImportGuard:
    def find_spec(self, fullname, path=None, target=None):
        leaf = fullname.rpartition('.')[2]
        search = sys.path if path is None else path
        for entry in search:
            if not isinstance(entry, str) or not entry:
                continue
            real = _real(entry)
            if _under(real, READ_ROOTS):
                continue
            candidates = (
                os.path.join(real, leaf + '.py'),
                os.path.join(real, leaf + '.pyc'),
                os.path.join(real, leaf + '.so'),
                os.path.join(real, leaf, '__init__.py'),
            )
            for candidate in candidates:
                try:
                    if os.path.isfile(candidate):
                        _deny('import', 'module %s from %s' % (
                            fullname, real))
                except OSError:
                    continue
        return None


sys.meta_path.insert(0, _ImportGuard())


class _PluginLoader(importlib.machinery.SourceFileLoader):
    def __init__(self, fullname, path, data):
        super().__init__(fullname, path)
        self._data = data

    def get_data(self, path):
        if _real(path) == _real(self.path):
            return self._data
        return super().get_data(path)

    def get_code(self, fullname):
        source = importlib.util.decode_source(self._data)
        _ALLOW_COMPILE[0] = True
        try:
            code = compile(source, self.path, 'exec')
        finally:
            _ALLOW_COMPILE[0] = False
        _LOAD_CODE[0] = code
        return code


def _module_under_root(name):
    module = sys.modules.get(name)
    filename = getattr(module, '__file__', None) if module is not None else None
    return isinstance(filename, str) and _under(filename, (ROOT,))


def _purge(names):
    for name in set(names):
        if _module_under_root(name):
            sys.modules.pop(name, None)


def _find_plugin(module):
    candidate = getattr(module, 'plugin', None)
    if isinstance(candidate, BasePlugin):
        return candidate, ''
    instances = []
    subclasses = []
    for value in vars(module).values():
        if isinstance(value, BasePlugin):
            instances.append(value)
        elif (isinstance(value, type)
              and value is not BasePlugin
              and issubclass(value, BasePlugin)):
            subclasses.append(value)
    if len(instances) > 1:
        return None, 'Plugin declares more than one BasePlugin instance.'
    if len(instances) == 1:
        return instances[0], ''
    if len(subclasses) > 1:
        return None, 'Plugin declares more than one BasePlugin subclass.'
    if len(subclasses) == 1:
        try:
            return subclasses[0](), ''
        except BaseException as error:
            return None, 'Plugin could not be created: %s' % error
    return None, 'Plugin must declare a BasePlugin instance or subclass.'


def _load(message):
    seq = _CURRENT_SEQ[0]
    plugin_id = message.get('id')
    if not isinstance(plugin_id, str) or not ID_RE.match(plugin_id):
        _send({'seq': seq, 'type': 'error', 'message': 'Invalid plugin id.'})
        return
    path = message.get('path')
    if not isinstance(path, str) or not os.path.isabs(path):
        _send({'seq': seq, 'type': 'error',
               'message': 'Plugin path is not absolute.'})
        return
    real = _real(path)
    if os.path.dirname(real) != ROOT:
        _send({'seq': seq, 'type': 'error',
               'message': 'Plugin path is outside the plugins directory.'})
        return
    if not real.lower().endswith(SUPPORTED):
        _send({'seq': seq, 'type': 'error',
               'message': 'Unsupported plugin format.'})
        return
    if os.path.islink(path):
        _send({'seq': seq, 'type': 'error',
               'message': 'Plugin path is a symlink.'})
        return
    try:
        info = os.stat(path, follow_symlinks=False)
    except OSError as error:
        _send({'seq': seq, 'type': 'error',
               'message': 'Plugin file is missing: %s' % error})
        return
    if not stat.S_ISREG(info.st_mode):
        _send({'seq': seq, 'type': 'error',
               'message': 'Plugin path is not a regular file.'})
        return
    flags = (os.O_RDONLY
             | getattr(os, 'O_NOFOLLOW', 0)
             | getattr(os, 'O_BINARY', 0))
    try:
        descriptor = os.open(path, flags)
    except OSError as error:
        _send({'seq': seq, 'type': 'error',
               'message': 'Plugin file could not be opened: %s' % error})
        return
    chunks = []
    total = 0
    try:
        info = os.fstat(descriptor)
        if not stat.S_ISREG(info.st_mode):
            _send({'seq': seq, 'type': 'error',
                   'message': 'Plugin path is not a regular file.'})
            return
        if info.st_size > MAX_BYTES:
            _send({'seq': seq, 'type': 'error',
                   'message': 'Plugin file is larger than 2 MiB.'})
            return
        while True:
            chunk = os.read(descriptor, 65536)
            if not chunk:
                break
            total += len(chunk)
            if total > MAX_BYTES:
                _send({'seq': seq, 'type': 'error',
                       'message': 'Plugin file is larger than 2 MiB.'})
                return
            chunks.append(chunk)
    finally:
        os.close(descriptor)
    data = b''.join(chunks)

    existing = _PLUGINS.get(plugin_id)
    if existing is not None:
        if existing['path'] == real:
            _send({'seq': seq, 'type': 'loaded', 'ok': True})
        else:
            _send({'seq': seq, 'type': 'error',
                   'message': 'Plugin id is already loaded from another path.'})
        return

    module_name = 'exteraless_plugin_' + plugin_id
    before = set(sys.modules)
    _CURRENT_ID[0] = plugin_id
    try:
        loader = _PluginLoader(module_name, real, data)
        spec = importlib.util.spec_from_file_location(
            module_name, real, loader=loader)
        module = importlib.util.module_from_spec(spec)
        sys.modules[module_name] = module
        spec.loader.exec_module(module)
    except BaseException as error:
        sys.modules.pop(module_name, None)
        _purge(set(sys.modules) - before)
        _send({'seq': seq, 'type': 'error',
               'message': 'Plugin load failed: %s' % error})
        return
    finally:
        _LOAD_CODE[0] = None

    instance, error = _find_plugin(module)
    if error:
        _purge({module_name})
        _send({'seq': seq, 'type': 'error', 'message': error})
        return
    declared = getattr(module, '__id__', None)
    if declared != plugin_id:
        _purge({module_name})
        _send({'seq': seq, 'type': 'error',
               'message': 'Plugin id mismatch: expected %s, got %s.' % (
                   plugin_id, declared)})
        return

    modules = {
        name for name in set(sys.modules) - before if _module_under_root(name)
    }
    modules.add(module_name)
    record = {
        'module': module,
        'instance': instance,
        'path': real,
        'modules': modules,
    }
    _PLUGINS[plugin_id] = record
    try:
        instance.on_load()
    except BaseException as error:
        _PLUGINS.pop(plugin_id, None)
        try:
            instance.on_unload()
        except BaseException:
            pass
        _purge(modules)
        _send({'seq': seq, 'type': 'error',
               'message': 'Plugin load failed: %s' % error})
        return
    _send({'seq': seq, 'type': 'loaded', 'ok': True})


def _unload(message):
    seq = _CURRENT_SEQ[0]
    plugin_id = message.get('id')
    record = None
    if isinstance(plugin_id, str):
        record = _PLUGINS.pop(plugin_id, None)
    if record is None:
        _send({'seq': seq, 'type': 'unloaded', 'ok': True})
        return
    warning = ''
    _CURRENT_ID[0] = plugin_id
    try:
        record['instance'].on_unload()
    except BaseException as error:
        warning = 'on_unload failed: %s' % error
    _purge(record['modules'])
    response = {'seq': seq, 'type': 'unloaded', 'ok': True}
    if warning:
        response['warning'] = warning
    _send(response)


def _shutdown():
    for plugin_id in list(_PLUGINS):
        record = _PLUGINS.get(plugin_id)
        if record is None:
            continue
        _CURRENT_ID[0] = plugin_id
        try:
            record['instance'].on_unload()
        except BaseException:
            pass
        _purge(record['modules'])
    _PLUGINS.clear()
    _send({'seq': _CURRENT_SEQ[0], 'type': 'bye'})


def _handle(line):
    try:
        message = json.loads(line)
    except ValueError:
        _send({'seq': 0, 'type': 'error', 'message': 'Invalid command.'})
        return True
    if not isinstance(message, dict):
        _send({'seq': 0, 'type': 'error', 'message': 'Invalid command.'})
        return True
    seq = message.get('seq')
    _CURRENT_SEQ[0] = seq if isinstance(seq, int) else 0
    command = message.get('cmd')
    if command == 'shutdown':
        _shutdown()
        return False
    if command == 'load':
        _load(message)
    elif command == 'unload':
        _unload(message)
    else:
        _send({'seq': _CURRENT_SEQ[0], 'type': 'error',
               'message': 'Unknown command.'})
    _CURRENT_ID[0] = ''
    _CURRENT_SEQ[0] = 0
    return True


KEYCTL_JOIN_SESSION_KEYRING = 1
KEYCTL_SYSCALL = {
    'aarch64': 219,
    'armv7l': 311,
    'i686': 288,
    'ppc64le': 240,
    'riscv64': 219,
    's390x': 280,
    'x86_64': 250,
}


def _close_library_fds():
    # Importing ctypes leaves an fd on libffi; the host keeps only its own.
    try:
        names = os.listdir('/proc/self/fd')
    except OSError:
        return
    for name in names:
        try:
            target = os.readlink('/proc/self/fd/' + name)
        except OSError:
            continue
        if 'libffi' in target or '_ctypes' in target:
            try:
                os.close(int(name))
            except (OSError, ValueError):
                pass


def _join_session_keyring():
    # Best-effort: bwrap leaves session keyrings shared, so drop the inherited one.
    try:
        import ctypes
        libc = ctypes.CDLL(None, use_errno=True)
        symbol = getattr(libc, 'keyctl', None)
        if symbol is not None:
            symbol.restype = ctypes.c_long
            return symbol(
                KEYCTL_JOIN_SESSION_KEYRING, ctypes.c_ulong(0), 0, 0)
        number = KEYCTL_SYSCALL.get(os.uname().machine)
        if number is None:
            return -1
        libc.syscall.restype = ctypes.c_long
        return libc.syscall(
            ctypes.c_long(number),
            ctypes.c_long(KEYCTL_JOIN_SESSION_KEYRING),
            ctypes.c_ulong(0),
            0,
            0)
    except Exception:
        return -1
    finally:
        _close_library_fds()


def _install():
    sys.addaudithook(_audit)
    base = types.ModuleType('base_plugin')
    base.BasePlugin = BasePlugin
    sys.modules['base_plugin'] = base


def main():
    _join_session_keyring()
    _install()
    _send({'type': 'ready', 'api': 1})
    while True:
        try:
            line = INPUT.readline()
        except OSError:
            return
        if not line:
            return
        text = line.decode('utf-8', 'replace').strip()
        if not text:
            continue
        if not _handle(text):
            return


main()
)PY";

Runtime *gRuntime = nullptr;

[[nodiscard]] QString StderrOf(QProcess *process) {
	return process
		? QString::fromUtf8(process->readAllStandardError()).trimmed()
		: QString();
}

[[nodiscard]] QString FindBwrap() {
	const auto overridePath = qEnvironmentVariable("EXTERALESS_BWRAP").trimmed();
	if (!overridePath.isEmpty()) {
		return QStandardPaths::findExecutable(overridePath);
	}
	return QStandardPaths::findExecutable(QStringLiteral("bwrap"));
}

[[nodiscard]] QStringList ReadOnlyBinds() {
	auto result = QStringList();
	for (const auto &path : {
			QStringLiteral("/nix"),
			QStringLiteral("/run/current-system/sw"),
			QStringLiteral("/usr"),
			QStringLiteral("/bin"),
			QStringLiteral("/lib"),
			QStringLiteral("/lib64") }) {
		if (QFileInfo::exists(path)) {
			result
				<< QStringLiteral("--ro-bind")
				<< path
				<< path;
		}
	}
	for (const auto &path : {
			QStringLiteral("/etc/resolv.conf"),
			QStringLiteral("/etc/hosts"),
			QStringLiteral("/etc/nsswitch.conf"),
			QStringLiteral("/etc/passwd"),
			QStringLiteral("/etc/group"),
			QStringLiteral("/etc/localtime") }) {
		if (QFileInfo::exists(path)) {
			result
				<< QStringLiteral("--ro-bind")
				<< path
				<< path;
		}
	}
	return result;
}

[[nodiscard]] QString SandboxPath() {
	for (const auto &path : {
			QStringLiteral("/run/current-system/sw/bin"),
			QStringLiteral("/usr/local/bin"),
			QStringLiteral("/usr/bin"),
			QStringLiteral("/bin") }) {
		if (QFileInfo(path).isDir()) {
			return path;
		}
	}
	return QString();
}

[[nodiscard]] QStringList SandboxArguments(
		const QString &pluginsRoot,
		const QString &python,
		const QString &script,
		const QString &sandboxPath) {
	return QStringList{
		QStringLiteral("--unshare-all"),
		QStringLiteral("--unshare-user"),
		QStringLiteral("--disable-userns"),
		QStringLiteral("--die-with-parent"),
		QStringLiteral("--new-session"),
		QStringLiteral("--cap-drop"),
		QStringLiteral("ALL"),
		QStringLiteral("--proc"),
		QStringLiteral("/proc"),
		QStringLiteral("--dev"),
		QStringLiteral("/dev"),
		QStringLiteral("--tmpfs"),
		QStringLiteral("/tmp"),
	} + ReadOnlyBinds() + QStringList{
		QStringLiteral("--ro-bind"),
		pluginsRoot,
		QStringLiteral("/plugins"),
		QStringLiteral("--remount-ro"),
		QStringLiteral("/"),
		QStringLiteral("--chdir"),
		QStringLiteral("/"),
		QStringLiteral("--clearenv"),
		QStringLiteral("--setenv"),
		QStringLiteral("PATH"),
		sandboxPath,
		QStringLiteral("--setenv"),
		QStringLiteral("HOME"),
		QStringLiteral("/tmp"),
		QStringLiteral("--setenv"),
		QStringLiteral("TMPDIR"),
		QStringLiteral("/tmp"),
		QStringLiteral("--setenv"),
		QStringLiteral("LANG"),
		QStringLiteral("C.UTF-8"),
		QStringLiteral("--"),
		python,
		QStringLiteral("-I"),
		QStringLiteral("-B"),
		QStringLiteral("-c"),
		script,
		QStringLiteral("/plugins"),
	};
}

} // namespace

Runtime::Runtime(QString pluginsDirectory, QString auditLogPath)
: _pluginsDirectory(std::move(pluginsDirectory))
, _auditLogPath(std::move(auditLogPath)) {
}

Runtime::~Runtime() {
	stop();
}

void Runtime::Start(QString pluginsDirectory, QString auditLogPath) {
	if (gRuntime
		&& gRuntime->_pluginsDirectory == pluginsDirectory
		&& gRuntime->_auditLogPath == auditLogPath) {
		return;
	}
	Stop();
	gRuntime = new Runtime(std::move(pluginsDirectory), std::move(auditLogPath));
}

Runtime *Runtime::Instance() {
	return gRuntime;
}

void Runtime::Stop() {
	delete gRuntime;
	gRuntime = nullptr;
}

PathCheck Runtime::ValidatePath(
		const QString &pluginsDirectory,
		const QString &path) {
	const auto rootInfo = QFileInfo(pluginsDirectory);
	if (pluginsDirectory.isEmpty() || !rootInfo.exists() || !rootInfo.isDir()) {
		return {
			false,
			QString(),
			QStringLiteral("Plugins directory is missing."),
		};
	}
	const auto canonicalRoot = rootInfo.canonicalFilePath();
	if (canonicalRoot.isEmpty()) {
		return {
			false,
			QString(),
			QStringLiteral("Plugins directory is missing."),
		};
	}
	if (path.isEmpty()) {
		return { false, QString(), QStringLiteral("Plugin file is missing.") };
	}
	const auto info = QFileInfo(path);
	if (info.isSymLink()) {
		return { false, QString(), QStringLiteral("Plugin path is a symlink.") };
	}
	if (!info.exists()) {
		return { false, QString(), QStringLiteral("Plugin file is missing.") };
	}
	if (!info.isFile()) {
		return {
			false,
			QString(),
			QStringLiteral("Plugin path is not a regular file."),
		};
	}
	const auto canonical = info.canonicalFilePath();
	if (canonical.isEmpty()) {
		return { false, QString(), QStringLiteral("Plugin file is missing.") };
	}
	const auto canonicalInfo = QFileInfo(canonical);
	if (canonicalInfo.isSymLink()) {
		return { false, QString(), QStringLiteral("Plugin path is a symlink.") };
	}
	if (!canonicalInfo.isFile()) {
		return {
			false,
			QString(),
			QStringLiteral("Plugin path is not a regular file."),
		};
	}
	const auto relative = QDir(canonicalRoot).relativeFilePath(canonical);
	if (relative.isEmpty()
		|| relative.startsWith(QStringLiteral(".."))
		|| relative.contains(QLatin1Char('/'))
		|| relative.contains(QLatin1Char('\\'))) {
		return {
			false,
			QString(),
			QStringLiteral(
				"Plugin is outside the plugins directory or not directly "
				"inside it."),
		};
	}
	const auto lower = canonical.toLower();
	if (!lower.endsWith(QStringLiteral(".plugin"))
		&& !lower.endsWith(QStringLiteral(".py"))) {
		return { false, QString(), QStringLiteral("Unsupported plugin format.") };
	}
	return { true, canonical, QString() };
}

bool Runtime::isRunning() const {
	return _process && _process->state() == QProcess::Running;
}

bool Runtime::isLoaded(const QString &id) const {
	return isRunning() && _loaded.contains(id);
}

qint64 Runtime::hostPid() const {
	return isRunning() ? _process->processId() : 0;
}

const QString &Runtime::pluginsDirectory() const {
	return _pluginsDirectory;
}

Result Runtime::enable(const QString &id, const QString &path) {
	const auto check = ValidatePath(_pluginsDirectory, path);
	if (!check.ok) {
		return { false, check.error };
	}
	const auto canonical = check.canonicalPath;
	const auto filename = QFileInfo(canonical).fileName();
	if (filename.isEmpty()
		|| filename == QStringLiteral(".")
		|| filename == QStringLiteral("..")
		|| filename.contains(QLatin1Char('/'))
		|| filename.contains(QLatin1Char('\\'))) {
		return { false, QStringLiteral("Plugin file is missing.") };
	}
	if (isRunning() && _loaded.value(id) == canonical) {
		return { true, QString() };
	}
	if (_loaded.contains(id)) {
		const auto disabled = disable(id);
		if (!disabled.ok) {
			return disabled;
		}
	}
	if (!isRunning()) {
		clearHost();
		auto error = QString();
		if (!startHost(&error)) {
			return { false, error };
		}
	}
	auto request = QJsonObject{
		{ QStringLiteral("seq"), ++_seq },
		{ QStringLiteral("cmd"), QStringLiteral("load") },
		{ QStringLiteral("id"), id },
		{ QStringLiteral("path"), QStringLiteral("/plugins/") + filename },
	};
	auto response = QJsonObject();
	auto error = QString();
	if (!sendRequest(request, &response, &error)) {
		return { false, error };
	}
	const auto type = response.value(QStringLiteral("type")).toString();
	if (type == QStringLiteral("loaded")) {
		const auto ok = response.value(QStringLiteral("ok"));
		if (!ok.isBool() || !ok.toBool()) {
			protocolViolation(&error);
			return { false, error };
		}
		_loaded[id] = canonical;
		return { true, QString() };
	}
	if (type == QStringLiteral("error")) {
		return { false, response.value(QStringLiteral("message")).toString() };
	}
	protocolViolation(&error);
	return { false, error };
}

Result Runtime::disable(const QString &id) {
	if (!isRunning()) {
		clearHost();
		return { true, QString() };
	}
	if (!_loaded.contains(id)) {
		return { true, QString() };
	}
	auto request = QJsonObject{
		{ QStringLiteral("seq"), ++_seq },
		{ QStringLiteral("cmd"), QStringLiteral("unload") },
		{ QStringLiteral("id"), id },
	};
	auto response = QJsonObject();
	auto error = QString();
	if (!sendRequest(request, &response, &error)) {
		return { false, error };
	}
	const auto type = response.value(QStringLiteral("type")).toString();
	if (type == QStringLiteral("unloaded")) {
		const auto ok = response.value(QStringLiteral("ok"));
		if (!ok.isBool() || !ok.toBool()) {
			protocolViolation(&error);
			return { false, error };
		}
		_loaded.remove(id);
		const auto warning = response.value(QStringLiteral("warning")).toString();
		if (!warning.isEmpty()) {
			writeAuditRecord(
				QStringLiteral("log"),
				id,
				QStringLiteral("log"),
				warning);
		}
		return { true, QString() };
	}
	if (type == QStringLiteral("error")) {
		clearHost();
		return { false, response.value(QStringLiteral("message")).toString() };
	}
	protocolViolation(&error);
	return { false, error };
}

void Runtime::stop() {
	if (_process && _process->state() == QProcess::Running) {
		const auto request = QJsonObject{
			{ QStringLiteral("cmd"), QStringLiteral("shutdown") },
		};
		auto payload = QJsonDocument(request).toJson(QJsonDocument::Compact);
		payload.append('\n');
		_process->write(payload);
		_process->waitForBytesWritten(kWriteTimeout);
		if (!_process->waitForFinished(kStopTimeout)) {
			_process->kill();
			_process->waitForFinished(kKillTimeout);
		}
	}
	clearHost();
}

void Runtime::clearHost() {
	if (_process) {
		if (_process->state() != QProcess::NotRunning) {
			_process->kill();
			_process->waitForFinished(kKillTimeout);
		}
		_process.reset();
	}
	_readBuffer.clear();
	_loaded.clear();
	_seq = 0;
}

bool Runtime::startHost(QString *error) {
	const auto python = QStandardPaths::findExecutable(
		QStringLiteral("python3"));
	if (python.isEmpty()) {
		*error = QStringLiteral("Python 3 was not found.");
		return false;
	}
	const auto canonicalPython = QFileInfo(python).canonicalFilePath();
	if (canonicalPython.isEmpty()) {
		*error = QStringLiteral("Python 3 was not found.");
		return false;
	}
	const auto bwrap = FindBwrap();
	if (bwrap.isEmpty()) {
		*error = QString::fromUtf8(kSandboxUnavailable);
		return false;
	}
	const auto canonicalRoot = QFileInfo(_pluginsDirectory).canonicalFilePath();
	if (canonicalRoot.isEmpty() || !QFileInfo(canonicalRoot).isDir()) {
		*error = QStringLiteral("Plugins directory is missing.");
		return false;
	}
	auto sandboxPath = SandboxPath();
	if (sandboxPath.isEmpty()) {
		sandboxPath = QFileInfo(canonicalPython).absolutePath();
	}
	if (!QFileInfo(sandboxPath).isDir()) {
		*error = QString::fromUtf8(kSandboxUnavailable);
		return false;
	}
	_process = std::make_unique<QProcess>();
	_process->setProcessEnvironment(QProcessEnvironment());
	_process->setProgram(bwrap);
	_process->setArguments(SandboxArguments(
		canonicalRoot,
		canonicalPython,
		QString::fromUtf8(kHostScript),
		sandboxPath));
	_process->start();
	if (!_process->waitForStarted(kStartTimeout)) {
		clearHost();
		*error = QStringLiteral("Plugin host failed to start.");
		return false;
	}
	auto message = QJsonObject();
	auto readError = QString();
	if (!readMessage(kStartTimeout, 0, true, &message, &readError)
		|| message.value(QStringLiteral("type")).toString()
			!= QStringLiteral("ready")) {
		clearHost();
		*error = QStringLiteral("Plugin host failed to start.");
		if (!readError.isEmpty()) {
			*error += QStringLiteral(" ") + readError;
		}
		return false;
	}
	return true;
}

void Runtime::protocolViolation(QString *error) {
	clearHost();
	*error = QString::fromUtf8(kProtocolViolation);
}

bool Runtime::sendRequest(
		const QJsonObject &request,
		QJsonObject *response,
		QString *error) {
	if (!isRunning()) {
		*error = QStringLiteral("Plugin host is not running.");
		return false;
	}
	auto payload = QJsonDocument(request).toJson(QJsonDocument::Compact);
	payload.append('\n');
	if (_process->write(payload) != payload.size()) {
		clearHost();
		*error = QStringLiteral("Plugin host exited.");
		return false;
	}
	_process->waitForBytesWritten(kWriteTimeout);
	return readMessage(
		kCommandTimeout,
		request.value(QStringLiteral("seq")).toInt(),
		false,
		response,
		error);
}

bool Runtime::readMessage(
		int timeout,
		int expectedSeq,
		bool returnReady,
		QJsonObject *response,
		QString *error) {
	const auto deadline = QDeadlineTimer(timeout);
	while (true) {
		if (_process) {
			_readBuffer += _process->readAllStandardOutput();
		}
		if (_readBuffer.size() > kMaxReadBuffer) {
			clearHost();
			*error = QStringLiteral("Plugin host returned too much output.");
			return false;
		}
		while (true) {
			const auto newline = _readBuffer.indexOf('\n');
			if (newline < 0) {
				break;
			}
			const auto line = _readBuffer.left(newline);
			_readBuffer.remove(0, newline + 1);
			if (line.trimmed().isEmpty()) {
				continue;
			}
			auto parseError = QJsonParseError{0, QJsonParseError::NoError};
			const auto document = QJsonDocument::fromJson(line, &parseError);
			if (parseError.error != QJsonParseError::NoError
				|| !document.isObject()) {
				clearHost();
				*error = QString::fromUtf8(kInvalidOutput);
				return false;
			}
			const auto object = document.object();
			const auto type = object.value(QStringLiteral("type")).toString();
			if (type == QStringLiteral("ready")) {
				if (returnReady && expectedSeq == 0) {
					*response = object;
					return true;
				}
				protocolViolation(error);
				return false;
			}
			const auto seqValue = object.value(QStringLiteral("seq"));
			const auto knownSeq = seqValue.isDouble()
				&& (seqValue.toInt() == expectedSeq)
				&& (expectedSeq != 0);
			if (type == QStringLiteral("audit")) {
				if (!knownSeq) {
					qWarning("Dropped an unsolicited plugin host audit event.");
					continue;
				}
				writeAuditRecord(
					QStringLiteral("denied"),
					object.value(QStringLiteral("id")).toString(),
					object.value(QStringLiteral("event")).toString(),
					object.value(QStringLiteral("detail")).toString());
				continue;
			}
			if (type == QStringLiteral("log")) {
				if (!knownSeq) {
					qWarning("Dropped an unsolicited plugin host log message.");
					continue;
				}
				writeAuditRecord(
					QStringLiteral("log"),
					object.value(QStringLiteral("id")).toString(),
					QStringLiteral("log"),
					object.value(QStringLiteral("message")).toString());
				continue;
			}
			if (!knownSeq
				|| (type != QStringLiteral("loaded")
					&& type != QStringLiteral("unloaded")
					&& type != QStringLiteral("error"))) {
				protocolViolation(error);
				return false;
			}
			*response = object;
			return true;
		}
		if (!isRunning()) {
			const auto stderrText = StderrOf(_process.get());
			clearHost();
			*error = stderrText.isEmpty()
				? QStringLiteral("Plugin host exited.")
				: QStringLiteral("Plugin host exited: %1").arg(stderrText);
			return false;
		}
		if (deadline.hasExpired()) {
			clearHost();
			*error = QStringLiteral("Plugin host timed out.");
			return false;
		}
		const auto remaining = deadline.remainingTime();
		_process->waitForReadyRead(int(std::clamp<qint64>(
			remaining,
			1,
			50)));
	}
}

void Runtime::writeAuditRecord(
		const QString &kind,
		const QString &id,
		const QString &event,
		const QString &detail) {
	const auto directory = QFileInfo(_auditLogPath).absolutePath();
	if (!QDir().mkpath(directory)) {
		qWarning("Could not create the plugin audit log directory.");
		return;
	}
	auto file = QFile(_auditLogPath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
		qWarning("Could not open the plugin audit log for writing.");
		return;
	}
	const auto record = QJsonObject{
		{
			QStringLiteral("time"),
			QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
		},
		{ QStringLiteral("kind"), kind },
		{ QStringLiteral("plugin"), id },
		{ QStringLiteral("event"), event },
		{ QStringLiteral("detail"), detail },
	};
	file.write(QJsonDocument(record).toJson(QJsonDocument::Compact));
	file.write("\n");
}

} // namespace Exteraless::Plugins
