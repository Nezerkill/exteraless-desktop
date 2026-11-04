#!/usr/bin/env python3
"""Regression checks for the Exteraless chats helpers and config/backup."""

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
DEFAULT_CLI = (
    ROOT / "out" / "exteraless" / "tests" / "chats"
    / "exteraless-chats-cli")
FIXTURES = Path(__file__).resolve().parent / "fixtures"
RUN_TIMEOUT = 30
MIN_STRIP_CASES = 38
MIN_REACTION_CASES = 10
EXPECTED_DEFAULTS = [True, False, False, False, True, False]


class Ledger:
    def __init__(self):
        self.checks = 0
        self.failures = []

    def check(self, name, condition, detail=""):
        self.checks += 1
        if condition:
            return
        suffix = f": {detail}" if detail else ""
        print(f"FAIL {name}{suffix}")
        self.failures.append(name)


def run(cli, arguments, env=None):
    return subprocess.run(
        [str(cli), *arguments],
        capture_output=True,
        text=True,
        env=env,
        timeout=RUN_TIMEOUT)


def make_env(root):
    data = root / "data"
    cache = root / "cache"
    data.mkdir(parents=True)
    cache.mkdir(parents=True)
    env = dict(os.environ)
    env["XDG_DATA_HOME"] = str(data)
    env["XDG_CACHE_HOME"] = str(cache)
    return env


def json_call(ledger, cli, arguments, env, name):
    result = run(cli, arguments, env=env)
    if result.returncode != 0:
        ledger.check(
            name,
            False,
            f"exit {result.returncode}, stderr {result.stderr.strip()!r}")
        return None
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError as error:
        ledger.check(name, False, f"invalid JSON: {error}")
        return None


def load_rows(name, columns, minimum):
    rows = []
    for number, line in enumerate(
            (FIXTURES / name).read_text(encoding="utf-8").splitlines(),
            start=1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != columns:
            raise SystemExit(
                f"{name}:{number}: expected {columns} columns, "
                f"got {len(fields)}")
        rows.append(fields)
    if len(rows) < minimum:
        raise SystemExit(
            f"{name}: expected at least {minimum} cases, got {len(rows)}")
    return rows


def check_strip(ledger, cli):
    rows = load_rows("strip.tsv", 2, MIN_STRIP_CASES)
    before = len(ledger.failures)
    for url, expected in rows:
        result = run(cli, ["strip", url])
        actual = result.stdout.rstrip("\n")
        ledger.check(
            f"strip {url}",
            result.returncode == 0 and actual == expected,
            f"exit {result.returncode}, got {actual!r}, want {expected!r}")
    return len(rows), len(ledger.failures) - before


def check_reactions(ledger, cli):
    rows = load_rows("reactions.tsv", 7, MIN_REACTION_CASES)
    before = len(ledger.failures)
    for row in rows:
        result = run(cli, ["hidden", *row[:6]])
        actual = result.stdout.rstrip("\n")
        ledger.check(
            f"reactions {'/'.join(row[:6])}",
            result.returncode == 0 and actual == row[6],
            f"exit {result.returncode}, got {actual!r}, want {row[6]!r}")
    return len(rows), len(ledger.failures) - before


def check_cli_args(ledger, cli):
    cases = [
        ("strip without URL", ["strip"]),
        ("hidden with five values", ["hidden", "0", "0", "0", "1", "0"]),
        ("config-set with five values",
         ["config-set", "0", "1", "0", "1", "0"]),
    ]
    before = len(ledger.failures)
    for name, arguments in cases:
        result = run(cli, arguments)
        ledger.check(
            f"cli {name}",
            result.returncode == 2 and bool(result.stderr.strip()),
            f"exit {result.returncode}, stderr {result.stderr!r}")
    return len(cases), len(ledger.failures) - before


def check_config(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "config-defaults")
    data = json_call(ledger, cli, ["config-defaults"], env, "config defaults")
    if data is not None:
        ledger.check(
            "config defaults 1/0/0/0/1",
            data.get("chats") == EXPECTED_DEFAULTS,
            data.get("chats"))

    env = make_env(work / "config-setters")
    data = json_call(
        ledger, cli, ["config-set", "0", "1", "1", "0", "0", "1"], env,
        "config setters")
    if data is not None:
        expected = [False, True, True, False, False, True]
        ledger.check(
            "config setters refresh cache",
            data.get("chats") == expected,
            data.get("chats"))
        reloaded = json_call(
            ledger, cli, ["config-loaded"], env, "config reload")
        if reloaded is not None:
            ledger.check(
                "config reload persisted",
                reloaded.get("chats") == expected,
                reloaded.get("chats"))

    env = make_env(work / "config-fromjson")
    root = {
        "version": 1,
        "exteraconfig": {
            "OEChatsRemoveMessageTail": False,
            "OEChatsHideReactionsChannels": True,
            "OEChatsHideReactionsGroups": False,
            "OEChatsHideReactionsPrivate": True,
            "OEChatsStripTrackingOnOpen": False,
            "OEChatsUnreadCapsule": True,
            "OEUnknownKey": True,
        },
    }
    data = json_call(
        ledger, cli, ["config-fromjson", json.dumps(root)], env,
        "config fromJson")
    if data is not None:
        expected = [False, True, False, True, False, True]
        ledger.check(
            "config fromJson accepted",
            data.get("accepted") is True,
            data.get("accepted"))
        ledger.check(
            "config fromJson unknown",
            data.get("unknown") == ["OEUnknownKey"],
            data.get("unknown"))
        ledger.check(
            "config fromJson cache",
            data.get("chats") == expected,
            data.get("chats"))
        reloaded = json_call(
            ledger, cli, ["config-loaded"], env, "config fromJson reload")
        if reloaded is not None:
            ledger.check(
                "config fromJson persisted",
                reloaded.get("chats") == expected,
                reloaded.get("chats"))

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_backup(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "backup-export")
    data = json_call(
        ledger, cli, ["backup-export", "0", "1", "0", "1", "0", "1"], env,
        "backup export")
    encoded = None
    if data is not None:
        section = data.get("exteraconfig")
        if not isinstance(section, dict):
            ledger.check("backup exteraconfig object", False, section)
            section = {}
        ledger.check(
            "backup removeMessageTail",
            section.get("removeMessageTail") is False,
            section.get("removeMessageTail"))
        ledger.check(
            "backup hideReactionsInChannels",
            section.get("hideReactionsInChannels") is True,
            section.get("hideReactionsInChannels"))
        ledger.check(
            "backup hideReactionsInGroups",
            section.get("hideReactionsInGroups") is False,
            section.get("hideReactionsInGroups"))
        ledger.check(
            "backup hideReactionsInPrivateChats",
            section.get("hideReactionsInPrivateChats") is True,
            section.get("hideReactionsInPrivateChats"))
        ledger.check(
            "backup has no strip tracking key",
            ("OEChatsStripTrackingOnOpen" not in section
             and "stripTrackingOnOpen" not in section))
        ledger.check(
            "backup has no unread capsule key",
            ("OEChatsUnreadCapsule" not in section
             and "unreadCapsule" not in section))
        ledger.check(
            "backup has no old private key",
            "hideReactionsInPrivate" not in section)
        encoded = data.get("backup_base64")
        ledger.check(
            "backup base64 present",
            isinstance(encoded, str) and bool(encoded),
            encoded)

    import_env = make_env(work / "backup-import")
    if encoded:
        data = json_call(
            ledger, cli, ["backup-import", encoded], import_env,
            "backup import")
        if data is not None:
            ledger.check(
                "backup import ok",
                data.get("ok") is True,
                data.get("ok"))
            ledger.check(
                "backup import unknown empty",
                data.get("unknown") == [],
                data.get("unknown"))
            ledger.check(
                "backup import cache",
                data.get("chats") == [False, True, False, True, True, False],
                data.get("chats"))

    env = make_env(work / "backup-import-four")
    root = {
        "version": 1,
        "exteraconfig": {
            "removeMessageTail": True,
            "hideReactionsInChannels": True,
            "hideReactionsInGroups": True,
            "hideReactionsInPrivateChats": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "backup import four")
    if data is not None:
        ledger.check(
            "backup import four ok",
            data.get("ok") is True,
            data.get("ok"))
        ledger.check(
            "backup import applied four keys",
            data.get("applied") == 4,
            data.get("applied"))
        ledger.check(
            "backup import four unknown empty",
            data.get("unknown") == [],
            data.get("unknown"))
        ledger.check(
            "backup import four cache",
            data.get("chats") == [True, True, True, True, True, False],
            data.get("chats"))

    env = make_env(work / "backup-wrong-type")
    root = {
        "version": 1,
        "exteraconfig": {"removeMessageTail": "yes"},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "backup wrong type")
    if data is not None:
        ledger.check(
            "backup wrong type ok",
            data.get("ok") is True,
            data.get("ok"))
        ledger.check(
            "backup wrong type applied zero",
            data.get("applied") == 0,
            data.get("applied"))
        ledger.check(
            "backup wrong type unknown",
            data.get("unknown") == ["removeMessageTail"],
            data.get("unknown"))
        ledger.check(
            "backup wrong type cache defaults",
            data.get("chats") == EXPECTED_DEFAULTS,
            data.get("chats"))

    env = make_env(work / "backup-old-name")
    root = {
        "version": 1,
        "exteraconfig": {"hideReactionsInPrivate": True},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "backup old name")
    if data is not None:
        ledger.check(
            "backup old name ok",
            data.get("ok") is True,
            data.get("ok"))
        ledger.check(
            "backup old name applied zero",
            data.get("applied") == 0,
            data.get("applied"))
        ledger.check(
            "backup old name unknown",
            data.get("unknown") == ["hideReactionsInPrivate"],
            data.get("unknown"))
        ledger.check(
            "backup old name cache defaults",
            data.get("chats") == EXPECTED_DEFAULTS,
            data.get("chats"))

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def main():
    cli = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT_CLI
    if not cli.is_file():
        print(f"FAIL chats CLI is missing at {cli}")
        return 2
    ledger = Ledger()
    strips = check_strip(ledger, cli)
    reactions = check_reactions(ledger, cli)
    cli_args = check_cli_args(ledger, cli)
    with tempfile.TemporaryDirectory(
            prefix="exteraless-chats-checks-") as temp:
        work = Path(temp)
        config = check_config(ledger, cli, work)
        backup = check_backup(ledger, cli, work)
    print(f"strip: {strips[0] - strips[1]} passed")
    print(f"reactions: {reactions[0] - reactions[1]} passed")
    print(f"cli: {cli_args[0] - cli_args[1]} passed")
    print(f"config: {config[0] - config[1]} passed")
    print(f"backup: {backup[0] - backup[1]} passed")
    if ledger.failures:
        print(
            f"RESULT: FAIL ({len(ledger.failures)} failed "
            f"of {ledger.checks} checks)")
        return 1
    print(f"RESULT: PASS ({ledger.checks} checks)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
