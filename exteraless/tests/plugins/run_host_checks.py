#!/usr/bin/env python3
"""Backend regression checks for the Exteraless Python plugin host."""

import ctypes
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
DEFAULT_CLI = (
    ROOT / "out" / "exteraless" / "tests" / "plugins"
    / "exteraless-plugin-host-cli")
FIXTURES = Path(__file__).resolve().parent / "fixtures" / "runtime"
RUNTIME_CPP = (
    ROOT / "Telegram" / "SourceFiles" / "exteraless" / "plugin_runtime.cpp")
RUN_TIMEOUT = 90

STARTUP_SOURCES = [
    ROOT / "Telegram" / "SourceFiles" / "core" / "application.cpp",
    ROOT / "Telegram" / "SourceFiles" / "core" / "application.h",
    ROOT / "Telegram" / "SourceFiles" / "core" / "launcher.cpp",
    ROOT / "Telegram" / "SourceFiles" / "core" / "launcher.h",
    ROOT / "Telegram" / "SourceFiles" / "main",
]
STARTUP_HOST_MARKERS = ("Runtime::Start", "Runtime::Instance")

SANDBOX_PREFIX = [
    "--unshare-all",
    "--unshare-user",
    "--disable-userns",
    "--die-with-parent",
    "--new-session",
    "--cap-drop",
    "ALL",
    "--proc",
    "/proc",
    "--dev",
    "/dev",
    "--tmpfs",
    "/tmp",
]

SYSTEM_BIND_PATHS = [
    "/nix",
    "/run/current-system/sw",
    "/usr",
    "/bin",
    "/lib",
    "/lib64",
]

ETC_BIND_PATHS = [
    "/etc/resolv.conf",
    "/etc/hosts",
    "/etc/nsswitch.conf",
    "/etc/passwd",
    "/etc/group",
    "/etc/localtime",
]

SANDBOX_PATH_CANDIDATES = [
    "/run/current-system/sw/bin",
    "/usr/local/bin",
    "/usr/bin",
    "/bin",
]

REQUIRED_SANDBOX_FLAGS = [
    ["--unshare-all"],
    ["--unshare-user"],
    ["--disable-userns"],
    ["--cap-drop", "ALL"],
    ["--remount-ro"],
]

FORBIDDEN_SANDBOX_FLAGS = [
    "--share-net",
    "--bind",
    "--dev-bind",
    "--cap-add",
]

KEY_SPEC_SESSION_KEYRING = -3
KEYCTL_GET_KEYRING_ID = 0
KEYCTL_SYSCALL = {
    "aarch64": 219,
    "armv7l": 311,
    "i686": 288,
    "ppc64le": 240,
    "riscv64": 219,
    "s390x": 280,
    "x86_64": 250,
}

EVIDENCE_SCRIPT = """
import ctypes
import json
import os
import socket

def attempt(path):
    try:
        with open(path, "w") as handle:
            handle.write("x")
    except OSError as error:
        return str(error.errno)
    return "allowed"

try:
    sock = socket.socket()
    sock.settimeout(2)
    sock.connect(("1.1.1.1", 53))
    network = "connected"
except OSError as error:
    network = str(error.errno)
status = open("/proc/self/status").read()
caps = [
    line.split()[1] for line in status.splitlines()
    if line.startswith("CapEff:")
][0]
nnp = [
    line.split()[1] for line in status.splitlines()
    if line.startswith("NoNewPrivs:")
][0]
libc = ctypes.CDLL(None, use_errno=True)
os.makedirs("/tmp/mountpoint", exist_ok=True)
mounted = libc.mount(
    b"tmpfs", b"/tmp/mountpoint", b"tmpfs", 0, b"")
mount_errno = ctypes.get_errno()
remounted = libc.mount(None, b"/", None, 32, None)
remount_errno = ctypes.get_errno()
print(json.dumps({
    "uid": os.getuid(),
    "pid1": open("/proc/1/cmdline", "rb").read().split(b"\\0")[0].decode(),
    "home": os.path.exists("/home"),
    "root": attempt("/root-probe"),
    "plugins": attempt("/plugins/probe"),
    "nix": attempt("/nix/probe"),
    "tmp": attempt("/tmp/probe"),
    "caps": caps,
    "nnp": nnp,
    "net": network,
    "mount": [mounted, mount_errno],
    "remount": [remounted, remount_errno],
    "proc1_home": os.path.exists("/proc/1/root/home"),
    "proc1_nix": os.path.exists("/proc/1/root/nix"),
    "repo": os.path.exists("__REPO__"),
    "tdata": os.path.exists("__TDATA__"),
    "cwd": os.getcwd(),
}))
"""

KEYRING_PROBE = """
import ctypes
import json
import os

KEY_SPEC_SESSION_KEYRING = -3

__JOIN__


def keyctl(op, arg=0):
    libc = ctypes.CDLL(None, use_errno=True)
    symbol = getattr(libc, 'keyctl', None)
    if symbol is not None:
        symbol.restype = ctypes.c_long
        return symbol(op, ctypes.c_ulong(arg), 0, 0)
    number = KEYCTL_SYSCALL.get(os.uname().machine)
    if number is None:
        return -1
    libc.syscall.restype = ctypes.c_long
    return libc.syscall(
        ctypes.c_long(number), ctypes.c_long(op), ctypes.c_ulong(arg), 0, 0)


def keys():
    try:
        with open('/proc/keys') as handle:
            return handle.read().splitlines()
    except OSError:
        return None


before = keyctl(0, KEY_SPEC_SESSION_KEYRING)
joined = _join_session_keyring()
after = keyctl(0, KEY_SPEC_SESSION_KEYRING)
print(json.dumps({
    'before': before,
    'joined': joined,
    'after': after,
    'keys': keys(),
}))
"""


def find_bwrap():
    override = os.environ.get("EXTERALESS_BWRAP", "").strip()
    if override:
        found = shutil.which(override)
        return found
    found = shutil.which("bwrap")
    if found:
        return found
    patterns = [
        "*bubblewrap-0.12.0*/bin/bwrap",
        "*bubblewrap-*/bin/bwrap",
    ]
    for pattern in patterns:
        for path in sorted(Path("/nix/store").glob(pattern)):
            if os.access(path, os.X_OK):
                return str(path)
    return None


BWRAP = find_bwrap()


def sandbox_tokens():
    tokens = list(SANDBOX_PREFIX)
    for path in SYSTEM_BIND_PATHS + ETC_BIND_PATHS:
        if os.path.exists(path):
            tokens += ["--ro-bind", path, path]
    return tokens


def sandbox_path():
    for path in SANDBOX_PATH_CANDIDATES:
        if os.path.isdir(path):
            return path
    python = shutil.which("python3")
    if python is None:
        return "/"
    return str(Path(os.path.realpath(python)).parent)


def bwrap_command(plugins, payload):
    return [
        BWRAP,
        *sandbox_tokens(),
        "--ro-bind",
        str(plugins),
        "/plugins",
        "--remount-ro",
        "/",
        "--chdir",
        "/",
        "--clearenv",
        "--setenv",
        "PATH",
        sandbox_path(),
        "--setenv",
        "HOME",
        "/tmp",
        "--setenv",
        "TMPDIR",
        "/tmp",
        "--setenv",
        "LANG",
        "C.UTF-8",
        "--",
        str(Path(os.path.realpath(shutil.which("python3")))),
        "-I",
        "-B",
        "-c",
        payload,
        "/plugins",
    ]


class Ledger:
    def __init__(self):
        self.checks = 0
        self.failures = []
        self.skips = []

    def check(self, name, condition, detail=""):
        self.checks += 1
        if condition:
            print(f"PASS {name}")
        else:
            suffix = f": {detail}" if detail else ""
            print(f"FAIL {name}{suffix}")
            self.failures.append(name)

    def skip(self, name, detail=""):
        suffix = f": {detail}" if detail else ""
        print(f"SKIP {name}{suffix}")
        self.skips.append(name)


def workspace(work, name):
    base = work / name
    plugins = base / "plugins"
    tdata = base / "tdata"
    plugins.mkdir(parents=True)
    tdata.mkdir()
    return base, plugins, tdata / "plugins-audit.log"


def copy_fixture(plugins, name):
    target = plugins / name
    shutil.copyfile(FIXTURES / name, target)
    return target


def write_plugin(plugins, name, body):
    target = plugins / name
    target.write_text(body, encoding="utf-8")
    return target


def cli_env(data_home, extra=None):
    env = dict(os.environ)
    env["XDG_DATA_HOME"] = str(data_home)
    env["XDG_CACHE_HOME"] = str(data_home.parent / "cache")
    if BWRAP is not None:
        env["EXTERALESS_BWRAP"] = BWRAP
    if extra:
        env.update(extra)
    return env


def run_cli(cli, plugins, audit, args, data_home, extra_env=None,
            timeout=RUN_TIMEOUT):
    env = cli_env(data_home, extra_env)
    result = subprocess.run(
        [str(cli), str(plugins), str(audit), *args],
        capture_output=True,
        text=True,
        env=env,
        timeout=timeout)
    if result.returncode != 0:
        raise RuntimeError(
            f"CLI exited {result.returncode} for {args}: "
            f"{result.stderr.strip()}")
    return json.loads(result.stdout)


def audit_records(path):
    records = []
    if not path.exists():
        return records
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            records.append(json.loads(line))
        except ValueError:
            continue
    return records


def has_event(records, plugin, event):
    return any(
        record.get("kind") == "denied"
        and record.get("plugin") == plugin
        and record.get("event") == event
        for record in records)


def has_log(records, plugin, detail):
    return any(
        record.get("kind") == "log"
        and record.get("plugin") == plugin
        and record.get("event") == "log"
        and record.get("detail") == detail
        for record in records)


def has_any_log(records, detail):
    return any(
        record.get("kind") == "log"
        and record.get("event") == "log"
        and record.get("detail") == detail
        for record in records)


def host_pids(plugins_root):
    needle = str(plugins_root).encode()
    pids = []
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            command = (entry / "cmdline").read_bytes()
        except OSError:
            continue
        sandboxed = b"bubblewrap" in command and needle in command
        bare = b"exteraless_plugin_" in command and b"-I" in command
        if sandboxed or bare:
            pids.append(int(entry.name))
    return pids


def extract_block(name, marker):
    source = RUNTIME_CPP.read_text(encoding="utf-8")
    match = re.search(
        rf'{name}\s*=\s*R"{marker}\((.*?)\){marker}";',
        source,
        re.S)
    return match.group(1) if match else None


def extract_keyring_join(script):
    match = re.search(
        r"^KEYCTL_JOIN_SESSION_KEYRING = 1\n(?:[^\n]*\n)*?"
        r"^def _join_session_keyring\(\):\n(?:[ \t].*\n|\n)*",
        script,
        re.M)
    return match.group(0) if match else None


def keyctl_call(op, arg=0):
    libc = ctypes.CDLL(None, use_errno=True)
    symbol = getattr(libc, "keyctl", None)
    if symbol is not None:
        symbol.restype = ctypes.c_long
        return symbol(op, ctypes.c_ulong(arg), 0, 0)
    number = KEYCTL_SYSCALL.get(os.uname().machine)
    if number is None:
        return -1
    libc.syscall.restype = ctypes.c_long
    return libc.syscall(
        ctypes.c_long(number), ctypes.c_long(op), ctypes.c_ulong(arg), 0, 0)


def read_proc_keys():
    try:
        text = Path("/proc/keys").read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None
    return text.splitlines()


def session_keyring_serials(lines):
    serials = set()
    for line in lines or ():
        fields = line.split()
        if (len(fields) >= 9
                and fields[7] == "keyring"
                and fields[8].startswith("_ses:")):
            try:
                serials.add(int(fields[0], 16))
            except ValueError:
                continue
    return serials


def key_serials(lines):
    serials = set()
    for line in lines or ():
        fields = line.split()
        if not fields:
            continue
        try:
            serials.add(int(fields[0], 16))
        except ValueError:
            continue
    return serials


def host_session_keyring():
    if read_proc_keys() is None:
        return None
    ctypes.set_errno(0)
    serial = keyctl_call(KEYCTL_GET_KEYRING_ID, KEY_SPEC_SESSION_KEYRING)
    return serial if serial > 0 else None


def require_tool(name):
    path = shutil.which(name)
    return path


def tool_directory(work, name, tools, extra_scripts=None):
    directory = work / name
    directory.mkdir(parents=True, exist_ok=True)
    for tool in tools:
        target = require_tool(tool)
        if target is None:
            return None
        os.symlink(target, directory / tool)
    for script_name, body in (extra_scripts or {}).items():
        path = directory / script_name
        path.write_text(body, encoding="utf-8")
        path.chmod(0o755)
    return directory


def block_body(source, signature):
    start = source.find(signature)
    if start < 0:
        return None
    start = source.find("{", start)
    end = source.find("\n}\n", start)
    return source[start:end + 3] if 0 <= start < end else None


def check_static(ledger):
    script = extract_block("kHostScript", "PY")
    ledger.check("host script found", script is not None, str(RUNTIME_CPP))
    if script is not None:
        try:
            compile(script, "<host>", "exec")
        except SyntaxError as error:
            ledger.check("host script syntax", False, str(error))
        else:
            ledger.check("host script syntax", True)
    source = RUNTIME_CPP.read_text(encoding="utf-8")
    arguments = block_body(source, "QStringList SandboxArguments(")
    binds = block_body(source, "QStringList ReadOnlyBinds(")
    path_block = block_body(source, "QString SandboxPath(")
    ledger.check("sandbox arguments found", bool(arguments))
    position = 0
    ordered = bool(arguments)
    missing = ""
    for token in SANDBOX_PREFIX:
        found = arguments.find(f'"{token}"', position)
        if found < 0:
            ordered = False
            missing = token
            break
        position = found + len(token) + 2
    ledger.check(
        "sandbox arguments ordered",
        ordered,
        missing or (arguments or "")[:200])
    missing_required = ""
    required = bool(arguments) and bool(binds)
    sandbox_source = (arguments or "") + (binds or "")
    for tokens in REQUIRED_SANDBOX_FLAGS:
        found = sandbox_source.find(f'"{tokens[0]}"', 0)
        if found < 0:
            required = False
            missing_required = tokens[0]
            break
        if len(tokens) > 1 and sandbox_source.find(
                f'"{tokens[1]}"', found) < 0:
            required = False
            missing_required = " ".join(tokens)
            break
    ledger.check("sandbox: required flags present", required, missing_required)
    forbidden = [
        token for token in FORBIDDEN_SANDBOX_FLAGS
        if f'"{token}"' in source
    ]
    ledger.check(
        "sandbox: forbidden flags absent",
        not forbidden,
        forbidden)
    missing_binds = [
        path for path in SYSTEM_BIND_PATHS + ETC_BIND_PATHS
        if not binds or f'"{path}"' not in binds
    ]
    ledger.check(
        "sandbox: existing system binds",
        not missing_binds
        and '"--ro-bind"' in (binds or "")
        and "QFileInfo::exists" in (binds or ""),
        missing_binds)
    ledger.check(
        "sandbox: no unconditional system bind",
        bool(arguments) and '"/nix"' not in arguments,
        (arguments or "")[:200])
    clearenv = arguments.find('"--clearenv"') if arguments else -1
    setenv = arguments.find('"--setenv"') if arguments else -1
    ledger.check(
        "sandbox: environment scrubbed",
        clearenv >= 0
        and setenv > clearenv
        and all(f'"{key}"' in arguments for key in ("HOME", "TMPDIR", "LANG")),
        (arguments or "")[:200])
    ledger.check(
        "sandbox: PATH uses an existing directory",
        bool(arguments)
        and '"PATH"' in arguments
        and bool(path_block)
        and "QFileInfo(path).isDir()" in (path_block or "")
        and all(f'"{path}"' in path_block for path in SANDBOX_PATH_CANDIDATES),
        path_block)
    ledger.check(
        "sandbox: no unshare fallback",
        'QStringLiteral("unshare")' not in source
        and "kSandboxWrapper" not in source)
    ledger.check(
        "sandbox: no chroot wrapper",
        "chroot" not in source.lower())
    join_code = extract_keyring_join(script or "")
    ledger.check(
        "keyring: join code in host script",
        join_code is not None
        and "ctypes.CDLL" in join_code
        and "KEYCTL_JOIN_SESSION_KEYRING" in join_code)
    if script is not None:
        main_body = script[script.find("def main():"):]
        join_call = main_body.find("_join_session_keyring()")
        install_call = main_body.find("_install()")
        ledger.check(
            "keyring: join before hook and plugin load",
            join_call >= 0 and install_call > join_call,
            [join_call, install_call])


def check_bwrap_environment(ledger, work):
    ledger.check(
        "bwrap: found",
        BWRAP is not None,
        "EXTERALESS_BWRAP or PATH or /nix/store")
    if BWRAP is None:
        return
    version = subprocess.run(
        [BWRAP, "--version"],
        capture_output=True,
        text=True,
        timeout=30)
    ledger.check(
        "bwrap: version",
        version.returncode == 0
        and version.stdout.strip().startswith("bubblewrap"),
        f"{version.returncode} {version.stdout.strip()} "
        f"{version.stderr.strip()}")
    base, plugins, _ = workspace(work, "bwrap-evidence")
    payload = EVIDENCE_SCRIPT.replace(
        "__REPO__", str(ROOT)).replace(
        "__TDATA__", str(base / "tdata"))
    result = subprocess.run(
        bwrap_command(plugins, payload),
        capture_output=True,
        text=True,
        timeout=60)
    ledger.check(
        "bwrap: payload ran",
        result.returncode == 0,
        f"{result.returncode} {result.stderr.strip()[-400:]}")
    try:
        evidence = json.loads(result.stdout.strip().splitlines()[-1])
    except (ValueError, IndexError):
        evidence = {}
    ledger.check(
        "bwrap: pid 1 is bubblewrap",
        "bubblewrap" in str(evidence.get("pid1", "")),
        evidence.get("pid1"))
    ledger.check(
        "bwrap: uid preserved",
        evidence.get("uid") == os.getuid(),
        [evidence.get("uid"), os.getuid()])
    ledger.check("bwrap: home hidden", evidence.get("home") is False, evidence)
    ledger.check("bwrap: root read-only", evidence.get("root") == "30", evidence)
    ledger.check(
        "bwrap: plugins read-only",
        evidence.get("plugins") == "30",
        evidence)
    if os.path.exists("/nix"):
        ledger.check(
            "bwrap: nix read-only",
            evidence.get("nix") == "30",
            evidence)
    else:
        ledger.skip("bwrap: nix read-only", "/nix is absent")
    ledger.check("bwrap: tmp writable", evidence.get("tmp") == "allowed", evidence)
    ledger.check(
        "bwrap: sandbox PATH exists",
        os.path.isdir(sandbox_path()),
        sandbox_path())
    ledger.check(
        "bwrap: capabilities dropped",
        evidence.get("caps") == "0000000000000000",
        evidence.get("caps"))
    ledger.check(
        "bwrap: no new privileges",
        evidence.get("nnp") == "1",
        evidence.get("nnp"))
    ledger.check(
        "bwrap: network unreachable",
        evidence.get("net") == "101",
        evidence.get("net"))
    ledger.check(
        "bwrap: mount denied",
        evidence.get("mount") == [-1, 1],
        evidence.get("mount"))
    ledger.check(
        "bwrap: remount denied",
        evidence.get("remount") == [-1, 1],
        evidence.get("remount"))
    ledger.check(
        "bwrap: proc root has no home",
        evidence.get("proc1_home") is False,
        evidence.get("proc1_home"))
    if os.path.exists("/nix"):
        ledger.check(
            "bwrap: proc root keeps nix",
            evidence.get("proc1_nix") is True,
            evidence.get("proc1_nix"))
    else:
        ledger.skip("bwrap: proc root keeps nix", "/nix is absent")
    ledger.check(
        "bwrap: repository hidden",
        evidence.get("repo") is False,
        evidence.get("repo"))
    ledger.check(
        "bwrap: tdata hidden",
        evidence.get("tdata") is False,
        evidence.get("tdata"))
    ledger.check("bwrap: cwd is root", evidence.get("cwd") == "/", evidence)


def check_keyring(ledger, work):
    script = extract_block("kHostScript", "PY")
    join_code = extract_keyring_join(script or "")
    host_keys = read_proc_keys()
    host_serial = host_session_keyring()
    if join_code is None or BWRAP is None:
        ledger.skip(
            "keyring: sandbox session keyring",
            "join code or bwrap is unavailable")
        return
    _, plugins, _ = workspace(work, "keyring")
    payload = KEYRING_PROBE.replace("__JOIN__", join_code)
    result = subprocess.run(
        bwrap_command(plugins, payload),
        capture_output=True,
        text=True,
        timeout=60)
    ledger.check(
        "keyring: sandbox probe ran",
        result.returncode == 0,
        f"{result.returncode} {result.stderr.strip()[-400:]}")
    try:
        evidence = json.loads(result.stdout.strip().splitlines()[-1])
    except (ValueError, IndexError):
        evidence = {}
    before = evidence.get("before", -1)
    joined = evidence.get("joined", -1)
    after = evidence.get("after", -1)
    sandbox_keys = evidence.get("keys")
    ledger.check(
        "keyring: join succeeds in sandbox",
        isinstance(joined, int) and joined > 0,
        evidence)
    ledger.check(
        "keyring: sandbox session keyring is the joined keyring",
        after == joined and joined > 0,
        [before, joined, after])
    if sandbox_keys is None:
        ledger.skip(
            "keyring: sandbox /proc/keys lists the session keyring",
            "no /proc/keys in the sandbox")
    else:
        ledger.check(
            "keyring: sandbox /proc/keys lists the session keyring",
            joined > 0 and joined in session_keyring_serials(sandbox_keys),
            sandbox_keys)
    if host_serial is None:
        ledger.skip(
            "keyring: sandbox inherits the host session keyring before join",
            "the host has no session keyring")
        ledger.skip(
            "keyring: plugin session keyring differs from the host",
            "the host has no session keyring")
        ledger.skip(
            "keyring: host session keyring absent in the sandbox",
            "the host has no session keyring")
        return
    ledger.check(
        "keyring: sandbox inherits the host session keyring before join",
        before == host_serial,
        [before, host_serial])
    ledger.check(
        "keyring: plugin session keyring differs from the host",
        after == joined and after != host_serial,
        [after, joined, host_serial])
    if sandbox_keys is None:
        ledger.skip(
            "keyring: host session keyring absent in the sandbox",
            "no /proc/keys in the sandbox")
        return
    ledger.check(
        "keyring: host session keyring absent in the sandbox",
        host_serial not in key_serials(sandbox_keys),
        [host_serial, sorted(key_serials(sandbox_keys))])
    listed = session_keyring_serials(host_keys)
    if not listed:
        ledger.skip(
            "keyring: host /proc/keys lists its session keyring",
            "the host keyring is not listed")
    else:
        ledger.check(
            "keyring: host /proc/keys lists its session keyring",
            host_serial in listed,
            [sorted(listed), host_serial])


def check_keyring_plugin(ledger, cli, work):
    if read_proc_keys() is None:
        ledger.skip(
            "keyring plugin: host keyring read denied",
            "the host has no /proc/keys")
        return
    _, plugins, audit = workspace(work, "keyring-plugin")
    fixture = copy_fixture(plugins, "keyring-observe.plugin")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "keyring_observe", str(fixture)], work / "data")
    ledger.check("keyring plugin: load ok", data.get("ok") is True, data)
    records = audit_records(audit)
    ledger.check(
        "keyring plugin: host keyring read denied",
        has_event(records, "keyring_observe", "open"),
        records)
    ledger.check(
        "keyring plugin: plugin logged denial",
        has_log(records, "keyring_observe", "keyring read denied"),
        records)
    ledger.check(
        "keyring plugin: no keyring read",
        not has_any_log(records, "keyring read allowed"),
        records)


def check_valid_load(ledger, cli, work):
    _, plugins, audit = workspace(work, "valid")
    valid = copy_fixture(plugins, "valid.plugin")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "demo_runtime", str(valid)], work / "data")
    ledger.check("valid: load ok", data.get("ok") is True, data)
    ledger.check("valid: loaded", data.get("loaded") is True, data)
    pid = data.get("pid")
    ledger.check("valid: host pid", isinstance(pid, int) and pid > 0, pid)
    records = audit_records(audit)
    ledger.check(
        "valid: log in audit",
        has_log(records, "demo_runtime", "demo loaded"),
        records)


def check_sandbox_observe(ledger, cli, work):
    _, plugins, audit = workspace(work, "sandbox-observe")
    fixture = copy_fixture(plugins, "sandbox-observe.plugin")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "sandbox_observe", str(fixture)], work / "data",
        extra_env={"PYTHONPATH": "/exteraless-evil"})
    ledger.check("sandbox observe: load ok", data.get("ok") is True, data)
    records = audit_records(audit)
    ledger.check(
        "sandbox observe: uid, no home, scrubbed env, root cwd",
        has_log(
            records,
            "sandbox_observe",
            "sandbox uid=%d home=0 plugins=1 pp=0 cwd=/" % os.getuid()),
        records)


def check_unload(ledger, cli, work):
    _, plugins, audit = workspace(work, "unload")
    valid = copy_fixture(plugins, "valid.plugin")
    data = run_cli(
        cli, plugins, audit,
        [
            "multi",
            json.dumps([
                {"op": "enable", "id": "demo_runtime", "path": str(valid)},
                {"op": "disable", "id": "demo_runtime"},
            ]),
        ],
        work / "data")
    ledger.check("unload: two results", len(data) == 2, data)
    if len(data) == 2:
        ledger.check("unload: enable ok", data[0].get("ok") is True, data[0])
        ledger.check("unload: enable loaded", data[0].get("loaded") is True)
        ledger.check("unload: disable ok", data[1].get("ok") is True, data[1])
        ledger.check(
            "unload: disable not loaded",
            data[1].get("loaded") is False,
            data[1])
    records = audit_records(audit)
    ledger.check(
        "unload: on_unload logged",
        has_log(records, "demo_runtime", "demo unloaded"),
        records)


def check_deny_write(ledger, cli, work):
    base, plugins, audit = workspace(work, "deny-write")
    fixture = copy_fixture(plugins, "deny-write.plugin")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "deny_write", str(fixture)], work / "data")
    ledger.check("deny write: load ok", data.get("ok") is True, data)
    ledger.check("deny write: marker absent", not (base / "marker-denied").exists())
    records = audit_records(audit)
    ledger.check(
        "deny write: exact open denial",
        has_event(records, "deny_write", "open"),
        records)
    ledger.check(
        "deny write: plugin logged denial",
        has_log(records, "deny_write", "write denied"),
        records)


def check_escape_write(ledger, cli, work):
    base, plugins, audit = workspace(work, "escape-write")
    marker = base / "escape-write-marker"
    marker.write_text("control", encoding="utf-8")
    fixture = write_plugin(
        plugins,
        "escape-write.plugin",
        "from base_plugin import BasePlugin\n"
        "\n"
        "import os\n"
        "\n"
        "__id__ = \"escape_write\"\n"
        "__name__ = \"Escape Write\"\n"
        "__version__ = \"1.0.0\"\n"
        "__permissions__ = [\"files\"]\n"
        "\n"
        "\n"
        "class Escape(BasePlugin):\n"
        "    def on_load(self):\n"
        "        try:\n"
        f"            with open({str(marker)!r}, \"w\") as handle:\n"
        "                handle.write(\"bad\")\n"
        "        except (PermissionError, OSError):\n"
        "            self.log(\"escape write denied\")\n"
        "        else:\n"
        "            self.log(\"escape write allowed\")\n"
        "        self.log(\"escape path exists=%d\" % (\n"
        f"            1 if os.path.exists({str(marker)!r}) else 0))\n"
        "\n"
        "\n"
        "plugin = Escape()\n")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "escape_write", str(fixture)], work / "data")
    ledger.check("escape write: load ok", data.get("ok") is True, data)
    ledger.check(
        "escape write: host marker untouched",
        marker.read_text(encoding="utf-8") == "control",
        marker.read_text(encoding="utf-8"))
    records = audit_records(audit)
    ledger.check(
        "escape write: host path invisible",
        has_log(records, "escape_write", "escape path exists=0"),
        records)
    ledger.check(
        "escape write: exact open denial",
        has_event(records, "escape_write", "open"),
        records)
    ledger.check(
        "escape write: denied log",
        has_log(records, "escape_write", "escape write denied"),
        records)
    ledger.check(
        "escape write: never allowed",
        not has_any_log(records, "escape write allowed"),
        records)


def check_escape_xattr(ledger, cli, work):
    base, plugins, audit = workspace(work, "escape-xattr")
    marker = base / "xattr-marker"
    marker.write_text("host file", encoding="utf-8")
    fixture = write_plugin(
        plugins,
        "escape-xattr.plugin",
        "from base_plugin import BasePlugin\n"
        "\n"
        "import os\n"
        "\n"
        "__id__ = \"escape_xattr\"\n"
        "__name__ = \"Escape Xattr\"\n"
        "__version__ = \"1.0.0\"\n"
        "__permissions__ = [\"files\"]\n"
        "\n"
        "\n"
        "class Escape(BasePlugin):\n"
        "    def on_load(self):\n"
        "        try:\n"
        f"            os.setxattr({str(marker)!r}, b\"user.exteraless\", b\"bad\")\n"
        "        except (PermissionError, OSError):\n"
        "            self.log(\"xattr denied\")\n"
        "        else:\n"
        "            self.log(\"xattr allowed\")\n"
        "\n"
        "\n"
        "plugin = Escape()\n")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "escape_xattr", str(fixture)], work / "data")
    ledger.check("escape xattr: load ok", data.get("ok") is True, data)
    try:
        os.getxattr(marker, b"user.exteraless")
        touched = True
    except OSError:
        touched = False
    ledger.check("escape xattr: host xattr absent", not touched)
    records = audit_records(audit)
    ledger.check(
        "escape xattr: exact setxattr denial",
        has_event(records, "escape_xattr", "os.setxattr"),
        records)
    ledger.check(
        "escape xattr: denied log",
        has_log(records, "escape_xattr", "xattr denied"),
        records)
    ledger.check(
        "escape xattr: never allowed",
        not has_any_log(records, "xattr allowed"),
        records)



FORK_FIXTURE = '''from base_plugin import BasePlugin

import _posixsubprocess
import os
import sys

__id__ = "escape_fork"
__name__ = "Escape Fork"
__version__ = "1.0.0"
__permissions__ = ["native"]

MARKER = {marker}
SHELL = "/run/current-system/sw/bin/sh"
MOUNT_CODE = """
import ctypes
import os
import sys

os.makedirs("/tmp/m", exist_ok=True)
pid1 = open("/proc/1/cmdline", "rb").read()
inside = b"bubblewrap" in pid1 and not os.path.exists("/home")
libc = ctypes.CDLL(None, use_errno=True)
mounted = libc.mount(b"tmpfs", b"/tmp/m", b"tmpfs", 0, b"")
mount_errno = ctypes.get_errno()
remounted = libc.mount(None, b"/", None, 32, None)
remount_errno = ctypes.get_errno()
sys.exit(0 if (inside
               and mounted == -1 and mount_errno == 1
               and remounted == -1 and remount_errno == 1) else 9)
"""
NET_CODE = """
import socket
import sys

try:
    sock = socket.socket()
    sock.settimeout(2)
    sock.connect(("1.1.1.1", 53))
except OSError as error:
    sys.exit(0 if error.errno == 101 else 9)
sys.exit(9)
"""


def spawn(arguments):
    error_read, error_write = os.pipe()
    executable = os.fsencode(arguments[0])
    try:
        pid = _posixsubprocess.fork_exec(
            arguments, (executable,), True, (error_write,),
            None, None, -1, -1, -1, -1, -1, -1,
            error_read, error_write, True, False,
            -1, None, None, None, -1, None)
    finally:
        os.close(error_write)
    os.read(error_read, 4096)
    os.close(error_read)
    _, status = os.waitpid(pid, 0)
    return os.waitstatus_to_exitcode(status)


class EscapeFork(BasePlugin):
    def on_load(self):
        direct = spawn([
            SHELL.encode(), b"-c",
            ("printf bad > %s && exit 0 || exit 9" % MARKER).encode(),
        ])
        self.log("fork-write-rc=%d" % direct)
        through = spawn([
            SHELL.encode(), b"-c",
            ("printf bad > /proc/1/root%s && exit 0 || exit 9"
             % MARKER).encode(),
        ])
        self.log("fork-proc1-write-rc=%d" % through)
        home = spawn([
            SHELL.encode(), b"-c", b"test -e /proc/1/root/home",
        ])
        self.log("fork-proc1-home-rc=%d" % home)
        nix = spawn([
            SHELL.encode(), b"-c", b"test -e /proc/1/root/nix",
        ])
        self.log("fork-proc1-nix-rc=%d" % nix)
        mount = spawn([
            sys.executable.encode(), b"-I", b"-B", b"-c",
            MOUNT_CODE.encode(),
        ])
        self.log("fork-mount-rc=%d" % mount)
        net = spawn([
            sys.executable.encode(), b"-I", b"-B", b"-c",
            NET_CODE.encode(),
        ])
        self.log("fork-net-rc=%d" % net)


plugin = EscapeFork()
'''


def check_fork_escape(ledger, cli, work):
    base, plugins, audit = workspace(work, "escape-fork")
    marker = base / "escape-fork-marker"
    marker.write_text("control", encoding="utf-8")
    fixture = write_plugin(
        plugins,
        "escape-fork.plugin",
        FORK_FIXTURE.format(marker=repr(str(marker))))
    data = run_cli(
        cli, plugins, audit,
        ["enable", "escape_fork", str(fixture)], work / "data")
    ledger.check("fork escape: load ok", data.get("ok") is True, data)
    ledger.check(
        "fork escape: host marker untouched",
        marker.read_text(encoding="utf-8") == "control",
        marker.read_text(encoding="utf-8"))
    records = audit_records(audit)
    expectations = [
        ("direct write denied", "fork-write-rc=9"),
        ("proc root write denied", "fork-proc1-write-rc=9"),
        ("proc root has no home", "fork-proc1-home-rc=1"),
        ("proc root keeps nix", "fork-proc1-nix-rc=0"),
        ("mount and remount denied", "fork-mount-rc=0"),
        ("network unreachable", "fork-net-rc=0"),
    ]
    for name, detail in expectations:
        ledger.check(
            f"fork escape: {name}",
            has_log(records, "escape_fork", detail),
            records)
    ledger.check(
        "fork escape: no escape code",
        not has_any_log(records, "fork-write-rc=0")
        and not has_any_log(records, "fork-proc1-write-rc=0"),
        records)


def check_deny_socket(ledger, cli, work):
    _, plugins, audit = workspace(work, "deny-socket")
    fixture = copy_fixture(plugins, "deny-socket.plugin")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "deny_socket", str(fixture)], work / "data")
    ledger.check("deny socket: load ok", data.get("ok") is True, data)
    records = audit_records(audit)
    ledger.check(
        "deny socket: exact socket denial",
        has_event(records, "deny_socket", "socket.__new__"),
        records)
    ledger.check(
        "deny socket: plugin logged denial",
        has_log(records, "deny_socket", "socket denied"),
        records)


def check_deny_subprocess(ledger, cli, work):
    base, plugins, audit = workspace(work, "deny-subprocess")
    fixture = copy_fixture(plugins, "deny-subprocess.plugin")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "deny_subprocess", str(fixture)], work / "data")
    ledger.check("deny subprocess: load ok", data.get("ok") is True, data)
    ledger.check(
        "deny subprocess: marker absent",
        not (base / "marker-subprocess").exists())
    records = audit_records(audit)
    ledger.check(
        "deny subprocess: exact Popen denial",
        has_event(records, "deny_subprocess", "subprocess.Popen"),
        records)
    ledger.check(
        "deny subprocess: plugin logged denial",
        has_log(records, "deny_subprocess", "subprocess denied"),
        records)


def check_deny_ctypes(ledger, cli, work):
    base, plugins, audit = workspace(work, "deny-ctypes")
    fixture = copy_fixture(plugins, "deny-ctypes.plugin")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "deny_ctypes", str(fixture)], work / "data")
    ledger.check("deny ctypes: load ok", data.get("ok") is True, data)
    ledger.check(
        "deny ctypes: marker absent",
        not (base / "marker-ctypes").exists())
    records = audit_records(audit)
    ledger.check(
        "deny ctypes: exact dlopen denial",
        has_event(records, "deny_ctypes", "ctypes.dlopen"),
        records)
    ledger.check(
        "deny ctypes: plugin logged denial",
        has_log(records, "deny_ctypes", "ctypes denied"),
        records)
    ledger.check(
        "deny ctypes: never allowed",
        not has_any_log(records, "ctypes allowed"),
        records)


def check_deny_exec(ledger, cli, work):
    base, plugins, audit = workspace(work, "deny-exec")
    fixture = copy_fixture(plugins, "deny-exec.plugin")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "deny_exec", str(fixture)], work / "data")
    ledger.check("deny exec: load ok", data.get("ok") is True, data)
    ledger.check(
        "deny exec: marker absent",
        not (base / "marker-exec-denied").exists())
    records = audit_records(audit)
    ledger.check(
        "deny exec: exact compile denial",
        has_event(records, "deny_exec", "compile"),
        records)
    ledger.check(
        "deny exec: plugin logged denial",
        has_log(records, "deny_exec", "exec denied"),
        records)


def check_escape_exec(ledger, cli, work):
    base, plugins, audit = workspace(work, "escape-exec")
    marker = base / "escape-exec-marker"
    marker.write_text("control", encoding="utf-8")
    fixture = write_plugin(
        plugins,
        "escape-exec.plugin",
        "from base_plugin import BasePlugin\n"
        "\n"
        "__id__ = \"escape_exec\"\n"
        "__name__ = \"Escape Exec\"\n"
        "__version__ = \"1.0.0\"\n"
        "__permissions__ = [\"native\"]\n"
        "\n"
        "\n"
        "class Escape(BasePlugin):\n"
        "    def on_load(self):\n"
        "        try:\n"
        f"            exec(\"open({str(marker)!r}, 'w').write('bad')\", {{}})\n"
        "        except (PermissionError, OSError):\n"
        "            self.log(\"escape exec denied\")\n"
        "        else:\n"
        "            self.log(\"escape exec allowed\")\n"
        "\n"
        "\n"
        "plugin = Escape()\n")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "escape_exec", str(fixture)], work / "data")
    ledger.check("escape exec: load ok", data.get("ok") is True, data)
    ledger.check(
        "escape exec: host marker untouched",
        marker.read_text(encoding="utf-8") == "control",
        marker.read_text(encoding="utf-8"))
    records = audit_records(audit)
    ledger.check(
        "escape exec: exact compile denial",
        has_event(records, "escape_exec", "compile"),
        records)
    ledger.check(
        "escape exec: denied log",
        has_log(records, "escape_exec", "escape exec denied"),
        records)
    ledger.check(
        "escape exec: never allowed",
        not has_any_log(records, "escape exec allowed"),
        records)



CHILD_MARKERS_FIXTURE = '''from base_plugin import BasePlugin

import ctypes
import os
import subprocess

__id__ = "escape_children"
__name__ = "Escape Children"
__version__ = "1.0.0"
__permissions__ = ["native"]

SUB = {sub}
CTYPES = {ctypes_marker}
EXEC = {exec_marker}


class Escape(BasePlugin):
    def on_load(self):
        results = []
        try:
            subprocess.run([
                "/run/current-system/sw/bin/sh", "-c",
                "printf bad > %s" % SUB,
            ])
        except (PermissionError, OSError):
            results.append("sub=0")
        else:
            results.append("sub=1")
        try:
            libc = ctypes.CDLL(None)
            libc.open.restype = ctypes.c_int
            descriptor = libc.open(
                CTYPES.encode(), os.O_WRONLY | os.O_CREAT, 0o600)
            if descriptor >= 0:
                libc.write(descriptor, b"bad", 3)
                libc.close(descriptor)
                results.append("ct=1")
            else:
                results.append("ct=0")
        except (PermissionError, OSError):
            results.append("ct=0")
        try:
            exec("open(%r, 'w').write('bad')" % EXEC, {{}})
        except (PermissionError, OSError):
            results.append("exec=0")
        else:
            results.append("exec=1")
        self.log("child-markers " + " ".join(results))


plugin = Escape()
'''


def check_escape_child_markers(ledger, cli, work):
    base, plugins, audit = workspace(work, "escape-children")
    sub = base / "sub-marker"
    ctypes_marker = base / "ctypes-marker"
    exec_marker = base / "exec-marker"
    for path in (sub, ctypes_marker, exec_marker):
        path.write_text("control", encoding="utf-8")
    fixture = write_plugin(
        plugins,
        "escape-children.plugin",
        CHILD_MARKERS_FIXTURE.format(
            sub=repr(str(sub)),
            ctypes_marker=repr(str(ctypes_marker)),
            exec_marker=repr(str(exec_marker))))
    data = run_cli(
        cli, plugins, audit,
        ["enable", "escape_children", str(fixture)], work / "data")
    ledger.check(
        "escape children: load ok",
        data.get("ok") is True,
        data)
    for name, path in (
            ("subprocess", sub),
            ("ctypes", ctypes_marker),
            ("exec", exec_marker)):
        ledger.check(
            f"escape children: {name} marker untouched",
            path.read_text(encoding="utf-8") == "control",
            path.read_text(encoding="utf-8"))
    records = audit_records(audit)
    ledger.check(
        "escape children: all denied",
        has_log(records, "escape_children", "child-markers sub=0 ct=0 exec=0"),
        records)


def check_deny_import(ledger, cli, work):
    base, plugins, audit = workspace(work, "deny-import")
    fixture = copy_fixture(plugins, "deny-import.plugin")
    evilmod = base / "evilmod.py"
    evilmod.write_text(
        "import os\n"
        "open(os.path.join(os.path.dirname(__file__), 'evilmod-ran'), "
        "'w').write('bad')\n",
        encoding="utf-8")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "deny_import", str(fixture)], work / "data")
    ledger.check("deny import: load ok", data.get("ok") is True, data)
    ledger.check(
        "deny import: evilmod not executed",
        not (base / "evilmod-ran").exists())
    records = audit_records(audit)
    ledger.check(
        "deny import: plugin logged denial",
        has_log(records, "deny_import", "import denied"),
        records)


def find_foreign_module():
    profiles = Path("/etc/profiles")
    if not profiles.is_dir():
        return None, None
    stdlib_names = set(sys.stdlib_module_names)
    for site in sorted(profiles.glob("*/*/lib/python*/site-packages")):
        for candidate in sorted(site.glob("*/__init__.py")):
            name = candidate.parent.name
            if (name.isidentifier()
                    and not name.startswith("_")
                    and name not in stdlib_names):
                return site, name
    return None, None


def check_foreign_import(ledger, cli, work):
    site, module = find_foreign_module()
    if site is None:
        ledger.skip(
            "import outside roots: audit denial",
            "no python site-packages outside the stdlib was found")
        return
    _, plugins, audit = workspace(work, "foreign-import")
    fixture = write_plugin(
        plugins,
        "foreign-import.plugin",
        "from base_plugin import BasePlugin\n"
        "\n"
        "import sys\n"
        "\n"
        "__id__ = \"foreign_import\"\n"
        "__name__ = \"Foreign Import\"\n"
        "__version__ = \"1.0.0\"\n"
        "__permissions__ = [\"native\"]\n"
        "\n"
        "\n"
        "class Foreign(BasePlugin):\n"
        "    def on_load(self):\n"
        f"        sys.path.insert(0, {str(Path(os.path.realpath(site)))!r})\n"
        "        try:\n"
        f"            import {module}\n"
        "        except (PermissionError, ImportError, OSError):\n"
        "            self.log(\"foreign import denied\")\n"
        "        else:\n"
        "            self.log(\"foreign import allowed\")\n"
        "\n"
        "\n"
        "plugin = Foreign()\n")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "foreign_import", str(fixture)], work / "data")
    ledger.check("foreign import: load ok", data.get("ok") is True, data)
    records = audit_records(audit)
    ledger.check(
        "foreign import: exact import denial",
        has_event(records, "foreign_import", "import"),
        records)
    ledger.check(
        "foreign import: module not executed",
        has_log(records, "foreign_import", "foreign import denied")
        and not has_any_log(records, "foreign import allowed"),
        records)


def check_print_noise(ledger, cli, work):
    _, plugins, audit = workspace(work, "print-noise")
    fixture = copy_fixture(plugins, "print-noise.plugin")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "print_noise", str(fixture)], work / "data")
    ledger.check("print noise: load ok", data.get("ok") is True, data)
    ledger.check("print noise: loaded", data.get("loaded") is True, data)
    records = audit_records(audit)
    ledger.check(
        "print noise: log survived",
        has_log(records, "print_noise", "print survived"),
        records)
    ledger.check(
        "print noise: no noise in protocol",
        not has_any_log(records, "noise from plugin"),
        records)


def check_paths(ledger, cli, work):
    base, plugins, audit = workspace(work, "paths")
    valid = copy_fixture(plugins, "valid.plugin")
    outside = base / "outside"
    outside.mkdir()
    outside_fixture = copy_fixture(outside, "valid.plugin")

    data = run_cli(
        cli, plugins, audit, ["validate", str(outside_fixture)], work / "data")
    ledger.check("path outside: rejected", data.get("ok") is False, data)
    ledger.check(
        "path outside: outside in error",
        "outside" in data.get("error", ""),
        data.get("error"))

    data = run_cli(
        cli, plugins, audit,
        ["validate", str(plugins / ".." / "outside" / "valid.plugin")],
        work / "data")
    ledger.check("path traversal: rejected", data.get("ok") is False, data)
    ledger.check(
        "path traversal: outside in error",
        "outside" in data.get("error", ""),
        data.get("error"))

    link = plugins / "linked.plugin"
    try:
        os.symlink(valid, link)
    except OSError as error:
        ledger.skip("symlink: rejected", str(error))
    else:
        data = run_cli(
            cli, plugins, audit, ["validate", str(link)], work / "data")
        ledger.check("symlink: rejected", data.get("ok") is False, data)
        ledger.check(
            "symlink: symlink in error",
            "symlink" in data.get("error", ""),
            data.get("error"))

    wrong = plugins / "wrong.txt"
    wrong.write_text("__id__ = \"wrong\"\n", encoding="utf-8")
    data = run_cli(
        cli, plugins, audit, ["validate", str(wrong)], work / "data")
    ledger.check("wrong extension: rejected", data.get("ok") is False, data)
    ledger.check(
        "wrong extension: unsupported format in error",
        "Unsupported plugin format" in data.get("error", ""),
        data.get("error"))

    nested = plugins / "nested"
    nested.mkdir()
    nested_fixture = copy_fixture(nested, "valid.plugin")
    data = run_cli(
        cli, plugins, audit, ["validate", str(nested_fixture)], work / "data")
    ledger.check("nested path: rejected", data.get("ok") is False, data)
    ledger.check(
        "nested path: directly inside in error",
        "directly inside" in data.get("error", ""),
        data.get("error"))

    data = run_cli(
        cli, plugins, audit,
        ["validate", str(plugins / "missing.plugin")], work / "data")
    ledger.check("missing file: rejected", data.get("ok") is False, data)
    ledger.check(
        "missing file: missing in error",
        "missing" in data.get("error", ""),
        data.get("error"))

    symlink_root = base / "plugins-link"
    try:
        os.symlink(plugins, symlink_root)
    except OSError as error:
        ledger.skip("canonical plugins root", str(error))
    else:
        data = run_cli(
            cli, symlink_root, audit,
            ["enable", "demo_runtime", str(valid)], work / "data")
        ledger.check(
            "canonical plugins root: load ok",
            data.get("ok") is True,
            data)
        ledger.check(
            "canonical plugins root: loaded",
            data.get("loaded") is True,
            data)


def check_bwrap_unavailable(ledger, cli, work):
    _, plugins, audit = workspace(work, "bwrap-unavailable")
    fixture = copy_fixture(plugins, "deny-write.plugin")
    tools = tool_directory(work, "bwrap-absent-bin", ["python3"])
    ledger.check("bwrap unavailable: tools built", tools is not None)
    if tools is None:
        return
    data = run_cli(
        cli, plugins, audit,
        ["enable", "deny_write", str(fixture)], work / "data",
        extra_env={"PATH": str(tools), "EXTERALESS_BWRAP": ""})
    ledger.check(
        "bwrap unavailable: enable failed",
        data.get("ok") is False,
        data)
    ledger.check(
        "bwrap unavailable: exact error",
        data.get("error") == "Plugin sandbox is unavailable.",
        data.get("error"))
    ledger.check(
        "bwrap unavailable: no host pid",
        data.get("pid") == 0,
        data.get("pid"))
    ledger.check(
        "bwrap unavailable: plugin not executed",
        not (work / "bwrap-unavailable" / "marker-denied").exists()
        and not audit.exists(),
        audit)
    ledger.check(
        "bwrap unavailable: no process left",
        host_pids(plugins) == [],
        host_pids(plugins))


def check_bwrap_override(ledger, cli, work):
    _, plugins, audit = workspace(work, "bwrap-override")
    fixture = copy_fixture(plugins, "valid.plugin")
    tools = tool_directory(work, "bwrap-override-bin", ["python3"])
    ledger.check("bwrap override: tools built", tools is not None and BWRAP)
    if tools is None or BWRAP is None:
        return
    data = run_cli(
        cli, plugins, audit,
        ["enable", "demo_runtime", str(fixture)], work / "data",
        extra_env={
            "PATH": str(tools),
            "EXTERALESS_BWRAP": BWRAP,
        })
    ledger.check(
        "bwrap override: load ok",
        data.get("ok") is True and data.get("loaded") is True,
        data)

    invalid = run_cli(
        cli, plugins, audit,
        ["enable", "demo_runtime", str(fixture)], work / "data",
        extra_env={
            "PATH": str(tools),
            "EXTERALESS_BWRAP": "/nonexistent/bwrap",
        })
    ledger.check(
        "bwrap override: invalid override fails closed",
        invalid.get("ok") is False
        and invalid.get("error") == "Plugin sandbox is unavailable.",
        invalid)


def check_bwrap_failure(ledger, cli, work):
    base, plugins, audit = workspace(work, "bwrap-failure")
    fixture = copy_fixture(plugins, "deny-write.plugin")
    marker = base / "fake-bwrap-ran"
    tools = tool_directory(
        work,
        "bwrap-broken-bin",
        ["python3"],
        extra_scripts={
            "bwrap": "#!/bin/sh\n"
                     f"echo ran >> {marker}\n"
                     "exit 1\n",
        })
    ledger.check("bwrap failure: tools built", tools is not None)
    if tools is None:
        return
    data = run_cli(
        cli, plugins, audit,
        ["enable", "deny_write", str(fixture)], work / "data",
        extra_env={"PATH": str(tools), "EXTERALESS_BWRAP": ""})
    ledger.check(
        "bwrap failure: enable failed",
        data.get("ok") is False,
        data)
    ledger.check(
        "bwrap failure: start error",
        data.get("error", "").startswith("Plugin host failed to start."),
        data.get("error"))
    ledger.check(
        "bwrap failure: fake bwrap executed",
        marker.is_file(),
        str(marker))
    ledger.check(
        "bwrap failure: no host started",
        data.get("pid") == 0 and not audit.exists(),
        data)


def check_protocol(ledger, cli, work):
    cases = [
        ("protocol-wrong-seq.plugin", "protocol_wrong_seq", "wrong seq"),
        ("protocol-missing-seq.plugin", "protocol_missing_seq", "missing seq"),
        ("protocol-ok-false.plugin", "protocol_ok_false", "false ok"),
    ]
    for fixture_name, plugin_id, label in cases:
        _, plugins, audit = workspace(work, f"protocol-{label.replace(' ', '-')}")
        fixture = copy_fixture(plugins, fixture_name)
        data = run_cli(
            cli, plugins, audit,
            ["enable", plugin_id, str(fixture)], work / "data")
        ledger.check(
            f"protocol {label}: rejected",
            data.get("ok") is False,
            data)
        ledger.check(
            f"protocol {label}: violation error",
            data.get("error") == "Plugin host protocol violation.",
            data.get("error"))
        ledger.check(
            f"protocol {label}: host killed",
            data.get("pid") == 0
            and host_pids(plugins) == [],
            [data.get("pid"), host_pids(plugins)])

    _, plugins, audit = workspace(work, "protocol-restart")
    broken = copy_fixture(plugins, "protocol-wrong-seq.plugin")
    valid = copy_fixture(plugins, "valid.plugin")
    data = run_cli(
        cli, plugins, audit,
        [
            "multi",
            json.dumps([
                {
                    "op": "enable",
                    "id": "protocol_wrong_seq",
                    "path": str(broken),
                },
                {"op": "enable", "id": "demo_runtime", "path": str(valid)},
            ]),
        ],
        work / "data")
    ledger.check("protocol restart: two results", len(data) == 2, data)
    if len(data) == 2:
        ledger.check(
            "protocol restart: first rejected",
            data[0].get("ok") is False,
            data[0])
        ledger.check(
            "protocol restart: second loaded",
            data[1].get("ok") is True and data[1].get("loaded") is True,
            data[1])

    _, plugins, audit = workspace(work, "protocol-unsolicited")
    fixture = copy_fixture(plugins, "protocol-unsolicited-log.plugin")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "protocol_unsolicited", str(fixture)], work / "data")
    ledger.check(
        "protocol unsolicited: load ok",
        data.get("ok") is True and data.get("loaded") is True,
        data)
    records = audit_records(audit)
    ledger.check(
        "protocol unsolicited: accepted known log",
        has_log(records, "protocol_unsolicited", "after unsolicited"),
        records)
    ledger.check(
        "protocol unsolicited: dropped stale log",
        not has_any_log(records, "unsolicited log"),
        records)


def check_fd_hygiene(ledger, work):
    script = extract_block("kHostScript", "PY")
    python = require_tool("python3")
    if script is None or python is None:
        ledger.check("fd hygiene: prerequisites", False)
        return
    marker = work / "fd-marker"
    descriptor = os.open(marker, os.O_RDWR | os.O_CREAT, 0o600)
    try:
        process = subprocess.Popen(
            [python, "-I", "-B", "-c", script, "/plugins"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            pass_fds=(descriptor,),
        )
    finally:
        os.close(descriptor)
    try:
        ready = process.stdout.readline(4096)
        try:
            message = json.loads(ready)
        except ValueError:
            message = {}
        ledger.check(
            "fd hygiene: host ready",
            message.get("type") == "ready",
            ready)
        open_fds = sorted(
            int(name)
            for name in os.listdir(f"/proc/{process.pid}/fd"))
        targets = []
        for descriptor_name in os.listdir(f"/proc/{process.pid}/fd"):
            try:
                targets.append(os.readlink(
                    f"/proc/{process.pid}/fd/{descriptor_name}"))
            except OSError:
                continue
        ledger.check(
            "fd hygiene: inherited fd closed",
            not any(str(marker) in target for target in targets),
            targets)
        ledger.check(
            "fd hygiene: only standard and protocol fds",
            open_fds == [0, 1, 2, 3, 4],
            open_fds)
        process.stdin.write(b'{"cmd":"shutdown"}\n')
        process.stdin.flush()
        process.wait(timeout=10)
        ledger.check(
            "fd hygiene: clean shutdown",
            process.returncode == 0,
            process.returncode)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)


def startup_sources():
    paths = []
    for entry in STARTUP_SOURCES:
        if entry.is_dir():
            paths.extend(sorted(
                path for path in entry.rglob("*")
                if path.suffix in (".cpp", ".h", ".mm")))
        else:
            paths.append(entry)
    return paths


def check_no_startup_host(ledger):
    missing = [
        str(path.relative_to(ROOT)) for path in STARTUP_SOURCES
        if not path.exists()
    ]
    ledger.check("startup: sources found", not missing, missing)
    offenders = []
    for path in startup_sources():
        text = path.read_text(encoding="utf-8", errors="ignore")
        lines = text.splitlines()
        for lineno, line in enumerate(lines, 1):
            for marker in STARTUP_HOST_MARKERS:
                if marker not in line:
                    continue
                window = "\n".join(lines[lineno - 1:lineno + 1])
                if "dispatchIntent" in window:
                    continue
                offenders.append(
                    f"{path.relative_to(ROOT)}:{lineno}: {marker}")
    ledger.check(
        "startup: no Runtime::Start or Runtime::Instance in app startup",
        not offenders,
        offenders)
    application = (
        ROOT / "Telegram" / "SourceFiles" / "core" / "application.cpp")
    text = application.read_text(encoding="utf-8", errors="ignore")
    ledger.check(
        "startup: application stops the runtime on exit",
        "Runtime::Stop" in text,
        str(application.relative_to(ROOT)))


def check_timeout(ledger, cli, work):
    _, plugins, audit = workspace(work, "timeout")
    fixture = copy_fixture(plugins, "infinite-loop.plugin")
    started = time.monotonic()
    data = run_cli(
        cli, plugins, audit,
        ["enable", "infinite_loop", str(fixture)], work / "data",
        timeout=60)
    elapsed = time.monotonic() - started
    ledger.check("timeout: load failed", data.get("ok") is False, data)
    ledger.check(
        "timeout: timed out error",
        "timed out" in data.get("error", "").lower(),
        data.get("error"))
    ledger.check(
        "timeout: elapsed about 10s",
        8 <= elapsed <= 20,
        f"{elapsed:.1f}s")
    ledger.check(
        "timeout: no host left",
        host_pids(plugins) == [],
        host_pids(plugins))

    valid = copy_fixture(plugins, "valid.plugin")
    restarted = run_cli(
        cli, plugins, audit,
        ["enable", "demo_runtime", str(valid)], work / "data")
    pid = restarted.get("pid")
    ledger.check("timeout: restart ok", restarted.get("ok") is True, restarted)
    ledger.check(
        "timeout: restart new pid",
        isinstance(pid, int) and pid > 0 and pid != data.get("pid"),
        pid)


def check_single_host(ledger, cli, work):
    _, plugins, audit = workspace(work, "single-host")
    first = copy_fixture(plugins, "valid.plugin")
    second = copy_fixture(plugins, "valid-second.plugin")
    data = run_cli(
        cli, plugins, audit,
        [
            "multi",
            json.dumps([
                {"op": "enable", "id": "demo_runtime", "path": str(first)},
                {"op": "enable", "id": "demo_second", "path": str(second)},
            ]),
        ],
        work / "data")
    ledger.check("single host: two results", len(data) == 2, data)
    if len(data) == 2:
        ledger.check("single host: first loaded", data[0].get("loaded") is True)
        ledger.check("single host: second loaded", data[1].get("loaded") is True)
        ledger.check(
            "single host: same pid",
            data[0].get("pid") == data[1].get("pid")
            and isinstance(data[0].get("pid"), int)
            and data[0].get("pid") > 0,
            [data[0].get("pid"), data[1].get("pid")])
    records = audit_records(audit)
    ledger.check(
        "single host: both plugins logged",
        has_log(records, "demo_runtime", "demo loaded")
        and has_log(records, "demo_second", "second loaded"),
        records)


def check_stop(ledger, cli, work):
    _, plugins, audit = workspace(work, "stop")
    valid = copy_fixture(plugins, "valid.plugin")
    data = run_cli(
        cli, plugins, audit,
        [
            "multi",
            json.dumps([
                {"op": "enable", "id": "demo_runtime", "path": str(valid)},
                {"op": "stop"},
            ]),
        ],
        work / "data")
    ledger.check("stop: two results", len(data) == 2, data)
    pid = data[0].get("pid") if data else 0
    if len(data) == 2:
        ledger.check("stop: enable pid", isinstance(pid, int) and pid > 0, pid)
        ledger.check("stop: ok", data[1].get("ok") is True, data[1])
        ledger.check("stop: pid zero", data[1].get("pid") == 0, data[1])
        ledger.check(
            "stop: not loaded",
            data[1].get("loaded") is False,
            data[1])
    if isinstance(pid, int) and pid > 0:
        ledger.check(
            f"stop: process {pid} gone",
            not Path(f"/proc/{pid}").exists())


def check_state(ledger, cli, work):
    _, plugins, audit = workspace(work, "state")
    data_home = work / "state-data"
    data = run_cli(
        cli, plugins, audit, ["state-get", "unknown_id"], data_home)
    ledger.check("state default: ok", data.get("ok") is True, data)
    ledger.check("state default: false", data.get("enabled") is False, data)

    data = run_cli(
        cli, plugins, audit, ["state-set", "demo_runtime", "true"], data_home)
    ledger.check("state set: ok", data.get("ok") is True, data)
    ledger.check("state set: true", data.get("enabled") is True, data)
    data = run_cli(
        cli, plugins, audit, ["state-get", "demo_runtime"], data_home)
    ledger.check("state persistence: true", data.get("enabled") is True, data)

    config_path = (
        data_home / "ExteralessDesktop" / "tdata" / "exteraless.json")
    ledger.check("state persistence: config file", config_path.is_file())
    if config_path.is_file():
        stored = json.loads(config_path.read_text(encoding="utf-8"))
        states = stored.get("exteraconfig", {}).get("OEPluginStates", {})
        ledger.check(
            "state persistence: stored state",
            states == {"demo_runtime": True},
            states)


def check_config_whitelist(ledger, cli, work):
    _, plugins, audit = workspace(work, "whitelist")
    data_home = work / "whitelist-data"
    import_path = work / "import-state.json"
    import_path.write_text(
        json.dumps({
            "exteraconfig": {
                "OEPluginStates": {
                    "demo_runtime": True,
                    "demo_second": False,
                    "demo_off": "true",
                    "demo_num": 1,
                    "1bad": True,
                    "bad id": True,
                    "-leading": True,
                    "toolongtoolongtoolongtoolongtoolong": True,
                },
                "OEBogus": True,
            },
        }),
        encoding="utf-8")
    data = run_cli(
        cli, plugins, audit, ["import-state", str(import_path)], data_home)
    ledger.check("import state: ok", data.get("ok") is True, data)
    ledger.check(
        "import state: unknown key reported",
        data.get("unknown") == ["OEBogus"],
        data.get("unknown"))
    for plugin_id in (
            "demo_second", "demo_off", "demo_num", "1bad", "-leading",
            "toolongtoolongtoolongtoolongtoolong"):
        state = run_cli(
            cli, plugins, audit, ["state-get", plugin_id], data_home)
        ledger.check(
            f"import state: {plugin_id} false",
            state.get("enabled") is False,
            state)
    state = run_cli(
        cli, plugins, audit, ["state-get", "demo_runtime"], data_home)
    ledger.check("import state: valid true kept", state.get("enabled") is True)

    config_path = (
        data_home / "ExteralessDesktop" / "tdata" / "exteraless.json")
    stored = json.loads(config_path.read_text(encoding="utf-8"))
    states = stored.get("exteraconfig", {}).get("OEPluginStates", {})
    ledger.check(
        "import state: only clean state stored",
        states == {"demo_runtime": True},
        states)


def check_plugin_settings(ledger, cli, work):
    _, plugins, audit = workspace(work, "plugin-settings")
    fixture = copy_fixture(plugins, "valid-settings.plugin")
    data_home = work / "plugin-settings-data"
    expected = {"enabled": True, "mode": 2, "prefix": "demo"}

    data = run_cli(
        cli, plugins, audit, ["settings-get", "demo_settings"], data_home)
    ledger.check("settings default: ok", data.get("ok") is True, data)
    ledger.check(
        "settings default: empty",
        data.get("settings") == {},
        data.get("settings"))

    data = run_cli(
        cli, plugins, audit,
        ["settings-set", "demo_settings", "enabled", "true"], data_home)
    ledger.check("settings write: ok", data.get("ok") is True, data)
    ledger.check(
        "settings write: stored",
        data.get("settings") == {"enabled": True},
        data.get("settings"))
    run_cli(
        cli, plugins, audit,
        ["settings-set", "demo_settings", "mode", "2"], data_home)
    run_cli(
        cli, plugins, audit,
        ["settings-set", "demo_settings", "prefix", '"demo"'], data_home)

    data = run_cli(
        cli, plugins, audit, ["settings-get", "demo_settings"], data_home)
    ledger.check(
        "settings read: own keys",
        data.get("settings") == expected,
        data.get("settings"))
    ledger.check(
        "settings read: values only",
        "default" not in data.get("settings", {}),
        data.get("settings"))

    data = run_cli(
        cli, plugins, audit,
        [
            "multi",
            json.dumps([
                {"op": "enable", "id": "demo_settings", "path": str(fixture)},
                {"op": "disable", "id": "demo_settings"},
                {"op": "enable", "id": "demo_settings", "path": str(fixture)},
                {"op": "disable", "id": "demo_settings"},
            ]),
        ],
        data_home)
    ledger.check(
        "settings reload: plugin reloaded",
        [entry.get("ok") for entry in data] == [True, True, True, True],
        data)
    data = run_cli(
        cli, plugins, audit, ["settings-get", "demo_settings"], data_home)
    ledger.check(
        "settings reload: survived",
        data.get("settings") == expected,
        data.get("settings"))

    run_cli(
        cli, plugins, audit,
        ["settings-set", "demo_second", "prefix", '"other"'], data_home)
    run_cli(
        cli, plugins, audit,
        ["settings-set", "demo_second", "only_second", "true"], data_home)
    data = run_cli(
        cli, plugins, audit, ["settings-get", "demo_settings"], data_home)
    ledger.check(
        "settings foreign: own keys untouched",
        data.get("settings") == expected,
        data.get("settings"))
    second = run_cli(
        cli, plugins, audit, ["settings-get", "demo_second"], data_home)
    ledger.check(
        "settings foreign: second keys kept",
        second.get("settings") == {"prefix": "other", "only_second": True},
        second.get("settings"))

    data = run_cli(
        cli, plugins, audit,
        ["settings-set", "demo_settings", "mode", "{bad"], data_home)
    ledger.check(
        "settings invalid json: rejected",
        data.get("ok") is False
        and data.get("error") == "Invalid JSON value.",
        data)
    data = run_cli(
        cli, plugins, audit,
        ["settings-set", "1bad", "mode", "1"], data_home)
    ledger.check(
        "settings invalid id: rejected",
        data.get("ok") is False,
        data)
    ledger.check(
        "settings invalid id: nothing stored",
        run_cli(
            cli, plugins, audit,
            ["settings-get", "1bad"], data_home).get("settings") == {},
        None)
    data = run_cli(
        cli, plugins, audit,
        ["settings-set", "demo_settings", "k" * 65, "1"], data_home)
    ledger.check(
        "settings invalid key: rejected",
        data.get("ok") is False,
        data)
    data = run_cli(
        cli, plugins, audit, ["settings-get", "demo_settings"], data_home)
    ledger.check(
        "settings invalid: state unchanged",
        data.get("settings") == expected,
        data.get("settings"))


GARBAGE_MENU_PLUGIN = r'''
from base_plugin import BasePlugin, MenuItemData

__id__ = "menu_garbage"
__name__ = "Menu Garbage"
__version__ = "1.0.0"
__permissions__ = ["ui"]


class Garbage(BasePlugin):
    def on_load(self):
        self.add_menu_item(MenuItemData(
            item_id="bad_text", text="Bell\x07text"))
        self.add_menu_item(MenuItemData(
            item_id="long_text", text="x" * 129))
        self.add_menu_item(MenuItemData(
            item_id="1bad", text="Bad id"))
        self.add_menu_item(MenuItemData(
            item_id="bad_ctx", text="Bad context"), context="bogus")
        self.add_menu_item(MenuItemData(
            item_id="kept",
            text="  Kept item  ",
            subtext="\x07bell",
            priority=True))


plugin = Garbage()
'''


def check_plugin_menu(ledger, cli, work):
    _, plugins, audit = workspace(work, "plugin-menu")
    fixture = copy_fixture(plugins, "valid-menu.plugin")
    data_home = work / "plugin-menu-data"
    expected = [
        {
            "plugin": "demo_menu",
            "item": "second",
            "text": "Second item",
            "subtext": "Added later",
            "priority": 10,
            "context": "main",
        },
        {
            "plugin": "demo_menu",
            "item": "first",
            "text": "First item",
            "priority": 5,
            "context": "main",
        },
    ]

    data = run_cli(
        cli, plugins, audit, ["menu-list", "demo_menu"], data_home)
    ledger.check(
        "menu list: empty before enable",
        data.get("items") == [],
        data.get("items"))
    data = run_cli(
        cli, plugins, audit,
        ["menu-click", "demo_menu", "first"], data_home)
    ledger.check(
        "menu click: not loaded rejected",
        data.get("ok") is False
        and data.get("error") == "Plugin is not loaded.",
        data)

    data = run_cli(
        cli, plugins, audit,
        [
            "multi",
            json.dumps([
                {"op": "enable", "id": "demo_menu", "path": str(fixture)},
                {"op": "menu-list", "id": "demo_menu"},
                {"op": "menu-click", "id": "demo_menu", "item": "second"},
                {"op": "menu-click", "id": "demo_menu", "item": "first"},
                {
                    "op": "menu-click",
                    "id": "demo_menu",
                    "item": "first",
                    "context": "message",
                },
                {
                    "op": "menu-click",
                    "id": "demo_menu",
                    "item": "missing",
                },
                {"op": "disable", "id": "demo_menu"},
                {"op": "menu-click", "id": "demo_menu", "item": "first"},
                {"op": "menu-list", "id": "demo_menu"},
            ]),
        ],
        data_home)
    ledger.check("menu: nine results", len(data) == 9, data)
    if len(data) != 9:
        return
    ledger.check(
        "menu: plugin loaded",
        data[0].get("ok") is True and data[0].get("loaded") is True,
        data[0])
    ledger.check(
        "menu list: priority order after enable",
        data[1].get("items") == expected,
        data[1].get("items"))
    ledger.check(
        "menu click: top item ok",
        data[2].get("ok") is True and data[2].get("error") == "",
        data[2])
    ledger.check(
        "menu click: bottom item ok",
        data[3].get("ok") is True and data[3].get("error") == "",
        data[3])
    ledger.check(
        "menu click: foreign context rejected",
        data[4].get("ok") is False
        and data[4].get("error") == "Unknown menu item.",
        data[4])
    ledger.check(
        "menu click: unknown item rejected",
        data[5].get("ok") is False
        and data[5].get("error") == "Unknown menu item.",
        data[5])
    ledger.check(
        "menu: plugin disabled",
        data[6].get("ok") is True and data[6].get("loaded") is False,
        data[6])
    ledger.check(
        "menu click: after unload rejected",
        data[7].get("ok") is False
        and data[7].get("error") == "Plugin is not loaded.",
        data[7])
    ledger.check(
        "menu list: empty after unload",
        data[8].get("items") == [],
        data[8].get("items"))
    records = audit_records(audit)
    ledger.check(
        "menu click: audit second",
        has_log(records, "demo_menu", "menu clicked second:main"),
        records)
    ledger.check(
        "menu click: audit first",
        has_log(records, "demo_menu", "menu clicked first:main"),
        records)

    garbage = write_plugin(plugins, "menu-garbage.plugin", GARBAGE_MENU_PLUGIN)
    data = run_cli(
        cli, plugins, audit,
        [
            "multi",
            json.dumps([
                {"op": "enable", "id": "menu_garbage", "path": str(garbage)},
                {"op": "menu-list", "id": "menu_garbage"},
                {"op": "menu-click", "id": "menu_garbage", "item": "bad_text"},
                {"op": "menu-click", "id": "menu_garbage", "item": "kept"},
            ]),
        ],
        data_home)
    ledger.check("menu garbage: four results", len(data) == 4, data)
    if len(data) == 4:
        ledger.check(
            "menu garbage: loaded",
            data[0].get("ok") is True,
            data[0])
        ledger.check(
            "menu garbage: only clean item kept",
            data[1].get("items") == [
                {
                    "plugin": "menu_garbage",
                    "item": "kept",
                    "text": "Kept item",
                    "priority": 0,
                    "context": "main",
                },
            ],
            data[1].get("items"))
        ledger.check(
            "menu garbage: dropped item not clickable",
            data[2].get("ok") is False
            and data[2].get("error") == "Unknown menu item.",
            data[2])
        ledger.check(
            "menu garbage: clean item clickable",
            data[3].get("ok") is True,
            data[3])

    crash = copy_fixture(plugins, "crash.plugin")
    data = run_cli(
        cli, plugins, audit,
        [
            "multi",
            json.dumps([
                {"op": "enable", "id": "demo_menu", "path": str(fixture)},
                {"op": "enable", "id": "crash_plugin", "path": str(crash)},
                {"op": "menu-list", "id": "demo_menu"},
                {"op": "menu-click", "id": "demo_menu", "item": "first"},
            ]),
        ],
        work / "plugin-menu-crash-data")
    ledger.check("menu crash: four results", len(data) == 4, data)
    if len(data) == 4:
        ledger.check(
            "menu crash: host died on load",
            data[1].get("ok") is False and data[1].get("pid") == 0,
            data[1])
        ledger.check(
            "menu crash: list empty after crash",
            data[2].get("items") == [],
            data[2].get("items"))
        ledger.check(
            "menu crash: click rejected after crash",
            data[3].get("ok") is False,
            data[3])
    ledger.check(
        "menu crash: no host left",
        host_pids(plugins) == [],
        host_pids(plugins))


GARBAGE_INTENTS_PLUGIN = r'''
from base_plugin import BasePlugin

__id__ = "intent_garbage"
__name__ = "Intent Garbage"
__version__ = "1.0.0"
__permissions__ = ["intents"]


class Garbage(BasePlugin):
    def on_load(self):
        self.add_intent_handler("1bad", ["tg"])
        self.add_intent_handler("bad_phase", ["tg"], phase="bogus")
        self.add_intent_handler("no_schemes", [])
        self.add_intent_handler("bad_scheme", ["1bad scheme"])
        self.add_intent_handler(
            "too_many", ["a", "b", "c", "d", "e", "f", "g", "h", "i"])
        self.add_intent_handler("kept", ["xkept"])

    def on_intent(self, item_id, url, context):
        self.log("intent %s %s" % (item_id, url))
        return {"cancel": True}


plugin = Garbage()
'''


def check_plugin_intents(ledger, cli, work):
    _, plugins, audit = workspace(work, "plugin-intents")
    fixture = copy_fixture(plugins, "valid-intents.plugin")
    data_home = work / "plugin-intents-data"
    expected_snapshot = [
        {
            "plugin": "demo_intents",
            "item": "cancel_test",
            "phase": "before",
            "schemes": ["tg"],
            "priority": 0,
        },
        {
            "plugin": "demo_intents",
            "item": "after_log",
            "phase": "after",
            "schemes": ["tg"],
            "priority": 0,
        },
    ]

    data = run_cli(
        cli, plugins, audit,
        [
            "multi",
            json.dumps([
                {"op": "enable", "id": "demo_intents", "path": str(fixture)},
                {"op": "intent-list", "id": "demo_intents"},
                {"op": "intent-test", "url": "tg://resolve?test=intent"},
                {"op": "intent-test", "url": "https://example.com/"},
                {"op": "intent-test",
                 "url": "tg://resolve?test=intent",
                 "fromIntent": True},
                {"op": "disable", "id": "demo_intents"},
                {"op": "intent-test", "url": "tg://resolve?test=intent"},
                {"op": "intent-list", "id": "demo_intents"},
            ]),
        ],
        data_home)
    ledger.check("intents: eight results", len(data) == 8, data)
    if len(data) != 8:
        return
    ledger.check(
        "intents: registered after enable",
        data[0].get("ok") is True and data[0].get("loaded") is True,
        data[0])
    ledger.check(
        "intents: snapshot after enable",
        data[1].get("items") == expected_snapshot,
        data[1].get("items"))
    ledger.check(
        "intents: before handler cancels",
        data[2].get("handled") is True,
        data[2])
    ledger.check(
        "intents: foreign scheme not handled",
        data[3].get("handled") is False,
        data[3])
    ledger.check(
        "intents: after pass does not cancel",
        data[4].get("handled") is False,
        data[4])
    ledger.check(
        "intents: plugin disabled",
        data[5].get("ok") is True and data[5].get("loaded") is False,
        data[5])
    ledger.check(
        "intents: unload clears handlers",
        data[6].get("handled") is False,
        data[6])
    ledger.check(
        "intents: snapshot empty after unload",
        data[7].get("items") == [],
        data[7].get("items"))
    records = audit_records(audit)
    ledger.check(
        "intents: before logged",
        has_log(
            records,
            "demo_intents",
            "intent cancel_test tg://resolve?test=intent"),
        records)
    ledger.check(
        "intents: after logged",
        has_log(
            records,
            "demo_intents",
            "intent after_log tg://resolve?test=intent"),
        records)
    before_logs = [
        record for record in records
        if record.get("plugin") == "demo_intents"
        and record.get("detail")
        == "intent cancel_test tg://resolve?test=intent"
    ]
    ledger.check(
        "intents: unload stops logging",
        len(before_logs) == 1,
        before_logs)

    garbage = write_plugin(
        plugins, "intent-garbage.plugin", GARBAGE_INTENTS_PLUGIN)
    data = run_cli(
        cli, plugins, audit,
        [
            "multi",
            json.dumps([
                {"op": "enable", "id": "intent_garbage", "path": str(garbage)},
                {"op": "intent-list", "id": "intent_garbage"},
                {"op": "intent-test", "url": "tg://drop"},
                {"op": "intent-test", "url": "xkept://drop"},
            ]),
        ],
        work / "plugin-intents-garbage-data")
    ledger.check("intents garbage: four results", len(data) == 4, data)
    if len(data) == 4:
        ledger.check(
            "intents garbage: loaded",
            data[0].get("ok") is True,
            data[0])
        ledger.check(
            "intents garbage: only clean handler kept",
            data[1].get("items") == [
                {
                    "plugin": "intent_garbage",
                    "item": "kept",
                    "phase": "before",
                    "schemes": ["xkept"],
                    "priority": 0,
                },
            ],
            data[1].get("items"))
        ledger.check(
            "intents garbage: dropped handler ignored",
            data[2].get("handled") is False,
            data[2])
        ledger.check(
            "intents garbage: clean handler kept",
            data[3].get("handled") is True,
            data[3])
    garbage_records = audit_records(audit)
    ledger.check(
        "intents garbage: clean handler ran",
        has_log(garbage_records, "intent_garbage", "intent kept xkept://drop"),
        garbage_records)
    dropped = [
        record for record in garbage_records
        if "tg://drop" in record.get("detail", "")
    ]
    ledger.check(
        "intents garbage: dropped handler did not run",
        not dropped,
        dropped)

    _, isolated_plugins, isolated_audit = workspace(
        work, "plugin-intents-isolated")
    isolated = copy_fixture(isolated_plugins, "valid-intents.plugin")
    isolated_home = work / "plugin-intents-isolated-data"
    default = run_cli(
        cli, isolated_plugins, isolated_audit,
        ["plugin-level", "demo_intents"], isolated_home)
    ledger.check(
        "intents level: default gated",
        default.get("level") == 1,
        default)
    isolated_level = run_cli(
        cli, isolated_plugins, isolated_audit,
        ["plugin-level-set", "demo_intents", "0"], isolated_home)
    ledger.check(
        "intents level: isolated stored",
        isolated_level.get("level") == 0,
        isolated_level)
    data = run_cli(
        cli, isolated_plugins, isolated_audit,
        [
            "multi",
            json.dumps([
                {"op": "enable", "id": "demo_intents", "path": str(isolated)},
                {"op": "intent-test", "url": "tg://resolve?test=intent"},
            ]),
        ],
        isolated_home)
    ledger.check("intents isolated: two results", len(data) == 2, data)
    if len(data) == 2:
        ledger.check(
            "intents isolated: plugin loaded",
            data[0].get("ok") is True and data[0].get("loaded") is True,
            data[0])
        ledger.check(
            "intents isolated: not dispatched",
            data[1].get("handled") is False,
            data[1])
    ledger.check(
        "intents isolated: no log",
        not has_log(
            audit_records(isolated_audit),
            "demo_intents",
            "intent cancel_test tg://resolve?test=intent"),
        audit_records(isolated_audit))
    trusted_level = run_cli(
        cli, isolated_plugins, isolated_audit,
        ["plugin-level-set", "demo_intents", "2"], isolated_home)
    data = run_cli(
        cli, isolated_plugins, isolated_audit,
        [
            "multi",
            json.dumps([
                {"op": "enable", "id": "demo_intents", "path": str(isolated)},
                {"op": "intent-test", "url": "tg://resolve?test=intent"},
            ]),
        ],
        isolated_home)
    ledger.check(
        "intents level: trusted stored",
        trusted_level.get("level") == 2,
        trusted_level)
    ledger.check(
        "intents trusted: dispatched",
        len(data) == 2 and data[1].get("handled") is True,
        data)


def check_missing_python(ledger, cli, work):
    _, plugins, audit = workspace(work, "missing-python")
    valid = copy_fixture(plugins, "valid.plugin")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "demo_runtime", str(valid)], work / "data",
        extra_env={"PATH": "/nonexistent"})
    ledger.check("missing python: failed", data.get("ok") is False, data)
    ledger.check(
        "missing python: error",
        data.get("error") == "Python 3 was not found.",
        data.get("error"))


def check_crash(ledger, cli, work):
    _, plugins, audit = workspace(work, "crash")
    crash = copy_fixture(plugins, "crash.plugin")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "crash_plugin", str(crash)], work / "data")
    ledger.check("crash: load failed", data.get("ok") is False, data)
    valid = copy_fixture(plugins, "valid.plugin")
    restarted = run_cli(
        cli, plugins, audit,
        ["enable", "demo_runtime", str(valid)], work / "data")
    pid = restarted.get("pid")
    ledger.check("crash: restart ok", restarted.get("ok") is True, restarted)
    ledger.check(
        "crash: restart pid",
        isinstance(pid, int) and pid > 0,
        pid)
    ledger.check(
        "crash: no host left",
        host_pids(plugins) == [],
        host_pids(plugins))


def check_missing_base(ledger, cli, work):
    _, plugins, audit = workspace(work, "missing-base")
    fixture = copy_fixture(plugins, "missing-base.plugin")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "missing_base", str(fixture)], work / "data")
    ledger.check("missing base: failed", data.get("ok") is False, data)
    ledger.check(
        "missing base: base plugin in error",
        "BasePlugin" in data.get("error", ""),
        data.get("error"))


def check_id_mismatch(ledger, cli, work):
    _, plugins, audit = workspace(work, "id-mismatch")
    fixture = copy_fixture(plugins, "id-mismatch.plugin")
    data = run_cli(
        cli, plugins, audit,
        ["enable", "demo_runtime", str(fixture)], work / "data")
    ledger.check("id mismatch: failed", data.get("ok") is False, data)
    ledger.check(
        "id mismatch: id in error",
        "id" in data.get("error", "").lower(),
        data.get("error"))


def main():
    cli = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT_CLI
    if not cli.is_file():
        print(f"FAIL host CLI is missing at {cli}")
        return 2
    ledger = Ledger()
    work = Path(tempfile.mkdtemp(prefix="exteraless-plugin-host-checks-"))
    try:
        check_static(ledger)
        check_bwrap_environment(ledger, work)
        check_keyring(ledger, work)
        check_no_startup_host(ledger)
        check_valid_load(ledger, cli, work)
        check_sandbox_observe(ledger, cli, work)
        check_unload(ledger, cli, work)
        check_deny_write(ledger, cli, work)
        check_escape_write(ledger, cli, work)
        check_fork_escape(ledger, cli, work)
        check_escape_xattr(ledger, cli, work)
        check_deny_socket(ledger, cli, work)
        check_deny_subprocess(ledger, cli, work)
        check_deny_ctypes(ledger, cli, work)
        check_deny_exec(ledger, cli, work)
        check_escape_exec(ledger, cli, work)
        check_escape_child_markers(ledger, cli, work)
        check_deny_import(ledger, cli, work)
        check_foreign_import(ledger, cli, work)
        check_keyring_plugin(ledger, cli, work)
        check_print_noise(ledger, cli, work)
        check_paths(ledger, cli, work)
        check_bwrap_unavailable(ledger, cli, work)
        check_bwrap_override(ledger, cli, work)
        check_bwrap_failure(ledger, cli, work)
        check_protocol(ledger, cli, work)
        check_fd_hygiene(ledger, work)
        check_timeout(ledger, cli, work)
        check_single_host(ledger, cli, work)
        check_stop(ledger, cli, work)
        check_state(ledger, cli, work)
        check_config_whitelist(ledger, cli, work)
        check_plugin_settings(ledger, cli, work)
        check_plugin_menu(ledger, cli, work)
        check_plugin_intents(ledger, cli, work)
        check_missing_python(ledger, cli, work)
        check_crash(ledger, cli, work)
        check_missing_base(ledger, cli, work)
        check_id_mismatch(ledger, cli, work)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print()
    if ledger.failures:
        print(
            f"RESULT: FAIL ({len(ledger.failures)} of "
            f"{ledger.checks} checks)")
        for name in ledger.failures:
            print(f"  - {name}")
        return 1
    skipped = f", {len(ledger.skips)} skipped" if ledger.skips else ""
    print(f"RESULT: PASS ({ledger.checks} checks{skipped})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
