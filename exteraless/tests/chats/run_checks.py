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
EXPECTED_DEFAULTS = [True, False, False, False, True, True, False]


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
    text_cases = [
        ("see https://x.com/a?utm_source=1&id=2 now",
         "see https://x.com/a?id=2 now"),
        ("https://x.com/a?fbclid=1.",
         "https://x.com/a."),
        ("plain text", "plain text"),
        (
            "one https://x.com/a?utm_source=1 and "
            "https://y.com/b?fbclid=2",
            "one https://x.com/a and https://y.com/b",
        ),
        (
            "«https://x.com/a?utm_source=1»",
            "«https://x.com/a»",
        ),
        (
            "HTTPS://X.COM/A?UTM_SOURCE=1#frag",
            "HTTPS://X.COM/A#frag",
        ),
    ]
    for text, expected in text_cases:
        result = run(cli, ["strip-text", text])
        actual = result.stdout.rstrip("\n")
        ledger.check(
            f"strip-text {text}",
            result.returncode == 0 and actual == expected,
            f"exit {result.returncode}, got {actual!r}, want {expected!r}")
    return len(rows) + len(text_cases), len(ledger.failures) - before


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
        ("message-menu-set with wrong count",
         ["message-menu-set", "0", "1"]),
        ("deleted-trim without json", ["deleted-trim", "10", "10"]),
    ]
    before = len(ledger.failures)
    for name, arguments in cases:
        result = run(cli, arguments)
        ledger.check(
            f"cli {name}",
            result.returncode == 2 and bool(result.stderr.strip()),
            f"exit {result.returncode}, stderr {result.stderr!r}")
    return len(cases), len(ledger.failures) - before


def check_general(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "general-default")
    data = json_call(ledger, cli, ["general"], env, "general default")
    if data is not None:
        ledger.check(
            "general rounding default off",
            data.get("disableNumberRounding") is False,
            data)
        ledger.check(
            "general seconds default off",
            data.get("showSeconds") is False,
            data)
        ledger.check(
            "general hide phone default off",
            data.get("hidePhone") is False,
            data)

    env = make_env(work / "general-set")
    data = json_call(
        ledger, cli, ["general-set", "1", "1", "1"], env, "general set")
    if data is not None:
        ledger.check(
            "general rounding setter",
            data.get("disableNumberRounding") is True,
            data)
        ledger.check(
            "general seconds setter",
            data.get("showSeconds") is True,
            data)
        ledger.check(
            "general hide phone setter",
            data.get("hidePhone") is True,
            data)
    data = json_call(
        ledger, cli, ["general"], env, "general reload")
    if data is not None:
        ledger.check(
            "general rounding persisted",
            data.get("disableNumberRounding") is True,
            data)
        ledger.check(
            "general seconds persisted",
            data.get("showSeconds") is True,
            data)
        ledger.check(
            "general hide phone persisted",
            data.get("hidePhone") is True,
            data)

    data = json_call(
        ledger, cli,
        ["backup-export", "1", "0", "0", "0", "1", "0", "0"],
        env, "general backup export")
    if data is not None:
        section = data.get("exteraconfig") or {}
        ledger.check(
            "general backup key rounding",
            section.get("disableNumberRounding") is True,
            section)
        ledger.check(
            "general backup key seconds",
            section.get("formatTimeWithSeconds") is True,
            section)
        ledger.check(
            "general backup key hide phone",
            section.get("hidePhoneNumber") is True,
            section)

    env = make_env(work / "general-fromjson")
    root = {
        "version": 1,
        "exteraconfig": {
            "OEGeneralDisableNumberRounding": True,
            "OEGeneralShowSeconds": True,
            "OEGeneralHidePhone": True,
        },
    }
    data = json_call(
        ledger, cli, ["config-fromjson", json.dumps(root)], env,
        "general fromJson")
    if data is not None:
        ledger.check(
            "general fromJson accepted",
            data.get("accepted") is True,
            data)
        data = json_call(ledger, cli, ["general"], env, "general fromJson read")
        if data is not None:
            ledger.check(
                "general fromJson rounding",
                data.get("disableNumberRounding") is True,
                data)
            ledger.check(
                "general fromJson seconds",
                data.get("showSeconds") is True,
                data)
            ledger.check(
                "general fromJson hide phone",
                data.get("hidePhone") is True,
                data)

    clock_cases = [
        ("C short", ["clock", "14", "5", "9", "0", "C"], "14:05"),
        ("C long", ["clock", "14", "5", "9", "1", "C"], "14:05:09"),
        ("C midnight short", ["clock", "0", "0", "0", "0", "C"], "00:00"),
        ("C midnight long", ["clock", "0", "0", "0", "1", "C"], "00:00:00"),
        ("ru_RU long", ["clock", "14", "5", "9", "1", "ru_RU"], "14:05:09"),
    ]
    for name, args, expected in clock_cases:
        res = run(cli, args)
        actual = res.stdout.strip()
        ledger.check(
            f"clock {name}",
            res.returncode == 0 and actual == expected,
            f"exit {res.returncode}, got {actual!r}, want {expected!r}")

    res = run(cli, ["clock", "14", "5", "9", "1", "en_US"])
    actual = res.stdout.strip()
    ledger.check(
        "clock en_US long has seconds and PM",
        res.returncode == 0 and "2:05:09" in actual and "PM" in actual,
        f"got {actual!r}")

    return ledger.checks - before_checks, len(ledger.failures) - before_failures


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
        ledger, cli, ["config-set", "0", "1", "1", "0", "0", "0", "1"], env,
        "config setters")
    if data is not None:
        expected = [False, True, True, False, False, False, True]
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
            "OEChatsStripTrackingOnPaste": False,
            "OEChatsUnreadCapsule": True,
            "OEUnknownKey": True,
        },
    }
    data = json_call(
        ledger, cli, ["config-fromjson", json.dumps(root)], env,
        "config fromJson")
    if data is not None:
        expected = [False, True, False, True, False, False, True]
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


def backup_sections(data):
    return {
        "exteraconfig": data.get("exteraconfig") or {},
        "pillstackconfig": data.get("pillstackconfig") or {},
        "mainconfig": data.get("mainconfig") or {},
    }


def check_backup(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "backup-export")
    data = json_call(
        ledger, cli, ["backup-export", "0", "1", "0", "1", "0", "1", "1"], env,
        "backup export")
    encoded = None
    exported = None
    if data is not None:
        if not isinstance(data.get("exteraconfig"), dict):
            ledger.check(
                "backup exteraconfig object",
                False,
                data.get("exteraconfig"))
        exported = backup_sections(data)
        section = exported["exteraconfig"]
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
            "backup strip tracking keys",
            section.get("OEChatsStripTrackingOnOpen") is False
            and section.get("OEChatsStripTrackingOnPaste") is True,
            (section.get("OEChatsStripTrackingOnOpen"),
             section.get("OEChatsStripTrackingOnPaste")))
        ledger.check(
            "backup unread capsule key",
            section.get("OEChatsUnreadCapsule") is True,
            section.get("OEChatsUnreadCapsule"))
        ledger.check(
            "backup has no old private key",
            "hideReactionsInPrivate" not in section)
        ledger.check(
            "backup ghost booleans",
            all(section.get(key) is True for key in (
                "OEGhostSendReadPackets",
                "OEGhostSendReadStories",
                "OEGhostSendOnline",
                "OEGhostSendUploadProgress"))
            and section.get("OEGhostSendOfflineAfterOnline") is False,
            {key: section.get(key) for key in section
             if key.startswith("OEGhostSend")})
        ledger.check(
            "backup exclusions empty",
            section.get("OEGhostReadExclusions") == {}
            and section.get("OEGhostTypingExclusions") == {},
            (section.get("OEGhostReadExclusions"),
             section.get("OEGhostTypingExclusions")))
        ledger.check(
            "backup save deleted key",
            section.get("OESaveDeletedMessages") is False,
            section.get("OESaveDeletedMessages"))
        ledger.check(
            "backup proxy auto disabled key",
            section.get("OEProxyAutoDisabled") is False,
            section.get("OEProxyAutoDisabled"))
        ledger.check(
            "backup plugin states empty",
            section.get("OEPluginStates") == {},
            section.get("OEPluginStates"))
        ledger.check(
            "backup extera key count",
            len(section) == 34,
            sorted(section))
        ledger.check(
            "backup pill keys",
            exported["pillstackconfig"] == {
                "OEPillStackEnabled": False,
                "OEPillStackPills": 255,
            },
            exported["pillstackconfig"])
        ledger.check(
            "backup main keys",
            exported["mainconfig"] == {
                "OEMainMenuOrder":
                    "exteraless,ghost,pillstack,proxy,storage",
                "OEDrawerAccountsExpanded": True,
            },
            exported["mainconfig"])
        ledger.check(
            "backup total key count",
            sum(len(value) for value in exported.values()) == 38,
            {name: len(value) for name, value in exported.items()})
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
                "backup import applied all keys",
                data.get("applied") == 38,
                data.get("applied"))
            ledger.check(
                "backup import unknown empty",
                data.get("unknown") == [],
                data.get("unknown"))
            ledger.check(
                "backup import cache",
                data.get("chats")
                == [False, True, False, True, False, True, True],
                data.get("chats"))
        current = json_call(
            ledger, cli, ["backup-export-current"], import_env,
            "backup import re-export")
        if current is not None and exported is not None:
            ledger.check(
                "backup import roundtrip sections",
                backup_sections(current) == exported,
                backup_sections(current))

    env = make_env(work / "backup-import-full")
    full = {
        "version": 1,
        "exteraconfig": {
            "relativeLastSeen": True,
            "addCommaAfterMention": False,
            "disableNumberRounding": True,
            "formatTimeWithSeconds": True,
            "hidePhoneNumber": True,
            "doNotUseProxy": 5,
            "removeMessageTail": True,
            "hideReactionsInChannels": False,
            "hideReactionsInGroups": True,
            "hideReactionsInPrivateChats": False,
            "OEGhostSendReadPackets": False,
            "OEGhostSendReadStories": False,
            "OEGhostSendOnline": False,
            "OEGhostSendUploadProgress": False,
            "OEGhostSendOfflineAfterOnline": True,
            "OEGhostReadExclusions": {
                "123": True,
                "-456": True,
                "bad": True,
                "789": False,
                "0": True,
            },
            "OEGhostTypingExclusions": {"42": True},
            "OESaveDeletedMessages": True,
            "OEProxyAutoDisabled": True,
            "OEPluginStates": {
                "plugin-a": True,
                "bad id": True,
                "plugin-b": False,
            },
            "OEChatsStripTrackingOnOpen": False,
            "OEChatsStripTrackingOnPaste": False,
            "OEChatsUnreadCapsule": True,
            "OEChatsMenuReply": False,
            "OEChatsMenuEdit": True,
            "OEChatsMenuPin": False,
            "OEChatsMenuCopyText": True,
            "OEChatsMenuCopyLink": False,
            "OEChatsMenuCopyImage": True,
            "OEChatsMenuForward": False,
            "OEChatsMenuSelect": True,
            "OEChatsMenuTranslate": False,
            "OEChatsMenuReport": True,
            "OEChatsMenuDelete": False,
        },
        "pillstackconfig": {
            "OEPillStackEnabled": True,
            "OEPillStackPills": 5,
        },
        "mainconfig": {
            "OEMainMenuOrder": "ghost,exteraless",
            "OEDrawerAccountsExpanded": False,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(full)], env,
        "backup import full")
    if data is not None:
        ledger.check(
            "backup import full ok",
            data.get("ok") is True,
            data.get("ok"))
        ledger.check(
            "backup import full applied",
            data.get("applied") == 38,
            data.get("applied"))
        ledger.check(
            "backup import full unknown empty",
            data.get("unknown") == [],
            data.get("unknown"))
    current = json_call(
        ledger, cli, ["backup-export-current"], env, "backup full re-export")
    if current is not None:
        sections = backup_sections(current)
        section = sections["exteraconfig"]
        ledger.check(
            "backup full ghost flags",
            all(section.get(key) is False for key in (
                "OEGhostSendReadPackets",
                "OEGhostSendReadStories",
                "OEGhostSendOnline",
                "OEGhostSendUploadProgress"))
            and section.get("OEGhostSendOfflineAfterOnline") is True,
            {key: section.get(key) for key in section
             if key.startswith("OEGhostSend")})
        ledger.check(
            "backup full read exclusions sanitized",
            section.get("OEGhostReadExclusions") == {
                "123": True,
                "456": True,
            },
            section.get("OEGhostReadExclusions"))
        ledger.check(
            "backup full typing exclusions",
            section.get("OEGhostTypingExclusions") == {"42": True},
            section.get("OEGhostTypingExclusions"))
        ledger.check(
            "backup full plugin states sanitized",
            section.get("OEPluginStates") == {"plugin-a": True},
            section.get("OEPluginStates"))
        ledger.check(
            "backup full save deleted",
            section.get("OESaveDeletedMessages") is True,
            section.get("OESaveDeletedMessages"))
        ledger.check(
            "backup full proxy auto reset",
            section.get("OEProxyAutoDisabled") is False,
            section.get("OEProxyAutoDisabled"))
        ledger.check(
            "backup full generic keys",
            section.get("relativeLastSeen") is True
            and section.get("addCommaAfterMention") is False
            and section.get("disableNumberRounding") is True
            and section.get("doNotUseProxy") == 5
            and section.get("OEChatsStripTrackingOnOpen") is False
            and section.get("OEChatsStripTrackingOnPaste") is False
            and section.get("OEChatsUnreadCapsule") is True,
            section)
        ledger.check(
            "backup full pill keys",
            sections["pillstackconfig"] == {
                "OEPillStackEnabled": True,
                "OEPillStackPills": 5,
            },
            sections["pillstackconfig"])
        ledger.check(
            "backup full main keys",
            sections["mainconfig"] == {
                "OEMainMenuOrder": "ghost,exteraless",
                "OEDrawerAccountsExpanded": False,
            },
            sections["mainconfig"])

    env = make_env(work / "backup-unknown-sections")
    root = {
        "version": 1,
        "exteraconfig": {
            "removeMessageTail": True,
            "OEUnknown": True,
        },
        "pillstackconfig": {
            "OEPillStackPills": 300,
            "futurePill": 1,
        },
        "mainconfig": {
            "OEMainMenuOrder": "bad order",
            "OEDrawerAccountsExpanded": True,
        },
        "unknownsection": {"x": 1},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "backup unknown sections")
    if data is not None:
        ledger.check(
            "backup unknown sections ok",
            data.get("ok") is True,
            data.get("ok"))
        ledger.check(
            "backup unknown sections applied",
            data.get("applied") == 2,
            data.get("applied"))
        ledger.check(
            "backup unknown sections report",
            sorted(data.get("unknown") or []) == [
                "OEUnknown",
                "mainconfig.OEMainMenuOrder",
                "pillstackconfig.OEPillStackPills",
                "pillstackconfig.futurePill",
                "unknownsection",
            ],
            data.get("unknown"))

    env = make_env(work / "backup-legacy-eight")
    legacy = {
        "version": 1,
        "exteraconfig": {
            "relativeLastSeen": True,
            "addCommaAfterMention": False,
            "disableNumberRounding": True,
            "doNotUseProxy": 3,
            "removeMessageTail": False,
            "hideReactionsInChannels": True,
            "hideReactionsInGroups": True,
            "hideReactionsInPrivateChats": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(legacy)], env,
        "backup import legacy eight")
    if data is not None:
        ledger.check(
            "backup legacy eight applied",
            data.get("applied") == 8,
            data.get("applied"))
        ledger.check(
            "backup legacy eight unknown empty",
            data.get("unknown") == [],
            data.get("unknown"))
    current = json_call(
        ledger, cli, ["backup-export-current"], env,
        "backup legacy eight re-export")
    if current is not None:
        sections = backup_sections(current)
        ledger.check(
            "backup legacy eight completed to 38",
            (len(sections["exteraconfig"]),
             len(sections["pillstackconfig"]),
             len(sections["mainconfig"])) == (34, 2, 2),
            {name: len(value) for name, value in sections.items()})
        ledger.check(
            "backup legacy eight values",
            sections["exteraconfig"].get("doNotUseProxy") == 3
            and sections["exteraconfig"].get("removeMessageTail") is False
            and sections["exteraconfig"].get("hideReactionsInPrivateChats")
                is True,
            sections["exteraconfig"])

    env = make_env(work / "backup-flat-legacy")
    flat = {
        "version": 1,
        "relativeLastSeen": True,
        "removeMessageTail": False,
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(flat)], env,
        "backup import flat legacy")
    if data is not None:
        ledger.check(
            "backup flat legacy applied",
            data.get("applied") == 2,
            data.get("applied"))
        ledger.check(
            "backup flat legacy unknown empty",
            data.get("unknown") == [],
            data.get("unknown"))

    for index, (name, proxy, pills) in enumerate([
            ("fractional", 6.5, 5.9),
            ("negative", -1, -1),
            ("too big", 8, 256)]):
        env = make_env(work / f"backup-numeric-{index}")
        root = {
            "version": 1,
            "exteraconfig": {"doNotUseProxy": proxy},
            "pillstackconfig": {"OEPillStackPills": pills},
        }
        data = json_call(
            ledger, cli, ["backup-import-json", json.dumps(root)], env,
            f"backup numeric {name}")
        if data is not None:
            ledger.check(
                f"backup numeric {name} applied zero",
                data.get("applied") == 0,
                data.get("applied"))
            ledger.check(
                f"backup numeric {name} unknown",
                sorted(data.get("unknown") or []) == [
                    "doNotUseProxy",
                    "pillstackconfig.OEPillStackPills",
                ],
                data.get("unknown"))

    env = make_env(work / "backup-sanitize-load")
    config_path = (
        Path(env["XDG_DATA_HOME"])
        / "ExteralessDesktop" / "tdata" / "exteraless.json")
    config_path.parent.mkdir(parents=True, exist_ok=True)
    config_path.write_text(json.dumps({
        "version": 1,
        "exteraconfig": {
            "OEGhostReadExclusions": {
                "123": True,
                "-9": True,
                "bad": True,
                "456": False,
            },
            "OEPluginStates": {
                "plugin-a": True,
                "bad id": True,
                "plugin-b": False,
            },
        },
    }), encoding="utf-8")
    current = json_call(
        ledger, cli, ["backup-export-current"], env,
        "backup sanitize load")
    if current is not None:
        section = backup_sections(current)["exteraconfig"]
        ledger.check(
            "backup sanitize load exclusions",
            section.get("OEGhostReadExclusions") == {
                "123": True,
                "9": True,
            },
            section.get("OEGhostReadExclusions"))
        ledger.check(
            "backup sanitize load plugins",
            section.get("OEPluginStates") == {"plugin-a": True},
            section.get("OEPluginStates"))
    raw = json_call(
        ledger, cli, ["config-raw"], env, "backup sanitize load raw")
    if raw is not None:
        section = raw.get("exteraconfig") or {}
        ledger.check(
            "backup sanitize load raw exclusions",
            section.get("OEGhostReadExclusions") == {"123": True, "9": True},
            section.get("OEGhostReadExclusions"))
        ledger.check(
            "backup sanitize load raw plugins",
            section.get("OEPluginStates") == {"plugin-a": True},
            section.get("OEPluginStates"))

    env = make_env(work / "backup-sanitize-fromjson")
    root = {
        "version": 1,
        "exteraconfig": {
            "OEGhostReadExclusions": {"321": True, "bad": True},
            "OEPluginStates": {"plugin-b": True, "bad id": True},
        },
    }
    data = json_call(
        ledger, cli, ["config-fromjson", json.dumps(root)], env,
        "backup sanitize fromJson")
    if data is not None:
        ledger.check(
            "backup sanitize fromJson accepted",
            data.get("accepted") is True,
            data.get("accepted"))
    current = json_call(
        ledger, cli, ["backup-export-current"], env,
        "backup sanitize fromJson export")
    if current is not None:
        section = backup_sections(current)["exteraconfig"]
        ledger.check(
            "backup sanitize fromJson exclusions",
            section.get("OEGhostReadExclusions") == {"321": True},
            section.get("OEGhostReadExclusions"))
        ledger.check(
            "backup sanitize fromJson plugins",
            section.get("OEPluginStates") == {"plugin-b": True},
            section.get("OEPluginStates"))
    raw = json_call(
        ledger, cli, ["config-raw"], env, "backup sanitize fromJson raw")
    if raw is not None:
        section = raw.get("exteraconfig") or {}
        ledger.check(
            "backup sanitize fromJson raw exclusions",
            section.get("OEGhostReadExclusions") == {"321": True},
            section.get("OEGhostReadExclusions"))
        ledger.check(
            "backup sanitize fromJson raw plugins",
            section.get("OEPluginStates") == {"plugin-b": True},
            section.get("OEPluginStates"))
    saved_path = (
        Path(env["XDG_DATA_HOME"])
        / "ExteralessDesktop" / "tdata" / "exteraless.json")
    if saved_path.is_file():
        saved = json.loads(saved_path.read_text(encoding="utf-8"))
        saved_section = saved.get("exteraconfig") or {}
        ledger.check(
            "backup sanitize fromJson saved",
            saved_section.get("OEGhostReadExclusions") == {"321": True}
            and saved_section.get("OEPluginStates") == {"plugin-b": True},
            saved_section)
    else:
        ledger.check(
            "backup sanitize fromJson saved",
            False,
            str(saved_path))

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
            data.get("chats") == [True, True, True, True, True, True, False],
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


def check_deleted(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "deleted-default")
    data = json_call(ledger, cli, ["deleted"], env, "deleted default")
    if data is not None:
        ledger.check(
            "deleted default off",
            data.get("saveDeletedMessages") is False,
            data)

    env = make_env(work / "deleted-set")
    data = json_call(ledger, cli, ["deleted-set", "1"], env, "deleted setter")
    if data is not None:
        ledger.check(
            "deleted setter",
            data.get("saveDeletedMessages") is True,
            data)
    data = json_call(ledger, cli, ["deleted"], env, "deleted reload")
    if data is not None:
        ledger.check(
            "deleted persisted",
            data.get("saveDeletedMessages") is True,
            data)

    data = json_call(ledger, cli, ["deleted-set", "0"], env, "deleted disable")
    if data is not None:
        ledger.check(
            "deleted disable",
            data.get("saveDeletedMessages") is False,
            data)
    data = json_call(ledger, cli, ["deleted"], env, "deleted disable reload")
    if data is not None:
        ledger.check(
            "deleted disable persisted",
            data.get("saveDeletedMessages") is False,
            data)

    env = make_env(work / "deleted-fromjson")
    root = {
        "version": 1,
        "exteraconfig": {"OESaveDeletedMessages": True},
    }
    data = json_call(
        ledger, cli, ["config-fromjson", json.dumps(root)], env,
        "deleted fromJson")
    if data is not None:
        ledger.check(
            "deleted fromJson accepted",
            data.get("accepted") is True,
            data)
        data = json_call(ledger, cli, ["deleted"], env, "deleted fromJson read")
        if data is not None:
            ledger.check(
                "deleted fromJson value",
                data.get("saveDeletedMessages") is True,
                data)

    cases = [
        ("trim under limits", 10, 10,
         [[1, 1], [1, 2], [2, 1]],
         [[1, 1], [1, 2], [2, 1]]),
        ("trim global cap", 3, 10,
         [[1, 1], [1, 2], [1, 3], [1, 4], [1, 5]],
         [[1, 3], [1, 4], [1, 5]]),
        ("trim per peer cap", 10, 2,
         [[1, 1], [1, 2], [1, 3], [2, 1], [2, 2], [2, 3]],
         [[1, 2], [1, 3], [2, 2], [2, 3]]),
        ("trim combined caps", 3, 2,
         [[1, 1], [2, 1], [1, 2], [2, 2], [1, 3]],
         [[1, 2], [2, 2], [1, 3]]),
        ("trim zero global cap", 0, 10, [[1, 1]], []),
        ("trim zero per peer cap", 10, 0, [[1, 1]], []),
        ("trim negative cap", -1, 10, [[1, 1]], []),
        ("trim empty", 10, 10, [], []),
        ("trim default per peer cap", None, None,
         [[1, msg] for msg in range(1, 102)],
         [[1, msg] for msg in range(2, 102)]),
        ("trim default global cap", None, None,
         [[peer, peer] for peer in range(1, 1002)],
         [[peer, peer] for peer in range(2, 1002)]),
    ]
    for name, max_entries, max_per_peer, given, expected in cases:
        arguments = ["deleted-trim"]
        if max_entries is not None:
            arguments += [str(max_entries), str(max_per_peer)]
        arguments.append(json.dumps(given))
        data = json_call(ledger, cli, arguments, env, name)
        if data is not None:
            ledger.check(
                f"{name} entries",
                data.get("entries") == expected,
                data.get("entries"))
            ledger.check(
                f"{name} count",
                data.get("count") == len(expected),
                data.get("count"))

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_reset(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "reset-seed")
    root = {
        "version": 1,
        "exteraconfig": {
            "OEGeneralDisableNumberRounding": True,
            "OEChatsUnreadCapsule": True,
            "OEPillStackPills": 5,
            "OESaveDeletedMessages": True,
        },
    }
    data = json_call(
        ledger, cli, ["config-fromjson", json.dumps(root)], env,
        "reset seed")
    if data is not None:
        ledger.check(
            "reset seed accepted",
            data.get("accepted") is True,
            data.get("accepted"))
    data = json_call(ledger, cli, ["config-reset"], env, "reset")
    if data is not None:
        ledger.check(
            "reset clears config",
            data.get("exteraconfig") == {},
            data.get("exteraconfig"))
    data = json_call(ledger, cli, ["config-raw"], env, "reset raw")
    if data is not None:
        ledger.check(
            "reset raw empty",
            data.get("exteraconfig") == {},
            data.get("exteraconfig"))
    data = json_call(ledger, cli, ["config-loaded"], env, "reset reload")
    if data is not None:
        ledger.check(
            "reset defaults",
            data.get("chats") == EXPECTED_DEFAULTS,
            data.get("chats"))
    data = json_call(ledger, cli, ["deleted"], env, "reset deleted")
    if data is not None:
        ledger.check(
            "reset deleted default",
            data.get("saveDeletedMessages") is False,
            data)
    data = json_call(ledger, cli, ["general"], env, "reset general")
    if data is not None:
        ledger.check(
            "reset rounding default",
            data.get("disableNumberRounding") is False,
            data)
    saved_path = (
        Path(env["XDG_DATA_HOME"])
        / "ExteralessDesktop" / "tdata" / "exteraless.json")
    if saved_path.is_file():
        saved = json.loads(saved_path.read_text(encoding="utf-8"))
        ledger.check(
            "reset file without keys",
            saved.get("exteraconfig") == {},
            saved)
    else:
        ledger.check("reset file without keys", False, str(saved_path))

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_workdir(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "workdir-env")
    native = work / "workdir-native"
    env["EXTERALESS_WORKDIR"] = str(native)
    data = json_call(
        ledger, cli, ["general-set", "1"], env, "workdir set")
    if data is not None:
        ledger.check(
            "workdir set value",
            data.get("disableNumberRounding") is True,
            data)
    path = native / "tdata" / "exteraless.json"
    ledger.check("workdir file created", path.is_file(), str(path))
    if path.is_file():
        saved = json.loads(path.read_text(encoding="utf-8"))
        ledger.check(
            "workdir file value",
            (saved.get("exteraconfig") or {}).get(
                "OEGeneralDisableNumberRounding") is True,
            saved)
    old_path = (
        Path(env["XDG_DATA_HOME"])
        / "ExteralessDesktop" / "tdata" / "exteraless.json")
    ledger.check(
        "workdir avoids xdg path",
        not old_path.exists(),
        str(old_path))
    data = json_call(ledger, cli, ["general"], env, "workdir reload")
    if data is not None:
        ledger.check(
            "workdir reload value",
            data.get("disableNumberRounding") is True,
            data)

    migrate_root = work / "workdir-migrate"
    env = make_env(work / "workdir-migrate-env")
    env["EXTERALESS_WORKDIR"] = str(migrate_root)
    old_path = (
        Path(env["XDG_DATA_HOME"])
        / "ExteralessDesktop" / "tdata" / "exteraless.json")
    old_path.parent.mkdir(parents=True, exist_ok=True)
    old_path.write_text(json.dumps({
        "version": 1,
        "exteraconfig": {"OEGeneralDisableNumberRounding": True},
    }), encoding="utf-8")
    data = json_call(ledger, cli, ["general"], env, "workdir migrate read")
    if data is not None:
        ledger.check(
            "workdir migrate value",
            data.get("disableNumberRounding") is True,
            data)
    migrated = migrate_root / "tdata" / "exteraless.json"
    ledger.check(
        "workdir migrate copied",
        migrated.is_file() and old_path.is_file(),
        str(migrated))

    migrate_root = work / "workdir-existing"
    env = make_env(work / "workdir-existing-env")
    env["EXTERALESS_WORKDIR"] = str(migrate_root)
    existing = migrate_root / "tdata" / "exteraless.json"
    existing.parent.mkdir(parents=True, exist_ok=True)
    existing.write_text(json.dumps({
        "version": 1,
        "exteraconfig": {"OEGeneralDisableNumberRounding": False},
    }), encoding="utf-8")
    old_path = (
        Path(env["XDG_DATA_HOME"])
        / "ExteralessDesktop" / "tdata" / "exteraless.json")
    old_path.parent.mkdir(parents=True, exist_ok=True)
    old_path.write_text(json.dumps({
        "version": 1,
        "exteraconfig": {"OEGeneralDisableNumberRounding": True},
    }), encoding="utf-8")
    data = json_call(
        ledger, cli, ["general"], env, "workdir existing wins")
    if data is not None:
        ledger.check(
            "workdir existing wins value",
            data.get("disableNumberRounding") is False,
            data)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_message_menu(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "menu-defaults")
    data = json_call(ledger, cli, ["message-menu"], env, "menu defaults")
    expected_defaults = {
        "reply": True,
        "edit": True,
        "pin": True,
        "copyText": True,
        "copyLink": True,
        "copyImage": True,
        "forward": True,
        "select": True,
        "translate": True,
        "report": True,
        "deleteMsg": True,
    }
    if data is not None:
        ledger.check(
            "menu defaults all true",
            data.get("menu") == expected_defaults,
            data.get("menu"))

    env = make_env(work / "menu-set")
    data = json_call(
        ledger, cli,
        ["message-menu-set", "0", "1", "0", "1", "0", "1", "0", "1", "0", "1", "0"],
        env, "menu set")
    custom = {
        "reply": False,
        "edit": True,
        "pin": False,
        "copyText": True,
        "copyLink": False,
        "copyImage": True,
        "forward": False,
        "select": True,
        "translate": False,
        "report": True,
        "deleteMsg": False,
    }
    if data is not None:
        ledger.check(
            "menu set matches",
            data.get("menu") == custom,
            data.get("menu"))
    reloaded = json_call(ledger, cli, ["message-menu"], env, "menu reload")
    if reloaded is not None:
        ledger.check(
            "menu reload matches",
            reloaded.get("menu") == custom,
            reloaded.get("menu"))

    env = make_env(work / "menu-fromjson")
    root = {
        "version": 1,
        "exteraconfig": {
            "OEChatsMenuCopyImage": False,
            "OEChatsMenuReport": False,
            "OEChatsMenuTranslate": False,
        },
    }
    data = json_call(
        ledger, cli, ["config-fromjson", json.dumps(root)], env,
        "menu fromJson")
    if data is not None:
        ledger.check(
            "menu fromJson accepted",
            data.get("accepted") is True,
            data)
    reloaded = json_call(ledger, cli, ["message-menu"], env, "menu fromJson read")
    if reloaded is not None:
        menu = reloaded.get("menu") or {}
        ledger.check(
            "menu fromJson values",
            menu.get("copyImage") is False
            and menu.get("report") is False
            and menu.get("translate") is False
            and menu.get("reply") is True,
            menu)

    env = make_env(work / "menu-backup-aliases")
    root = {
        "version": 1,
        "exteraconfig": {
            "showCopyPhotoButton": False,
            "showReportButton": False,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "menu backup aliases")
    if data is not None:
        ledger.check(
            "menu backup aliases applied",
            data.get("applied") == 2,
            data.get("applied"))
        ledger.check(
            "menu backup aliases unknown empty",
            data.get("unknown") == [],
            data.get("unknown"))
    reloaded = json_call(ledger, cli, ["message-menu"], env, "menu aliases read")
    if reloaded is not None:
        menu = reloaded.get("menu") or {}
        ledger.check(
            "menu aliases applied to config",
            menu.get("copyImage") is False
            and menu.get("report") is False
            and menu.get("reply") is True,
            menu)

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
        general = check_general(ledger, cli, work)
        deleted = check_deleted(ledger, cli, work)
        reset = check_reset(ledger, cli, work)
        workdir = check_workdir(ledger, cli, work)
        menu = check_message_menu(ledger, cli, work)
        backup = check_backup(ledger, cli, work)
    print(f"strip: {strips[0] - strips[1]} passed")
    print(f"reactions: {reactions[0] - reactions[1]} passed")
    print(f"cli: {cli_args[0] - cli_args[1]} passed")
    print(f"config: {config[0] - config[1]} passed")
    print(f"general: {general[0] - general[1]} passed")
    print(f"deleted: {deleted[0] - deleted[1]} passed")
    print(f"reset: {reset[0] - reset[1]} passed")
    print(f"workdir: {workdir[0] - workdir[1]} passed")
    print(f"menu: {menu[0] - menu[1]} passed")
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
