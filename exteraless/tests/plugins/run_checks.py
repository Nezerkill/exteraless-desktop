#!/usr/bin/env python3
"""Backend regression checks for the Exteraless plugin scanner CLI."""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
DEFAULT_CLI = (
    ROOT / "out" / "exteraless" / "tests" / "plugins"
    / "exteraless-plugins-scan")
FIXTURES = Path(__file__).resolve().parent / "fixtures"
RUN_TIMEOUT = 60
HUGE_TERMS = 200000
MIB = 1024 * 1024
RECURSION_ERRORS = (
    "File is nested too deeply to scan.",
    "File is too large to parse.",
)


class Ledger:
    def __init__(self):
        self.checks = 0
        self.failures = []

    def check(self, name, condition, detail=""):
        self.checks += 1
        if condition:
            print(f"PASS {name}")
        else:
            suffix = f": {detail}" if detail else ""
            print(f"FAIL {name}{suffix}")
            self.failures.append(name)


def scan(cli, directory, env=None):
    result = subprocess.run(
        [str(cli), str(directory)],
        capture_output=True,
        text=True,
        env=env,
        timeout=RUN_TIMEOUT)
    if result.returncode != 0:
        raise RuntimeError(
            f"CLI exited {result.returncode} for {directory}: "
            f"{result.stderr.strip()}")
    return json.loads(result.stdout)


def entries_of(data):
    return {entry["file"]: entry for entry in data["entries"]}


def fake_python(root, name, body):
    folder = root / name / "bin"
    folder.mkdir(parents=True)
    script = folder / "python3"
    script.write_text(body)
    script.chmod(0o755)
    return folder


def env_with_path(folder):
    env = dict(os.environ)
    env["PATH"] = str(folder) + os.pathsep + env.get("PATH", "")
    return env


def check_fixtures(ledger, cli):
    data = scan(cli, FIXTURES)
    ledger.check("fixtures: no global error", data["error"] == "", data["error"])
    entries = entries_of(data)
    ledger.check(
        "fixtures: expected files",
        set(entries) == {
            "valid.plugin",
            "dynamic.plugin",
            "unknown-permission.plugin",
            "broken.plugin",
            "invalid-id.plugin",
            "legacy.elyx",
        },
        sorted(entries))
    ledger.check("fixtures: notes.txt ignored", "notes.txt" not in entries)
    valid = entries.get("valid.plugin", {})
    ledger.check(
        "fixtures: valid metadata",
        (valid.get("id") == "demo_plugin"
         and valid.get("name") == "Demo Plugin"
         and valid.get("version") == "1.0.0"
         and valid.get("author") == "Exteraless"
         and valid.get("permissions") == ["ui", "messages.read"]
         and valid.get("error") == ""),
        valid)
    expected = {
        "dynamic.plugin": "Metadata __name__ is not a literal.",
        "unknown-permission.plugin": "Unknown permission: root.",
        "invalid-id.plugin": "Invalid id: 1bad.",
        "legacy.elyx": "Unsupported plugin format: .elyx.",
    }
    for name, message in expected.items():
        ledger.check(
            f"fixtures: {name} error",
            entries.get(name, {}).get("error") == message,
            entries.get(name, {}).get("error"))
    broken = entries.get("broken.plugin", {}).get("error", "")
    ledger.check(
        "fixtures: broken.plugin syntax error",
        broken.startswith("Syntax error:"),
        broken)


def check_recursion(ledger, cli, work):
    folder = work / "recursion"
    folder.mkdir()
    (folder / "ok.plugin").write_text(
        '__id__ = "ok_plugin"\n__name__ = "OK Plugin"\n')
    (folder / "huge.plugin").write_text(
        "x = " + "+".join(["1"] * HUGE_TERMS) + "\n")
    data = scan(cli, folder)
    entries = entries_of(data)
    ledger.check("recursion: no global error", data["error"] == "", data["error"])
    ledger.check(
        "recursion: both entries kept",
        set(entries) == {"ok.plugin", "huge.plugin"},
        sorted(entries))
    ledger.check(
        "recursion: ok.plugin intact",
        entries.get("ok.plugin", {}).get("id") == "ok_plugin",
        entries.get("ok.plugin"))
    huge_error = entries.get("huge.plugin", {}).get("error", "")
    ledger.check(
        "recursion: huge.plugin per-file error",
        huge_error in RECURSION_ERRORS,
        huge_error)


def check_size_limit(ledger, cli, work):
    folder = work / "oversize"
    folder.mkdir()
    header = '__id__ = "too_big"\n__name__ = "Too Big"\n'
    (folder / "too-big.plugin").write_text(header + "#" * (2 * MIB))
    data = scan(cli, folder)
    entry = entries_of(data).get("too-big.plugin", {})
    ledger.check(
        "oversize: per-file error",
        entry.get("error") == "File is larger than 2 MiB.",
        entry)
    ledger.check("oversize: no global error", data["error"] == "", data["error"])

    boundary = work / "at-limit"
    boundary.mkdir()
    at_limit = "__id__ = \"at_limit\"\n__name__ = \"At Limit\"\n"
    filler = (2 * MIB) - len(at_limit.encode("utf-8"))
    (boundary / "at-limit.plugin").write_text(at_limit + "#" * filler)
    data = scan(cli, boundary)
    entry = entries_of(data).get("at-limit.plugin", {})
    ledger.check(
        "2 MiB exactly: accepted",
        entry.get("error") == "" and entry.get("id") == "at_limit",
        entry)


def check_missing_python(ledger, cli, work):
    empty = work / "empty-bin"
    empty.mkdir()
    env = dict(os.environ)
    env["PATH"] = str(empty)
    data = scan(cli, FIXTURES, env=env)
    ledger.check(
        "missing python: global error",
        "not found" in data["error"],
        data["error"])
    ledger.check(
        "missing python: no entries",
        data["entries"] == [],
        data["entries"])


def check_process_failures(ledger, cli, work):
    bad = fake_python(work, "bad-json", '#!/bin/sh\nprintf "not json"\n')
    data = scan(cli, FIXTURES, env=env_with_path(bad))
    ledger.check(
        "bad json: invalid output",
        data["error"] == "Plugin scan returned invalid output.",
        data["error"])

    nonzero = fake_python(
        work, "nonzero", "#!/bin/sh\necho boom >&2\nexit 1\n")
    data = scan(cli, FIXTURES, env=env_with_path(nonzero))
    ledger.check(
        "nonzero exit: stderr in global error",
        "boom" in data["error"],
        data["error"])
    ledger.check(
        "nonzero exit: no entries",
        data["entries"] == [],
        data["entries"])


def check_timeout(ledger, cli, work):
    slow = fake_python(work, "timeout", "#!/bin/sh\nsleep 30\n")
    started = time.monotonic()
    data = scan(cli, FIXTURES, env=env_with_path(slow))
    elapsed = time.monotonic() - started
    ledger.check(
        "timeout: global error",
        data["error"] == "Plugin scan timed out.",
        data["error"])
    ledger.check(
        "timeout: elapsed about 10s",
        8 <= elapsed <= 25,
        f"{elapsed:.1f}s")
    ledger.check(
        "timeout: no entries",
        data["entries"] == [],
        data["entries"])


def check_single_process(ledger, cli, work):
    folder = work / "single-process"
    folder.mkdir()
    counter = work / "invocations"
    script = fake_python(
        work,
        "count",
        f"#!/bin/sh\nprintf x >> '{counter}'\nprintf '[]'\n")
    data = scan(cli, folder, env=env_with_path(script))
    count = counter.read_text().count("x") if counter.exists() else 0
    ledger.check("one python process per directory", count == 1, count)
    ledger.check(
        "one process: empty result",
        data["error"] == "",
        data["error"])


def check_no_execution(ledger, cli, work):
    folder = work / "no-execution"
    folder.mkdir()
    marker = work / "plugin-executed"
    (folder / "exec-check.plugin").write_text(
        '__id__ = "exec_check"\n'
        '__name__ = "Exec Check"\n'
        f'open("{marker}", "w").write("bad")\n')
    data = scan(cli, folder)
    entry = entries_of(data).get("exec-check.plugin", {})
    ledger.check("plugin code not executed", not marker.exists())
    ledger.check(
        "exec-check: metadata scanned cleanly",
        entry.get("error") == "" and entry.get("name") == "Exec Check",
        entry)


def check_skipped_files(ledger, cli, work):
    folder = work / "skipped"
    folder.mkdir()
    (folder / "valid.plugin").write_text(
        (FIXTURES / "valid.plugin").read_text())
    (folder / "notes.txt").write_text("ignored\n")
    (folder / "folder.py").mkdir()
    try:
        os.symlink(folder / "valid.plugin", folder / "linked.plugin")
    except (OSError, NotImplementedError):
        pass
    data = scan(cli, folder)
    entries = entries_of(data)
    ledger.check(
        "skipped: only regular plugins listed",
        set(entries) == {"valid.plugin"},
        sorted(entries))
    ledger.check("skipped: symlink not read", "linked.plugin" not in entries)
    ledger.check("skipped: directory not read", "folder.py" not in entries)
    ledger.check("skipped: txt not read", "notes.txt" not in entries)


def main():
    cli = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT_CLI
    if not cli.is_file():
        print(f"FAIL scanner CLI is missing at {cli}")
        return 2
    ledger = Ledger()
    work = Path(tempfile.mkdtemp(prefix="exteraless-plugins-checks-"))
    try:
        check_fixtures(ledger, cli)
        check_recursion(ledger, cli, work)
        check_size_limit(ledger, cli, work)
        check_missing_python(ledger, cli, work)
        check_process_failures(ledger, cli, work)
        check_timeout(ledger, cli, work)
        check_single_process(ledger, cli, work)
        check_no_execution(ledger, cli, work)
        check_skipped_files(ledger, cli, work)
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
    print(f"RESULT: PASS ({ledger.checks} checks)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
