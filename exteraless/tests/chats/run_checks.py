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
        ("doubletap-set with wrong count", ["doubletap-set", "1"]),
        ("doubletap-resolve without value", ["doubletap-resolve"]),
        ("stickertime-set with wrong count", ["stickertime-set", "1"]),
        ("avatar-corners-set with wrong count", ["avatar-corners-set"]),
        ("pillorder-set without order", ["pillorder-set"]),
        ("pillorder-reset with args", ["pillorder-reset", "x"]),
        ("ghost-set with wrong count", ["ghost-set"]),
        ("ghost-lock with wrong count", ["ghost-lock", "1"]),
        ("deletedmark-set with wrong count", ["deletedmark-set", "1"]),
        ("vpn-name without name", ["vpn-name"]),
        ("proxy-rule with wrong count", ["proxy-rule", "0"]),
        ("pangu-apply without text", ["pangu-apply"]),
        ("silent-set with wrong count", ["silent-set"]),
        ("settings-sections-set with wrong count",
         ["settings-sections-set", "1"]),
        ("ai-editor-set with wrong count", ["ai-editor-set"]),
        ("plugin-filename without name", ["plugin-filename"]),
        ("send-as-set with wrong count", ["send-as-set"]),
        ("profile-buttons-set with wrong count", ["profile-buttons-set"]),
        ("bot-privacy-set with wrong count", ["bot-privacy-set", "1", "2"]),
        ("bot-privacy-set invalid enabled",
         ["bot-privacy-set", "2", "1", "0"]),
        ("inline-math-set with wrong count", ["inline-math-set"]),
        ("save-subfolder-set with wrong count", ["save-subfolder-set"]),
        ("ghost-schedule-set with wrong count", ["ghost-schedule-set"]),
        ("seek-duration-set without value", ["seek-duration-set"]),
        ("edited-mark-set with wrong count", ["edited-mark-set", "1"]),
        ("forward-date-set with wrong count", ["forward-date-set"]),
        ("sender-mini-avatars-set with wrong count",
         ["sender-mini-avatars-set"]),
        ("media-menu-set with wrong count", ["media-menu-set", "1"]),
        ("chat-menu-set with wrong count", ["chat-menu-set", "1"]),
        ("disable-link-preview-set with wrong count",
         ["disable-link-preview-set"]),
        ("hide-all-chats-set with wrong count",
         ["hide-all-chats-set"]),
        ("greeting-sticker-set with wrong count",
         ["greeting-sticker-set"]),
        ("panels-on-scroll-set with wrong count",
         ["panels-on-scroll-set"]),
        ("channel-bottom-button-set with wrong count",
         ["channel-bottom-button-set"]),
        ("channel-bottom-button-set out of range",
         ["channel-bottom-button-set", "3"]),
        ("download-boost-set with wrong count",
         ["download-boost-set"]),
        ("upload-boost-set with wrong count",
         ["upload-boost-set"]),
        ("always-show-download-set with wrong count",
         ["always-show-download-set"]),
        ("false-bottom-set with wrong count", ["false-bottom-set"]),
        ("decoration-set with wrong count", ["decoration-set"]),
        ("decoration-set out of range", ["decoration-set", "7"]),
        ("shortcuts-set with wrong count", ["shortcuts-set"]),
        ("message-menu-style-set with wrong count",
         ["message-menu-style-set", "1"]),
        ("ghost-schedule-decide with wrong count",
         ["ghost-schedule-decide", "1"]),
        ("chat-folder without name", ["chat-folder"]),
        ("chat-subfolder without args", ["chat-subfolder"]),
        ("math-eval without expression", ["math-eval"]),
        ("plugin-install without args", ["plugin-install"]),
        ("plugin-delete without args", ["plugin-delete"]),
        ("deleted-trim without json", ["deleted-trim", "10", "10"]),
        ("deleted-entry without json", ["deleted-entry"]),
        ("autodownload-archive-set with wrong count",
         ["autodownload-archive-set"]),
        ("win32-exe-set with wrong count", ["win32-exe-set"]),
        ("subtitles-parse without file", ["subtitles-parse"]),
        ("subtitles-active without position", ["subtitles-active", "a.srt"]),
        ("subtitles-key without id", ["subtitles-key"]),
        ("lyrics-parse without file", ["lyrics-parse"]),
        ("lyrics-active without position", ["lyrics-active", "a.lrc"]),
        ("header-style-set with wrong count", ["header-style-set"]),
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


def check_bot_privacy(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "bot-privacy-defaults")
    data = json_call(
        ledger, cli, ["bot-privacy"], env, "bot privacy defaults")
    if data is not None:
        ledger.check(
            "bot privacy defaults",
            data.get("enabled") is False
            and data.get("account") == -1
            and data.get("alwaysAsk") is False,
            data)

    data = json_call(
        ledger, cli, ["bot-privacy-set", "1", "2", "1"], env,
        "bot privacy set")
    if data is not None:
        ledger.check(
            "bot privacy set",
            data.get("enabled") is True
            and data.get("account") == 2
            and data.get("alwaysAsk") is True,
            data)
    reloaded = json_call(
        ledger, cli, ["bot-privacy"], env, "bot privacy reload")
    if reloaded is not None:
        ledger.check(
            "bot privacy persisted",
            reloaded.get("enabled") is True
            and reloaded.get("account") == 2
            and reloaded.get("alwaysAsk") is True,
            reloaded)

    data = json_call(
        ledger, cli, ["bot-privacy-set", "1", "0", "1"], env,
        "bot privacy account zero")
    if data is not None:
        ledger.check(
            "bot privacy account zero is off",
            data.get("account") == 0,
            data)

    data = json_call(
        ledger, cli, ["bot-privacy-set", "1", "99", "1"], env,
        "bot privacy clamp high")
    if data is not None:
        ledger.check(
            "bot privacy account clamped to 15",
            data.get("account") == 15,
            data)
    data = json_call(
        ledger, cli, ["bot-privacy-set", "0", "-9", "0"], env,
        "bot privacy clamp low")
    if data is not None:
        ledger.check(
            "bot privacy account clamped to -1",
            data.get("account") == -1,
            data)
        ledger.check(
            "bot privacy disabled",
            data.get("enabled") is False
            and data.get("alwaysAsk") is False,
            data)

    env = make_env(work / "bot-privacy-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "botPrivacyEnabled": True,
            "botPrivacyUserId": 3,
            "alwaysAsk": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "bot privacy alias")
    if data is not None:
        ledger.check(
            "bot privacy alias applied",
            data.get("applied") == 3 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["bot-privacy"], env, "bot privacy alias read")
    if reloaded is not None:
        ledger.check(
            "bot privacy alias values",
            reloaded.get("enabled") is True
            and reloaded.get("account") == 3
            and reloaded.get("alwaysAsk") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


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


def check_regex_filters(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "regex-defaults")
    data = json_call(ledger, cli, ["regex-filters"], env, "regex defaults")
    if data is not None:
        ledger.check(
            "regex default disabled",
            data.get("enabled") is False,
            data)
        ledger.check(
            "regex default count",
            data.get("count") == 0,
            data)
    listed = json_call(
        ledger, cli, ["regex-filter-list"], env, "regex default list")
    if listed is not None:
        ledger.check(
            "regex default list empty",
            listed.get("filters") == [] and listed.get("count") == 0,
            listed)

    for name, arguments in [
            ("regex-filter-set without flags", ["regex-filter-set", "spam"]),
            ("regex-filter-set bad flag",
             ["regex-filter-set", "spam", "2", "0"]),
            ("regex-test without text", ["regex-test"])]:
        result = run(cli, arguments, env=env)
        ledger.check(
            f"cli {name}",
            result.returncode == 2 and bool(result.stderr.strip()),
            f"exit {result.returncode}, stderr {result.stderr!r}")

    result = run(cli, ["regex-filter-set", "(", "0", "0"], env=env)
    ledger.check(
        "regex invalid pattern rejected",
        result.returncode == 2 and bool(result.stderr.strip()),
        f"exit {result.returncode}, stderr {result.stderr!r}")
    data = json_call(
        ledger, cli, ["regex-filters"], env, "regex after invalid")
    if data is not None:
        ledger.check(
            "regex invalid pattern not stored",
            data.get("count") == 0 and data.get("enabled") is False,
            data)

    env = make_env(work / "regex-enabled")
    root = {
        "version": 1,
        "exteraconfig": {
            "regexFiltersEnabled": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "regex enabled import")
    if data is not None:
        ledger.check(
            "regex enabled applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    data = json_call(
        ledger, cli, ["regex-filters"], env, "regex enabled state")
    if data is not None:
        ledger.check(
            "regex enabled flag",
            data.get("enabled") is True and data.get("count") == 0,
            data)

    data = json_call(
        ledger, cli, ["regex-filter-set", "spam", "1", "0"], env,
        "regex add filter")
    if data is not None:
        ledger.check(
            "regex add count",
            data.get("count") == 1,
            data)
    listed = json_call(
        ledger, cli, ["regex-filter-list"], env, "regex list after add")
    if listed is not None:
        filters = listed.get("filters") or []
        ledger.check(
            "regex filter fields",
            listed.get("count") == 1
            and filters == [{
                "regex": "spam",
                "caseInsensitive": True,
                "reversed": False,
                "enabled": True,
            }],
            listed)

    for text, expected in [("SPAM now", True), ("ham", False), ("", False)]:
        result = json_call(
            ledger, cli, ["regex-test", text], env, f"regex test {text!r}")
        if result is not None:
            ledger.check(
                f"regex test {text!r}",
                result.get("hidden") is expected,
                result)

    data = json_call(
        ledger, cli, ["regex-filter-set", "^ok$", "0", "1"], env,
        "regex reversed add")
    if data is not None:
        ledger.check(
            "regex reversed add count",
            data.get("count") == 2,
            data)
    for text, expected in [("ok", False), ("hello", True)]:
        result = json_call(
            ledger, cli, ["regex-test", text], env,
            f"regex reversed test {text!r}")
        if result is not None:
            ledger.check(
                f"regex reversed test {text!r}",
                result.get("hidden") is expected,
                result)

    data = json_call(
        ledger, cli, ["regex-filter-clear"], env, "regex clear")
    if data is not None:
        ledger.check(
            "regex clear count",
            data.get("count") == 0,
            data)
    listed = json_call(
        ledger, cli, ["regex-filter-list"], env, "regex list after clear")
    if listed is not None:
        ledger.check(
            "regex clear list empty",
            listed.get("filters") == [] and listed.get("count") == 0,
            listed)
    result = json_call(
        ledger, cli, ["regex-test", "hello"], env, "regex test after clear")
    if result is not None:
        ledger.check(
            "regex test after clear",
            result.get("hidden") is False,
            result)

    env = make_env(work / "regex-alias")
    filter_data = json.dumps([{
        "regex": "spam",
        "caseInsensitive": True,
        "reversed": False,
        "enabled": True,
    }])
    excluded = json.dumps([-100123])
    root = {
        "version": 1,
        "exteraconfig": {
            "regexFiltersEnabled": True,
            "regexFiltersData": filter_data,
            "enableInChats": True,
            "excludedDialogs": excluded,
            "OEChatsRegexFiltersShowHidden": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "regex alias import")
    if data is not None:
        ledger.check(
            "regex alias applied",
            data.get("applied") == 5 and data.get("unknown") == [],
            data)
    data = json_call(
        ledger, cli, ["regex-filters"], env, "regex alias state")
    if data is not None:
        ledger.check(
            "regex alias enabled and count",
            data.get("enabled") is True and data.get("count") == 1,
            data)
    result = json_call(
        ledger, cli, ["regex-test", "spam"], env, "regex show hidden")
    if result is not None:
        ledger.check(
            "regex show hidden suppresses",
            result.get("hidden") is False,
            result)
    current = json_call(
        ledger, cli, ["backup-export-current"], env, "regex alias export")
    if current is not None:
        section = backup_sections(current)["exteraconfig"]
        ledger.check(
            "regex backup keys",
            section.get("OEChatsRegexFiltersEnabled") is True
            and section.get("OEChatsRegexFiltersData") == filter_data
            and section.get("OEChatsRegexFiltersEnableInChats") is True
            and section.get("OEChatsRegexFiltersShowHidden") is True
            and section.get("OEChatsRegexFiltersExcludedDialogs")
                == excluded,
            section)

    root = {
        "version": 1,
        "exteraconfig": {
            "OEChatsRegexFiltersShowHidden": False,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "regex show hidden off")
    if data is not None:
        ledger.check(
            "regex show hidden off applied",
            data.get("applied") == 1,
            data)
    result = json_call(
        ledger, cli, ["regex-test", "spam"], env, "regex hide again")
    if result is not None:
        ledger.check(
            "regex hides after show hidden off",
            result.get("hidden") is True,
            result)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


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
                "OEGhostSendUploadProgress")),
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
            "backup double tap defaults",
            section.get("OEChatsDoubleTapIn") == -1
            and section.get("OEChatsDoubleTapOut") == -1,
            (section.get("OEChatsDoubleTapIn"),
             section.get("OEChatsDoubleTapOut")))
        ledger.check(
            "backup extera key count",
            len(section) == 138,
            sorted(section))
        ledger.check(
            "backup pill keys",
            exported["pillstackconfig"] == {
                "OEPillStackEnabled": False,
                "OEPillStackPills": 0x3FFF,
                "OEPillStackOrder":
                    "cpu,ram,net,dc,proxy,cache,ghost,lastseen,weather,btc,eth,usd,eur,gold",
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
            sum(len(value) for value in exported.values()) == 143,
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
                data.get("applied") == 143,
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
            "OEChatsDoubleTapIn": 0,
            "OEChatsDoubleTapOut": 4,
            "OEChatsStickerTimeSide": True,
            "OEChatsHideStickerTime": False,
            "OEChatsStickerSize": 14,
            "OEChatsStickerShape": 1,
            "OEChatsAvatarRadius": 10,
            "OEChatsSectionRadius": 8,
            "OEChatsSectionSeparateHeaders": True,
            "OEChatsChatHeaderStyle": 2,
            "OEChatsReplyColors": False,
            "OEChatsReplyEmoji": True,
            "OEChatsReplyBackground": False,
            "OEGhostShowStatus": True,
            "OEGhostSendReadLocked": True,
            "OEGhostSendStoriesLocked": False,
            "OEGhostSendOnlineLocked": False,
            "OEGhostSendUploadProgressLocked": False,
            "OEChatsTranslucentDeleted": False,
            "OEChatsUseDeletedIcon": False,
            "OEChatsCustomDeletedMark": "del",
            "OEChatsDeletedMarkColor": 16711680,
            "OEChatsPanguOnSending": True,
            "OEChatsSilentByDefault": True,
            "OESettingsHidePremium": True,
            "OESettingsHideHelp": True,
            "OEAppearanceHideAiEditor": True,
            "OEChatsHideSendAsPeer": True,
            "OEChatsHideGiftButton": True,
            "OEChatsHideCallButton": True,
            "OEChatsHideChannelSearchButton": True,
            "OEChatsInlineMathResult": True,
            "OEGeneralDefaultVideoQuality": 1,
            "OEGeneralHardwareAcceleration": False,
            "OEChatsTakeGifAsVideo": True,
            "OEGeneralCustomAudioBitrate": 24,
            "OEGeneralAutoUpdateChannel": 2,
            "OEGeneralSaveToChatSubfolder": True,
            "OEGhostScheduledSend": True,
            "OEChatsSeekDuration": 15,
            "OEChatsUseEditedIcon": True,
            "OEChatsCustomEditedMark": "изм.",
            "OEChatsShowForwardDate": True,
            "OEChatsSenderMiniAvatars": True,
            "OEMediaViewerMenuForward": False,
            "OEMediaViewerMenuCopyFrame": False,
            "OEMediaViewerMenuCopyPhoto": True,
            "OEMediaViewerMenuSetProfile": False,
            "OEChatsChatMenuBoost": False,
            "OEChatsChatMenuLinked": False,
            "OEChatsChatMenuViewDeleted": True,
            "OEChatsChatMenuClearDeleted": False,
            "OEChatsDisableLinkPreviewByDefault": True,
            "OEChatsHideAllChatsFolder": True,
            "OEChatsHideArchive": True,
            "OEChatsHideGreetingSticker": True,
            "OEChatsHidePanelsOnScroll": True,
            "OEChatsChannelBottomButton": 2,
            "OEChatsDownloadSpeedBoost": 2,
            "OEChatsUploadBoost": True,
            "OEChatsSortByUnread": True,
            "OEChatsWideChannelPosts": True,
            "OEChatsDisableAutoDownloadingArchive": True,
            "OEChatsBlockWin32ExecutableFiles": False,
            "OEChatsRegexFiltersEnabled": True,
            "OEChatsRegexFiltersData": "[\"spam\"]",
            "OEChatsRegexFiltersEnableInChats": True,
            "OEChatsRegexFiltersShowHidden": True,
            "OEChatsRegexFiltersExcludedDialogs": "[-100123]",
            "OEMediaViewerMenuSubtitles": False,
            "OESubtitlesFiles": {
                "doc_1": {"path": "/tmp/a.srt", "label": "en"},
            },
            "OELyricsFiles": {
                "doc_2": {"path": "/tmp/a.lrc", "label": "ru"},
            },
            "OELyricsAllowNetwork": True,
            "OEChatsAlwaysShowDownloadIcon": True,
            "OEPrivacyFalseBottom": True,
            "OEAppearanceChatDecoration": 1,
            "OEChatsShortcutAdministrators": True,
            "OEChatsShortcutRecentActions": True,
            "OEChatsShortcutStatistics": True,
            "OEChatsShortcutPermissions": True,
            "OEChatsShortcutMembers": True,
            "OEChatsGroupedMessageMenu": True,
            "OEChatsDefaultDeleteMenu": True,
            "OEChatsShowResultsBeforeVoting": True,
            "OEGeneralBotPrivacyEnabled": True,
            "OEGeneralBotPrivacyAccount": 2,
            "OEGeneralBotPrivacyAlwaysAsk": True,
            "hideStoriesFromHeader": True,
            "showIdAndDc": True,
            "zalgoFilter": True,
            "showSpoilersDirectly": True,
            "showFormattingButtons": True,
            "OEGeneralUsePersianCalendar": True,
            "OEGeneralPersianCalendarLatin": True,
            "OETranslationProvider": 2,
            "OETranslationDeepLFormality": 1,
            "OEAiEndpoint": "https://full.test/v1",
            "OEAiModel": "full-model",
            "OEAiRoles": json.dumps([
                {"name": "translator", "prompt": "Translate."},
            ]),
            "OEAiActiveRole": "translator",
            "OEAiHistory": False,
            "OEGeneralRecognitionLanguage": "de",
            "OENetworkUseIPv6": True,
            "OENetworkDnsType": 1,
            "OENetworkCustomDoH": "https://full.test/dns-query",
        },
        "pillstackconfig": {
            "OEPillStackEnabled": True,
            "OEPillStackPills": 5,
            "OEPillStackOrder": "ghost,cache,cpu,ram,net,dc,proxy,lastseen,weather,btc,eth,usd,eur,gold",
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
            data.get("applied") == 143,
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
            "backup full network",
            section.get("OENetworkUseIPv6") is True
            and section.get("OENetworkDnsType") == 1
            and section.get("OENetworkCustomDoH")
            == "https://full.test/dns-query",
            (section.get("OENetworkUseIPv6"),
             section.get("OENetworkDnsType"),
             section.get("OENetworkCustomDoH")))
        ledger.check(
            "backup full ghost flags",
            all(section.get(key) is False for key in (
                "OEGhostSendReadPackets",
                "OEGhostSendReadStories",
                "OEGhostSendOnline",
                "OEGhostSendUploadProgress")),
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
            "backup full lyrics files",
            section.get("OELyricsFiles") == {
                "doc_2": {"path": "/tmp/a.lrc", "label": "ru"},
            },
            section.get("OELyricsFiles"))
        ledger.check(
            "backup full lyrics network",
            section.get("OELyricsAllowNetwork") is True,
            section.get("OELyricsAllowNetwork"))
        ledger.check(
            "backup full save deleted",
            section.get("OESaveDeletedMessages") is True,
            section.get("OESaveDeletedMessages"))
        ledger.check(
            "backup full proxy auto reset",
            section.get("OEProxyAutoDisabled") is False,
            section.get("OEProxyAutoDisabled"))
        ledger.check(
            "backup full double tap",
            section.get("OEChatsDoubleTapIn") == 0
            and section.get("OEChatsDoubleTapOut") == 4,
            (section.get("OEChatsDoubleTapIn"),
             section.get("OEChatsDoubleTapOut")))
        ledger.check(
            "backup full sticker time",
            section.get("OEChatsStickerTimeSide") is True
            and section.get("OEChatsHideStickerTime") is False,
            (section.get("OEChatsStickerTimeSide"),
             section.get("OEChatsHideStickerTime")))
        ledger.check(
            "backup full ghost status",
            section.get("OEGhostShowStatus") is True,
            section.get("OEGhostShowStatus"))
        ledger.check(
            "backup full ai editor",
            section.get("OEAppearanceHideAiEditor") is True,
            section.get("OEAppearanceHideAiEditor"))
        ledger.check(
            "backup full hide send as",
            section.get("OEChatsHideSendAsPeer") is True,
            section.get("OEChatsHideSendAsPeer"))
        ledger.check(
            "backup full inline math",
            section.get("OEChatsInlineMathResult") is True,
            section.get("OEChatsInlineMathResult"))
        ledger.check(
            "backup full save subfolder",
            section.get("OEGeneralSaveToChatSubfolder") is True,
            section.get("OEGeneralSaveToChatSubfolder"))
        ledger.check(
            "backup full ghost schedule",
            section.get("OEGhostScheduledSend") is True,
            section.get("OEGhostScheduledSend"))
        ledger.check(
            "backup full seek duration",
            section.get("OEChatsSeekDuration") == 15,
            section.get("OEChatsSeekDuration"))
        ledger.check(
            "backup full hide all chats",
            section.get("OEChatsHideAllChatsFolder") is True,
            section.get("OEChatsHideAllChatsFolder"))
        ledger.check(
            "backup full hide archive",
            section.get("OEChatsHideArchive") is True,
            section.get("OEChatsHideArchive"))
        ledger.check(
            "backup full chat misc",
            section.get("OEChatsHideGreetingSticker") is True
            and section.get("OEChatsHidePanelsOnScroll") is True
            and section.get("OEChatsChannelBottomButton") == 2,
            (section.get("OEChatsHideGreetingSticker"),
             section.get("OEChatsHidePanelsOnScroll"),
             section.get("OEChatsChannelBottomButton")))
        ledger.check(
            "backup full sort by unread",
            section.get("OEChatsSortByUnread") is True,
            section.get("OEChatsSortByUnread"))
        ledger.check(
            "backup full regex filters",
            section.get("OEChatsRegexFiltersEnabled") is True
            and section.get("OEChatsRegexFiltersData") == "[\"spam\"]"
            and section.get("OEChatsRegexFiltersEnableInChats") is True
            and section.get("OEChatsRegexFiltersShowHidden") is True
            and section.get("OEChatsRegexFiltersExcludedDialogs")
                == "[-100123]",
            {key: section.get(key) for key in section
             if key.startswith("OEChatsRegexFilters")})
        ledger.check(
            "backup full always show download",
            section.get("OEChatsAlwaysShowDownloadIcon") is True,
            section.get("OEChatsAlwaysShowDownloadIcon"))
        ledger.check(
            "backup full false bottom",
            section.get("OEPrivacyFalseBottom") is True,
            section.get("OEPrivacyFalseBottom"))
        ledger.check(
            "backup full decoration",
            section.get("OEAppearanceChatDecoration") == 1,
            section.get("OEAppearanceChatDecoration"))
        ledger.check(
            "backup full shortcuts",
            section.get("OEChatsShortcutAdministrators") is True
            and section.get("OEChatsShortcutMembers") is True,
            {key: section.get(key) for key in section
             if key.startswith("OEChatsShortcut")})
        ledger.check(
            "backup full message menu style",
            section.get("OEChatsGroupedMessageMenu") is True
            and section.get("OEChatsDefaultDeleteMenu") is True,
            (section.get("OEChatsGroupedMessageMenu"),
             section.get("OEChatsDefaultDeleteMenu")))
        ledger.check(
            "backup full disable link preview",
            section.get("OEChatsDisableLinkPreviewByDefault") is True,
            section.get("OEChatsDisableLinkPreviewByDefault"))
        ledger.check(
            "backup full chat menu",
            section.get("OEChatsChatMenuBoost") is False
            and section.get("OEChatsChatMenuLinked") is False
            and section.get("OEChatsChatMenuViewDeleted") is True
            and section.get("OEChatsChatMenuClearDeleted") is False,
            (section.get("OEChatsChatMenuBoost"),
             section.get("OEChatsChatMenuLinked"),
             section.get("OEChatsChatMenuViewDeleted"),
             section.get("OEChatsChatMenuClearDeleted")))
        ledger.check(
            "backup full media menu",
            section.get("OEMediaViewerMenuForward") is False
            and section.get("OEMediaViewerMenuCopyFrame") is False
            and section.get("OEMediaViewerMenuCopyPhoto") is True
            and section.get("OEMediaViewerMenuSetProfile") is False,
            (section.get("OEMediaViewerMenuForward"),
             section.get("OEMediaViewerMenuCopyFrame"),
             section.get("OEMediaViewerMenuCopyPhoto"),
             section.get("OEMediaViewerMenuSetProfile")))
        ledger.check(
            "backup full forward date",
            section.get("OEChatsShowForwardDate") is True,
            section.get("OEChatsShowForwardDate"))
        ledger.check(
            "backup full edited mark",
            section.get("OEChatsUseEditedIcon") is True
            and section.get("OEChatsCustomEditedMark") == "изм.",
            (section.get("OEChatsUseEditedIcon"),
             section.get("OEChatsCustomEditedMark")))
        ledger.check(
            "backup full settings sections",
            section.get("OESettingsHidePremium") is True
            and section.get("OESettingsHideHelp") is True,
            (section.get("OESettingsHidePremium"),
             section.get("OESettingsHideHelp")))
        ledger.check(
            "backup full silent",
            section.get("OEChatsSilentByDefault") is True,
            section.get("OEChatsSilentByDefault"))
        ledger.check(
            "backup full pangu",
            section.get("OEChatsPanguOnSending") is True,
            section.get("OEChatsPanguOnSending"))
        ledger.check(
            "backup full deleted mark",
            section.get("OEChatsTranslucentDeleted") is False
            and section.get("OEChatsUseDeletedIcon") is False
            and section.get("OEChatsCustomDeletedMark") == "del"
            and section.get("OEChatsDeletedMarkColor") == 16711680,
            (section.get("OEChatsTranslucentDeleted"),
             section.get("OEChatsUseDeletedIcon"),
             section.get("OEChatsCustomDeletedMark"),
             section.get("OEChatsDeletedMarkColor")))
        ledger.check(
            "backup full ghost locks",
            section.get("OEGhostSendReadLocked") is True
            and section.get("OEGhostSendStoriesLocked") is False,
            (section.get("OEGhostSendReadLocked"),
             section.get("OEGhostSendStoriesLocked")))
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
            "backup full update channel",
            section.get("OEGeneralAutoUpdateChannel") == 2,
            section.get("OEGeneralAutoUpdateChannel"))
        ledger.check(
            "backup full pill keys",
            sections["pillstackconfig"] == {
                "OEPillStackEnabled": True,
                "OEPillStackPills": 5,
                "OEPillStackOrder":
                    "ghost,cache,cpu,ram,net,dc,proxy,lastseen,weather,btc,eth,usd,eur,gold",
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
            "OEPillStackPills": 0x4000,
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
            "backup legacy eight completed to 138",
            (len(sections["exteraconfig"]),
             len(sections["pillstackconfig"]),
             len(sections["mainconfig"])) == (138, 3, 2),
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
            ("too big", 8, 0x4000)]):
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


def check_doubletap(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "doubletap-defaults")
    data = json_call(ledger, cli, ["doubletap"], env, "doubletap defaults")
    if data is not None:
        ledger.check(
            "doubletap defaults telegram",
            data.get("in") == -1 and data.get("out") == -1,
            data)

    env = make_env(work / "doubletap-set")
    data = json_call(
        ledger, cli, ["doubletap-set", "1", "4"], env, "doubletap set")
    if data is not None:
        ledger.check(
            "doubletap set values",
            data.get("in") == 1 and data.get("out") == 4,
            data)
    reloaded = json_call(
        ledger, cli, ["doubletap"], env, "doubletap reload")
    if reloaded is not None:
        ledger.check(
            "doubletap persisted",
            reloaded.get("in") == 1 and reloaded.get("out") == 4,
            reloaded)

    data = json_call(
        ledger, cli, ["doubletap-set", "99", "-5"], env,
        "doubletap set out of range")
    if data is not None:
        ledger.check(
            "doubletap out of range falls back to telegram",
            data.get("in") == -1 and data.get("out") == -1,
            data)

    resolve_cases = [
        ("telegram", "-1", "telegram"),
        ("none", "0", "none"),
        ("reaction", "1", "reaction"),
        ("show reactions maps to reaction", "2", "reaction"),
        ("translate", "3", "translate"),
        ("reply", "4", "reply"),
        ("save", "5", "save"),
        ("repeat stays inert", "6", "none"),
        ("repeat-as-copy stays inert", "7", "none"),
        ("edit", "8", "edit"),
        ("unnamed action 9 stays inert", "9", "none"),
        ("delete", "10", "delete"),
        ("forward", "11", "forward"),
        ("download", "12", "download"),
    ]
    for name, value, expected in resolve_cases:
        result = run(cli, ["doubletap-resolve", value])
        actual = result.stdout.strip()
        ledger.check(
            f"doubletap resolve {name}",
            result.returncode == 0 and actual == expected,
            f"exit {result.returncode}, got {actual!r}, want {expected!r}")

    env = make_env(work / "doubletap-fromjson")
    root = {
        "version": 1,
        "exteraconfig": {
            "OEChatsDoubleTapIn": 0,
            "OEChatsDoubleTapOut": 1,
        },
    }
    data = json_call(
        ledger, cli, ["config-fromjson", json.dumps(root)], env,
        "doubletap fromJson")
    if data is not None:
        ledger.check(
            "doubletap fromJson accepted",
            data.get("accepted") is True,
            data)
    reloaded = json_call(
        ledger, cli, ["doubletap"], env, "doubletap fromJson read")
    if reloaded is not None:
        ledger.check(
            "doubletap fromJson values",
            reloaded.get("in") == 0 and reloaded.get("out") == 1,
            reloaded)

    env = make_env(work / "doubletap-backup-aliases")
    root = {
        "version": 1,
        "exteraconfig": {
            "doubleTapAction": 4,
            "doubleTapActionOutOwner": 0,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "doubletap backup aliases")
    if data is not None:
        ledger.check(
            "doubletap backup aliases applied",
            data.get("applied") == 2,
            data.get("applied"))
        ledger.check(
            "doubletap backup aliases unknown empty",
            data.get("unknown") == [],
            data.get("unknown"))
    reloaded = json_call(
        ledger, cli, ["doubletap"], env, "doubletap aliases read")
    if reloaded is not None:
        ledger.check(
            "doubletap aliases applied to config",
            reloaded.get("in") == 4 and reloaded.get("out") == 0,
            reloaded)

    env = make_env(work / "doubletap-backup-invalid")
    root = {
        "version": 1,
        "exteraconfig": {
            "doubleTapAction": 42,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "doubletap backup invalid")
    if data is not None:
        ledger.check(
            "doubletap backup invalid rejected",
            data.get("ok") is True
            and data.get("applied") == 0
            and data.get("unknown") == ["doubleTapAction"],
            data)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_sticker_time(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "stickertime-defaults")
    data = json_call(ledger, cli, ["stickertime"], env, "stickertime defaults")
    if data is not None:
        ledger.check(
            "stickertime defaults",
            data.get("side") is False and data.get("hidden") is False,
            data)

    env = make_env(work / "stickertime-side")
    data = json_call(
        ledger, cli, ["stickertime-set", "1", "0"], env, "stickertime side")
    if data is not None:
        ledger.check(
            "stickertime side set",
            data.get("side") is True and data.get("hidden") is False,
            data)
    reloaded = json_call(
        ledger, cli, ["stickertime"], env, "stickertime side reload")
    if reloaded is not None:
        ledger.check(
            "stickertime side persisted",
            reloaded.get("side") is True and reloaded.get("hidden") is False,
            reloaded)

    env = make_env(work / "stickertime-hidden")
    data = json_call(
        ledger, cli, ["stickertime-set", "0", "1"], env,
        "stickertime hidden")
    if data is not None:
        ledger.check(
            "stickertime hidden set",
            data.get("side") is False and data.get("hidden") is True,
            data)

    env = make_env(work / "stickertime-fromjson")
    root = {
        "version": 1,
        "exteraconfig": {
            "OEChatsStickerTimeSide": True,
            "OEChatsHideStickerTime": False,
            "OEGhostShowStatus": True,
            "OEGhostSendReadLocked": True,
            "OEGhostSendStoriesLocked": False,
            "OEGhostSendOnlineLocked": False,
            "OEGhostSendUploadProgressLocked": False,
            "OEChatsTranslucentDeleted": False,
            "OEChatsUseDeletedIcon": False,
            "OEChatsCustomDeletedMark": "del",
            "OEChatsDeletedMarkColor": 16711680,
            "OEChatsPanguOnSending": True,
            "OEChatsSilentByDefault": True,
            "OESettingsHidePremium": True,
            "OESettingsHideHelp": True,
            "OEAppearanceHideAiEditor": True,
            "OEChatsHideSendAsPeer": True,
            "OEChatsInlineMathResult": True,
            "OEGeneralSaveToChatSubfolder": True,
            "OEGhostScheduledSend": True,
            "OEChatsSeekDuration": 15,
            "OEChatsUseEditedIcon": True,
            "OEChatsCustomEditedMark": "изм.",
            "OEChatsShowForwardDate": True,
            "OEMediaViewerMenuForward": False,
            "OEMediaViewerMenuCopyFrame": False,
            "OEMediaViewerMenuCopyPhoto": True,
            "OEMediaViewerMenuSetProfile": False,
            "OEChatsChatMenuBoost": False,
            "OEChatsChatMenuLinked": False,
            "OEChatsChatMenuViewDeleted": True,
            "OEChatsChatMenuClearDeleted": False,
            "OEChatsDisableLinkPreviewByDefault": True,
            "OEChatsHideAllChatsFolder": True,
            "OEChatsAlwaysShowDownloadIcon": True,
            "OEPrivacyFalseBottom": True,
            "OEAppearanceChatDecoration": 1,
            "OEChatsShortcutAdministrators": True,
            "OEChatsShortcutRecentActions": True,
            "OEChatsShortcutStatistics": True,
            "OEChatsShortcutPermissions": True,
            "OEChatsShortcutMembers": True,
            "OEChatsGroupedMessageMenu": True,
            "OEChatsDefaultDeleteMenu": True,
        },
    }
    data = json_call(
        ledger, cli, ["config-fromjson", json.dumps(root)], env,
        "stickertime fromJson")
    if data is not None:
        ledger.check(
            "stickertime fromJson accepted",
            data.get("accepted") is True,
            data)
    reloaded = json_call(
        ledger, cli, ["stickertime"], env, "stickertime fromJson read")
    if reloaded is not None:
        ledger.check(
            "stickertime fromJson values",
            reloaded.get("side") is True and reloaded.get("hidden") is False,
            reloaded)

    env = make_env(work / "stickertime-alias")
    root = {
        "version": 1,
        "exteraconfig": {"hideStickerTime": True},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "stickertime alias")
    if data is not None:
        ledger.check(
            "stickertime alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["stickertime"], env, "stickertime alias read")
    if reloaded is not None:
        ledger.check(
            "stickertime alias value",
            reloaded.get("hidden") is True,
            reloaded)

    mode_cases = [
        ("0", False, False),
        ("1", True, False),
        ("2", False, True),
    ]
    for mode, side, hidden in mode_cases:
        env = make_env(work / f"stickertime-mode-{mode}")
        root = {
            "version": 1,
            "exteraconfig": {"stickerTimeMode": int(mode)},
        }
        data = json_call(
            ledger, cli, ["backup-import-json", json.dumps(root)], env,
            f"stickertime mode {mode}")
        if data is not None:
            ledger.check(
                f"stickertime mode {mode} applied",
                data.get("applied") == 1 and data.get("unknown") == [],
                data)
        reloaded = json_call(
            ledger, cli, ["stickertime"], env,
            f"stickertime mode {mode} read")
        if reloaded is not None:
            ledger.check(
                f"stickertime mode {mode} values",
                reloaded.get("side") is side
                and reloaded.get("hidden") is hidden,
                reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_pill_order(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    default = ["cpu", "ram", "net", "dc", "proxy", "cache", "ghost",
               "lastseen", "weather", "btc", "eth", "usd", "eur", "gold"]

    env = make_env(work / "pillorder-defaults")
    data = json_call(ledger, cli, ["pillorder"], env, "pillorder defaults")
    if data is not None:
        ledger.check(
            "pillorder defaults",
            data.get("order") == default,
            data)

    env = make_env(work / "pillorder-custom")
    data = json_call(
        ledger, cli,
        ["pillorder-set", "ghost,cpu,ram,net,dc,proxy,cache,lastseen"],
        env, "pillorder custom")
    custom = ["ghost", "cpu", "ram", "net", "dc", "proxy", "cache",
              "lastseen", "weather", "btc", "eth", "usd", "eur", "gold"]
    if data is not None:
        ledger.check(
            "pillorder custom set",
            data.get("order") == custom,
            data)
    reloaded = json_call(
        ledger, cli, ["pillorder"], env, "pillorder custom reload")
    if reloaded is not None:
        ledger.check(
            "pillorder custom persisted",
            reloaded.get("order") == custom,
            reloaded)

    env = make_env(work / "pillorder-reset")
    json_call(
        ledger, cli,
        ["pillorder-set", "ghost,cpu,ram,net,dc,proxy,cache,lastseen"],
        env, "pillorder reset setup")
    data = json_call(
        ledger, cli, ["pillorder-reset"], env, "pillorder reset")
    if data is not None:
        ledger.check(
            "pillorder reset default",
            data.get("order") == default,
            data)
    reloaded = json_call(
        ledger, cli, ["pillorder"], env, "pillorder reset reload")
    if reloaded is not None:
        ledger.check(
            "pillorder reset persisted",
            reloaded.get("order") == default,
            reloaded)

    env = make_env(work / "pillorder-sanitize")
    data = json_call(
        ledger, cli, ["pillorder-set", "GHOST,cpu,ghost,bogus"], env,
        "pillorder sanitize")
    sanitized = ["ghost", "cpu", "ram", "net", "dc", "proxy", "cache",
                 "lastseen", "weather", "btc", "eth", "usd", "eur", "gold"]
    if data is not None:
        ledger.check(
            "pillorder sanitized",
            data.get("order") == sanitized,
            data)

    env = make_env(work / "pillorder-fromjson")
    root = {
        "version": 1,
        "exteraconfig": {"OEPillStackOrder": "cache,ghost"},
    }
    data = json_call(
        ledger, cli, ["config-fromjson", json.dumps(root)], env,
        "pillorder fromJson")
    if data is not None:
        ledger.check(
            "pillorder fromJson accepted",
            data.get("accepted") is True,
            data)
    reloaded = json_call(
        ledger, cli, ["pillorder"], env, "pillorder fromJson read")
    if reloaded is not None:
        ledger.check(
            "pillorder fromJson values",
            reloaded.get("order")
            == ["cache", "ghost", "cpu", "ram", "net", "dc", "proxy",
                "lastseen", "weather", "btc", "eth", "usd", "eur",
                "gold"],
            reloaded)

    env = make_env(work / "pillorder-backup")
    data = json_call(
        ledger, cli, ["backup-export", "1", "0", "0", "0", "1", "0", "0"],
        env, "pillorder backup")
    if data is not None:
        pills = data.get("pillstackconfig") or {}
        ledger.check(
            "pillorder backup default",
            pills.get("OEPillStackOrder")
            == "cpu,ram,net,dc,proxy,cache,ghost,lastseen,weather,btc,eth,usd,eur,gold",
            pills)
    root = {
        "version": 1,
        "pillstackconfig": {"OEPillStackOrder": "lastseen,ghost"},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "pillorder backup import")
    if data is not None:
        ledger.check(
            "pillorder backup import applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["pillorder"], env, "pillorder backup read")
    if reloaded is not None:
        ledger.check(
            "pillorder backup import values",
            reloaded.get("order")
            == ["lastseen", "ghost", "cpu", "ram", "net", "dc", "proxy",
                "cache", "weather", "btc", "eth", "usd", "eur", "gold"],
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_ghost(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "ghost-defaults")
    data = json_call(ledger, cli, ["ghost"], env, "ghost defaults")
    if data is not None:
        ledger.check(
            "ghost defaults",
            data.get("read") is True
            and data.get("stories") is True
            and data.get("online") is True
            and data.get("typing") is True
            and data.get("active") is False
            and data.get("partial") is False
            and data.get("count") == 0
            and data.get("showStatus") is False,
            data)

    env = make_env(work / "ghost-on")
    data = json_call(ledger, cli, ["ghost-set", "1"], env, "ghost on")
    if data is not None:
        ledger.check(
            "ghost batch on",
            data.get("read") is False
            and data.get("stories") is False
            and data.get("online") is False
            and data.get("typing") is False
            and data.get("active") is True
            and data.get("partial") is False
            and data.get("count") == 4,
            data)
    reloaded = json_call(ledger, cli, ["ghost"], env, "ghost on reload")
    if reloaded is not None:
        ledger.check(
            "ghost batch persisted",
            reloaded.get("active") is True and reloaded.get("count") == 4,
            reloaded)

    data = json_call(ledger, cli, ["ghost-set", "0"], env, "ghost off")
    if data is not None:
        ledger.check(
            "ghost batch off",
            data.get("active") is False
            and data.get("partial") is False
            and data.get("count") == 0,
            data)

    env = make_env(work / "ghost-partial")
    data = json_call(
        ledger, cli, ["ghost-flags", "1", "0", "1", "0"], env,
        "ghost partial")
    if data is not None:
        ledger.check(
            "ghost partial flags",
            data.get("read") is False
            and data.get("stories") is True
            and data.get("online") is False
            and data.get("typing") is True,
            data)
        ledger.check(
            "ghost partial status",
            data.get("active") is False
            and data.get("partial") is True
            and data.get("count") == 2,
            data)

    env = make_env(work / "ghost-lock")
    data = json_call(
        ledger, cli, ["ghost-lock", "1", "0", "0", "0"], env,
        "ghost lock read")
    if data is not None:
        ledger.check(
            "ghost lock read set",
            data.get("readLocked") is True
            and data.get("unlocked") == 3,
            data)
    data = json_call(
        ledger, cli, ["ghost-set", "1"], env, "ghost locked batch on")
    if data is not None:
        ledger.check(
            "ghost locked batch keeps read",
            data.get("read") is True
            and data.get("stories") is False
            and data.get("online") is False
            and data.get("typing") is False,
            data)
        ledger.check(
            "ghost locked batch status",
            data.get("active") is True
            and data.get("partial") is False
            and data.get("count") == 3,
            data)
    data = json_call(
        ledger, cli, ["ghost-lock", "0", "0", "0", "0"], env,
        "ghost unlock read")
    data = json_call(
        ledger, cli, ["ghost-set", "0"], env, "ghost locked batch off")
    if data is not None:
        ledger.check(
            "ghost unlocked batch off",
            data.get("read") is True
            and data.get("stories") is True
            and data.get("online") is True
            and data.get("typing") is True
            and data.get("active") is False,
            data)

    env = make_env(work / "ghost-locks-fromjson")
    root = {
        "version": 1,
        "exteraconfig": {
            "OEGhostSendReadLocked": True,
            "OEGhostSendOnlineLocked": True,
        },
    }
    data = json_call(
        ledger, cli, ["config-fromjson", json.dumps(root)], env,
        "ghost locks fromJson")
    if data is not None:
        ledger.check(
            "ghost locks fromJson accepted",
            data.get("accepted") is True,
            data)
    reloaded = json_call(
        ledger, cli, ["ghost"], env, "ghost locks fromJson read")
    if reloaded is not None:
        ledger.check(
            "ghost locks fromJson values",
            reloaded.get("readLocked") is True
            and reloaded.get("onlineLocked") is True
            and reloaded.get("unlocked") == 2,
            reloaded)

    env = make_env(work / "ghost-show")
    data = json_call(
        ledger, cli, ["ghost-show", "1"], env, "ghost show status")
    if data is not None:
        ledger.check(
            "ghost show status set",
            data.get("showStatus") is True,
            data)
    reloaded = json_call(
        ledger, cli, ["ghost"], env, "ghost show status reload")
    if reloaded is not None:
        ledger.check(
            "ghost show status persisted",
            reloaded.get("showStatus") is True,
            reloaded)

    env = make_env(work / "ghost-fromjson")
    root = {
        "version": 1,
        "exteraconfig": {
            "OEGhostShowStatus": True,
            "OEGhostSendReadPackets": False,
            "OEGhostSendReadStories": False,
            "OEGhostSendOnline": False,
            "OEGhostSendUploadProgress": False,
        },
    }
    data = json_call(
        ledger, cli, ["config-fromjson", json.dumps(root)], env,
        "ghost fromJson")
    if data is not None:
        ledger.check(
            "ghost fromJson accepted",
            data.get("accepted") is True,
            data)
    reloaded = json_call(
        ledger, cli, ["ghost"], env, "ghost fromJson read")
    if reloaded is not None:
        ledger.check(
            "ghost fromJson values",
            reloaded.get("active") is True
            and reloaded.get("showStatus") is True,
            reloaded)

    env = make_env(work / "ghost-backup")
    data = json_call(
        ledger, cli, ["backup-export", "1", "0", "0", "0", "1", "0", "0"],
        env, "ghost backup export")
    if data is not None:
        section = data.get("exteraconfig") or {}
        ledger.check(
            "ghost backup show status",
            section.get("OEGhostShowStatus") is False,
            section.get("OEGhostShowStatus"))
    root = {
        "version": 1,
        "exteraconfig": {
            "showGhostModeStatus": True,
            "sendReadMessagePacketsLocked": True,
            "sendOnlinePacketsLocked": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "ghost backup alias")
    if data is not None:
        ledger.check(
            "ghost backup alias applied",
            data.get("applied") == 3 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["ghost"], env, "ghost backup alias read")
    if reloaded is not None:
        ledger.check(
            "ghost backup alias value",
            reloaded.get("showStatus") is True
            and reloaded.get("readLocked") is True
            and reloaded.get("onlineLocked") is True
            and reloaded.get("unlocked") == 2,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_deleted_entry(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "deleted-entry")
    entry = {
        "peerId": "-1001234567890",
        "peerName": "Chat",
        "msgId": "77",
        "fromId": "42",
        "fromName": "Alice",
        "date": 1000,
        "editedDate": 1200,
        "text": "Hello world",
        "media": "",
        "out": True,
        "read": False,
        "replyToMsgId": "76",
        "replyToText": "quoted message",
        "forwardedFrom": "Channel",
        "forwardedDate": 900,
        "reactions": ["👍", "custom:12345"],
        "entities": [
            {"type": 4, "offset": 0, "length": 5, "data": ""},
            {"type": 99, "offset": 0, "length": 5, "data": ""},
            {"type": 4, "offset": 6, "length": 50, "data": ""},
        ],
    }
    data = json_call(
        ledger, cli, ["deleted-entry", json.dumps(entry)], env,
        "deleted entry")
    if data is not None:
        ledger.check(
            "deleted entry valid",
            data.get("valid") is True,
            data)
        out = data.get("entry") or {}
        ledger.check(
            "deleted entry schema",
            out.get("peerId") == "-1001234567890"
            and out.get("msgId") == "77"
            and out.get("text") == "Hello world"
            and out.get("out") is True
            and out.get("read") is False,
            out)
        ledger.check(
            "deleted entry extras",
            out.get("editedDate") == 1200
            and out.get("replyToMsgId") == "76"
            and out.get("replyToText") == "quoted message"
            and out.get("forwardedFrom") == "Channel"
            and out.get("forwardedDate") == 900,
            out)
        ledger.check(
            "deleted entry reactions",
            out.get("reactions") == ["👍", "custom:12345"],
            out.get("reactions"))
        ledger.check(
            "deleted entry entities filtered",
            out.get("entities") == [
                {"type": 4, "offset": 0, "length": 5, "data": ""},
            ],
            out.get("entities"))

    legacy = {
        "peerId": "1",
        "msgId": "2",
        "text": "old entry",
        "date": 10,
    }
    data = json_call(
        ledger, cli, ["deleted-entry", json.dumps(legacy)], env,
        "deleted entry legacy")
    if data is not None:
        out = data.get("entry") or {}
        ledger.check(
            "deleted entry legacy defaults",
            data.get("valid") is True
            and out.get("read") is True
            and out.get("editedDate") == 0
            and out.get("reactions") == []
            and out.get("entities") == [],
            out)

    data = json_call(
        ledger, cli, ["deleted-entry", json.dumps({"text": "no ids"})],
        env, "deleted entry no ids")
    if data is not None:
        ledger.check(
            "deleted entry requires ids",
            data.get("valid") is False,
            data)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_deleted_mark(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "deletedmark-defaults")
    data = json_call(
        ledger, cli, ["deletedmark"], env, "deletedmark defaults")
    if data is not None:
        ledger.check(
            "deletedmark defaults",
            data.get("translucent") is True
            and data.get("icon") is True
            and data.get("mark") == ""
            and data.get("color") == 0,
            data)

    env = make_env(work / "deletedmark-set")
    data = json_call(
        ledger, cli, ["deletedmark-set", "0", "0", "del", "16711680"],
        env, "deletedmark set")
    if data is not None:
        ledger.check(
            "deletedmark set values",
            data.get("translucent") is False
            and data.get("icon") is False
            and data.get("mark") == "del"
            and data.get("color") == 16711680,
            data)
    reloaded = json_call(
        ledger, cli, ["deletedmark"], env, "deletedmark reload")
    if reloaded is not None:
        ledger.check(
            "deletedmark persisted",
            reloaded.get("mark") == "del"
            and reloaded.get("color") == 16711680,
            reloaded)

    long_mark = "x" * 40
    data = json_call(
        ledger, cli,
        ["deletedmark-set", "1", "1", long_mark, "-5"],
        env, "deletedmark limits")
    if data is not None:
        ledger.check(
            "deletedmark mark truncated",
            data.get("mark") == "x" * 32,
            data.get("mark"))
        ledger.check(
            "deletedmark color sanitized",
            data.get("color") == 0,
            data.get("color"))

    env = make_env(work / "deletedmark-fromjson")
    root = {
        "version": 1,
        "exteraconfig": {
            "OEChatsTranslucentDeleted": False,
            "OEChatsUseDeletedIcon": False,
            "OEChatsCustomDeletedMark": "gone",
            "OEChatsDeletedMarkColor": 255,
        },
    }
    data = json_call(
        ledger, cli, ["config-fromjson", json.dumps(root)], env,
        "deletedmark fromJson")
    if data is not None:
        ledger.check(
            "deletedmark fromJson accepted",
            data.get("accepted") is True,
            data)
    reloaded = json_call(
        ledger, cli, ["deletedmark"], env, "deletedmark fromJson read")
    if reloaded is not None:
        ledger.check(
            "deletedmark fromJson values",
            reloaded.get("translucent") is False
            and reloaded.get("icon") is False
            and reloaded.get("mark") == "gone"
            and reloaded.get("color") == 255,
            reloaded)

    env = make_env(work / "deletedmark-backup")
    data = json_call(
        ledger, cli, ["backup-export", "1", "0", "0", "0", "1", "0", "0"],
        env, "deletedmark backup")
    if data is not None:
        section = data.get("exteraconfig") or {}
        ledger.check(
            "deletedmark backup keys",
            section.get("OEChatsTranslucentDeleted") is True
            and section.get("OEChatsUseDeletedIcon") is True
            and section.get("OEChatsCustomDeletedMark") == ""
            and section.get("OEChatsDeletedMarkColor") == 0,
            section)
    root = {
        "version": 1,
        "exteraconfig": {
            "TranslucentDeletedMessages": False,
            "UseDeletedIcon": False,
            "CustomDeletedMark": "legacy",
            "DeletedMarkColor": 305419896,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "deletedmark aliases")
    if data is not None:
        ledger.check(
            "deletedmark aliases applied",
            data.get("applied") == 4 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["deletedmark"], env, "deletedmark aliases read")
    if reloaded is not None:
        ledger.check(
            "deletedmark aliases values",
            reloaded.get("translucent") is False
            and reloaded.get("icon") is False
            and reloaded.get("mark") == "legacy"
            and reloaded.get("color") == 305419896,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_vpn(ledger, cli):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    cases = [
        ("tun0", True),
        ("tap0", True),
        ("wg0", True),
        ("utun3", True),
        ("TUN0", True),
        ("ppp0", False),
        ("ppp1", False),
        ("enp3s0", False),
        ("wlan0", False),
        ("", False),
    ]
    for name, expected in cases:
        result = run(cli, ["vpn-name", name])
        actual = result.stdout.strip()
        want = "1" if expected else "0"
        ledger.check(
            f"vpn name {name or 'empty'}",
            result.returncode == 0 and actual == want,
            f"exit {result.returncode}, got {actual!r}, want {want!r}")

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_proxy_rules(ledger, cli):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    cases = [
        ("disable on transition", ["0", "0", "1", "1"],
         {"conditionActive": True, "autoDisabled": True,
          "disable": True, "enable": False}),
        ("manual enable wins", ["1", "1", "1", "1"],
         {"conditionActive": True, "autoDisabled": False,
          "disable": False, "enable": False}),
        ("restore when condition clears", ["1", "1", "0", "0"],
         {"conditionActive": False, "autoDisabled": False,
          "disable": False, "enable": True}),
        ("no auto enable on fresh start", ["0", "0", "0", "0"],
         {"conditionActive": False, "autoDisabled": False,
          "disable": False, "enable": False}),
        ("active condition no repeat", ["1", "0", "1", "1"],
         {"conditionActive": True, "autoDisabled": False,
          "disable": False, "enable": False}),
        ("disabled proxy no auto flag", ["0", "0", "1", "0"],
         {"conditionActive": True, "autoDisabled": False,
          "disable": False, "enable": False}),
    ]
    for name, arguments, expected in cases:
        data = json_call(
            ledger, cli, ["proxy-rule", *arguments], None,
            f"proxy rule {name}")
        if data is not None:
            ledger.check(
                f"proxy rule {name}",
                all(data.get(key) == value for key, value in expected.items()),
                data)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_pangu(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "pangu-defaults")
    data = json_call(ledger, cli, ["pangu"], env, "pangu defaults")
    if data is not None:
        ledger.check(
            "pangu default off",
            data.get("enabled") is False,
            data)

    cases = [
        ("中文test", "中文 test"),
        ("test中文", "test 中文"),
        ("中文test中文", "中文 test 中文"),
        ("中文 test", "中文 test"),
        ("123中文", "123 中文"),
        ("中文123", "中文 123"),
        ("日本語テスト", "日本語テスト"),
        ("한국어test", "한국어 test"),
        ("abc def", "abc def"),
    ]
    for text, expected in cases:
        result = run(cli, ["pangu-apply", text])
        actual = result.stdout.rstrip("\n")
        ledger.check(
            f"pangu apply {text}",
            result.returncode == 0 and actual == expected,
            f"exit {result.returncode}, got {actual!r}, want {expected!r}")

    data = json_call(
        ledger, cli, ["pangu-tags", "中文test", "2", "4"], env,
        "pangu tags")
    if data is not None:
        ledger.check(
            "pangu tags shifted",
            data.get("text") == "中文 test"
            and data.get("offset") == 3
            and data.get("length") == 4,
            data)

    env = make_env(work / "pangu-set")
    data = json_call(
        ledger, cli, ["pangu-set", "1"], env, "pangu set")
    if data is not None:
        ledger.check(
            "pangu set on",
            data == 1,
            data)
    reloaded = json_call(ledger, cli, ["pangu"], env, "pangu reload")
    if reloaded is not None:
        ledger.check(
            "pangu persisted",
            reloaded.get("enabled") is True,
            reloaded)

    env = make_env(work / "pangu-fromjson")
    root = {
        "version": 1,
        "exteraconfig": {"EnablePanguOnSending": True},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "pangu alias")
    if data is not None:
        ledger.check(
            "pangu alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(ledger, cli, ["pangu"], env, "pangu alias read")
    if reloaded is not None:
        ledger.check(
            "pangu alias value",
            reloaded.get("enabled") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_zalgo(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "zalgo-defaults")
    data = json_call(ledger, cli, ["zalgo"], env, "zalgo defaults")
    if data is not None:
        ledger.check(
            "zalgo default off",
            data.get("zalgoFilter") is False,
            data)

    classic = "a\u0301\u0302\u0303\u0304b"
    arabic = "\u0628\u0650\u0633\u0652\u0645\u0650 "
    arabic += "\u0627\u0644\u0644\u0651\u064e\u0647\u0650"
    cases = [
        (classic, "ab"),
        ("cafe\u0301", "cafe\u0301"),
        (arabic, arabic),
        ("\U0001f468\u200d\U0001f469\u200d\U0001f467",
         "\U0001f468\u200d\U0001f469\u200d\U0001f467"),
        ("plain text", "plain text"),
    ]
    for text, expected in cases:
        result = run(cli, ["zalgo-apply", text])
        actual = result.stdout.rstrip("\n")
        ledger.check(
            f"zalgo apply {text}",
            result.returncode == 0 and actual == expected,
            f"exit {result.returncode}, got {actual!r}, want {expected!r}")

    padding = "x" * (2048 - len(classic))
    inside = classic + padding
    outside = inside + "x"
    for text, expected in ((inside, "ab" + padding), (outside, outside)):
        result = run(cli, ["zalgo-apply", text])
        actual = result.stdout.rstrip("\n")
        ledger.check(
            f"zalgo apply length {len(text)}",
            result.returncode == 0 and actual == expected,
            f"exit {result.returncode}, got {len(actual)} chars, "
            f"want {len(expected)}")

    data = json_call(
        ledger, cli, ["zalgo-tags", classic, "1", "4"], env, "zalgo tags")
    if data is not None:
        ledger.check(
            "zalgo tags dropped marks",
            data.get("text") == "ab"
            and data.get("offset") == 1
            and data.get("length") == 0,
            data)
    data = json_call(
        ledger, cli, ["zalgo-tags", classic, "5", "1"], env,
        "zalgo tags tail")
    if data is not None:
        ledger.check(
            "zalgo tags shifted tail",
            data.get("text") == "ab"
            and data.get("offset") == 1
            and data.get("length") == 1,
            data)

    env = make_env(work / "zalgo-set")
    data = json_call(
        ledger, cli, ["zalgo-set", "1"], env, "zalgo set")
    if data is not None:
        ledger.check(
            "zalgo set on",
            data == 1,
            data)
    reloaded = json_call(ledger, cli, ["zalgo"], env, "zalgo reload")
    if reloaded is not None:
        ledger.check(
            "zalgo persisted",
            reloaded.get("zalgoFilter") is True,
            reloaded)

    env = make_env(work / "zalgo-fromjson")
    root = {
        "version": 1,
        "exteraconfig": {"zalgoFilter": True},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "zalgo alias")
    if data is not None:
        ledger.check(
            "zalgo alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(ledger, cli, ["zalgo"], env, "zalgo alias read")
    if reloaded is not None:
        ledger.check(
            "zalgo alias value",
            reloaded.get("zalgoFilter") is True,
            reloaded)

    for name, arguments in (
            ("zalgo-set with wrong count", ["zalgo-set"]),
            ("zalgo-apply without text", ["zalgo-apply"]),
            ("zalgo-tags with wrong count", ["zalgo-tags", "a", "0"])):
        result = run(cli, arguments)
        ledger.check(
            f"cli {name}",
            result.returncode == 2 and bool(result.stderr.strip()),
            f"exit {result.returncode}, stderr {result.stderr!r}")

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_spoilers_direct(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "spoilers-direct-defaults")
    data = json_call(
        ledger, cli, ["spoilers-direct"], env, "spoilers-direct defaults")
    if data is not None:
        ledger.check(
            "spoilers direct default off",
            data.get("direct") is False,
            data)

    data = json_call(
        ledger, cli, ["spoilers-direct-set", "1"], env,
        "spoilers-direct set")
    if data is not None:
        ledger.check("spoilers direct set on", data == 1, data)
    reloaded = json_call(
        ledger, cli, ["spoilers-direct"], env, "spoilers-direct reload")
    if reloaded is not None:
        ledger.check(
            "spoilers direct persisted",
            reloaded.get("direct") is True,
            reloaded)

    env = make_env(work / "spoilers-direct-fromjson")
    root = {
        "version": 1,
        "exteraconfig": {"showSpoilersDirectly": True},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "spoilers-direct alias")
    if data is not None:
        ledger.check(
            "spoilers direct alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["spoilers-direct"], env, "spoilers-direct alias read")
    if reloaded is not None:
        ledger.check(
            "spoilers direct alias value",
            reloaded.get("direct") is True,
            reloaded)

    for name, arguments in (
            ("spoilers-direct-set with wrong count",
             ["spoilers-direct-set"]),
            ("spoilers-direct-set invalid value",
             ["spoilers-direct-set", "2"])):
        result = run(cli, arguments)
        ledger.check(
            f"cli {name}",
            result.returncode == 2 and bool(result.stderr.strip()),
            f"exit {result.returncode}, stderr {result.stderr!r}")

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_formatting_buttons(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "formatting-buttons-defaults")
    data = json_call(
        ledger, cli, ["formatting-buttons"], env,
        "formatting-buttons defaults")
    if data is not None:
        ledger.check(
            "formatting buttons default off",
            data.get("enabled") is False,
            data)

    data = json_call(
        ledger, cli, ["formatting-buttons-set", "1"], env,
        "formatting-buttons set")
    if data is not None:
        ledger.check("formatting buttons set on", data == 1, data)
    reloaded = json_call(
        ledger, cli, ["formatting-buttons"], env,
        "formatting-buttons reload")
    if reloaded is not None:
        ledger.check(
            "formatting buttons persisted",
            reloaded.get("enabled") is True,
            reloaded)

    env = make_env(work / "formatting-buttons-alias")
    root = {
        "version": 1,
        "exteraconfig": {"showFormattingButtons": True},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "formatting-buttons alias")
    if data is not None:
        ledger.check(
            "formatting buttons alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["formatting-buttons"], env,
        "formatting-buttons alias read")
    if reloaded is not None:
        ledger.check(
            "formatting buttons alias value",
            reloaded.get("enabled") is True,
            reloaded)

    for name, arguments in (
            ("formatting-buttons-set with wrong count",
             ["formatting-buttons-set"]),
            ("formatting-buttons-set invalid value",
             ["formatting-buttons-set", "2"])):
        result = run(cli, arguments)
        ledger.check(
            f"cli {name}",
            result.returncode == 2 and bool(result.stderr.strip()),
            f"exit {result.returncode}, stderr {result.stderr!r}")

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_silent(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "silent-defaults")
    data = json_call(ledger, cli, ["silent"], env, "silent defaults")
    if data is not None:
        ledger.check(
            "silent default off",
            data.get("enabled") is False,
            data)

    data = json_call(ledger, cli, ["silent-set", "1"], env, "silent set")
    if data is not None:
        ledger.check("silent set on", data == 1, data)
    reloaded = json_call(ledger, cli, ["silent"], env, "silent reload")
    if reloaded is not None:
        ledger.check(
            "silent persisted",
            reloaded.get("enabled") is True,
            reloaded)

    env = make_env(work / "silent-alias")
    root = {
        "version": 1,
        "exteraconfig": {"silentMessageByDefault": True},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "silent alias")
    if data is not None:
        ledger.check(
            "silent alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(ledger, cli, ["silent"], env, "silent alias read")
    if reloaded is not None:
        ledger.check(
            "silent alias value",
            reloaded.get("enabled") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_settings_sections(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "sections-defaults")
    data = json_call(
        ledger, cli, ["settings-sections"], env, "sections defaults")
    if data is not None:
        ledger.check(
            "sections default off",
            data.get("hidePremium") is False
            and data.get("hideHelp") is False,
            data)

    data = json_call(
        ledger, cli, ["settings-sections-set", "1", "1"], env,
        "sections set")
    if data is not None:
        ledger.check(
            "sections set on",
            data.get("hidePremium") is True
            and data.get("hideHelp") is True,
            data)
    reloaded = json_call(
        ledger, cli, ["settings-sections"], env, "sections reload")
    if reloaded is not None:
        ledger.check(
            "sections persisted",
            reloaded.get("hidePremium") is True
            and reloaded.get("hideHelp") is True,
            reloaded)

    env = make_env(work / "sections-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "hidePremiumSection": True,
            "hideHelpSection": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "sections alias")
    if data is not None:
        ledger.check(
            "sections alias applied",
            data.get("applied") == 2 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["settings-sections"], env, "sections alias read")
    if reloaded is not None:
        ledger.check(
            "sections alias values",
            reloaded.get("hidePremium") is True
            and reloaded.get("hideHelp") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_ai_editor(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "ai-editor-defaults")
    data = json_call(ledger, cli, ["ai-editor"], env, "ai-editor defaults")
    if data is not None:
        ledger.check(
            "ai editor default shown",
            data.get("hidden") is False,
            data)

    data = json_call(ledger, cli, ["ai-editor-set", "1"], env, "ai-editor set")
    if data is not None:
        ledger.check("ai editor set hidden", data == 1, data)
    reloaded = json_call(ledger, cli, ["ai-editor"], env, "ai-editor reload")
    if reloaded is not None:
        ledger.check(
            "ai editor persisted",
            reloaded.get("hidden") is True,
            reloaded)

    env = make_env(work / "ai-editor-alias")
    root = {
        "version": 1,
        "exteraconfig": {"hideAiEditor": True},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "ai-editor alias")
    if data is not None:
        ledger.check(
            "ai editor alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["ai-editor"], env, "ai-editor alias read")
    if reloaded is not None:
        ledger.check(
            "ai editor alias value",
            reloaded.get("hidden") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_plugin_installer(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    cases = [
        ("My Plugin.plugin", "My_Plugin.plugin"),
        ("../evil.plugin", "evil.plugin"),
        ("a/b/../../x.py", "x.py"),
        ("plugin.exe", "invalid"),
        ("noext", "invalid"),
        (".plugin", "invalid"),
        ("x.PLUGIN", "x.plugin"),
        ("\u0442\u0435\u0441\u0442.py", "\u0442\u0435\u0441\u0442.py"),
        ("", "invalid"),
    ]
    for name, expected in cases:
        result = run(cli, ["plugin-filename", name])
        actual = result.stdout.strip()
        ledger.check(
            f"plugin filename {name or 'empty'}",
            result.returncode == 0 and actual == expected,
            f"exit {result.returncode}, got {actual!r}, want {expected!r}")

    source = work / "plugin-source"
    source.mkdir(parents=True, exist_ok=True)
    note = source / "note.plugin"
    note.write_text("__id__ = 'note'\n", encoding="utf-8")
    install_dir = work / "plugin-install"

    result = run(cli, ["plugin-install", str(note), str(install_dir)])
    installed = result.stdout.strip()
    ledger.check(
        "plugin install first",
        result.returncode == 0 and installed == "note.plugin",
        f"exit {result.returncode}, got {installed!r}")
    target = install_dir / "note.plugin"
    ledger.check(
        "plugin install content",
        target.is_file()
        and target.read_text(encoding="utf-8") == note.read_text(encoding="utf-8"),
        target)

    result = run(cli, ["plugin-install", str(note), str(install_dir)])
    second = result.stdout.strip()
    ledger.check(
        "plugin install dedup",
        result.returncode == 0
        and second == "note-2.plugin"
        and (install_dir / "note-2.plugin").is_file(),
        second)

    bad = source / "bad.exe"
    bad.write_text("nope", encoding="utf-8")
    result = run(cli, ["plugin-install", str(bad), str(install_dir)])
    ledger.check(
        "plugin install rejects bad extension",
        result.stdout.strip() == "failed",
        result.stdout.strip())

    big = source / "big.plugin"
    big.write_bytes(b"x" * (2 * 1024 * 1024 + 1))
    result = run(cli, ["plugin-install", str(big), str(install_dir)])
    ledger.check(
        "plugin install rejects oversize",
        result.stdout.strip() == "failed",
        result.stdout.strip())

    link = source / "link.plugin"
    link.symlink_to(note)
    result = run(cli, ["plugin-install", str(link), str(install_dir)])
    ledger.check(
        "plugin install rejects symlink",
        result.stdout.strip() == "failed",
        result.stdout.strip())

    result = run(cli, ["plugin-delete", str(install_dir), "note.PLUGIN"])
    ledger.check(
        "plugin delete case insensitive",
        result.stdout.strip() == "1" and not target.is_file(),
        result.stdout.strip())

    result = run(cli, ["plugin-delete", str(install_dir), "note.plugin"])
    ledger.check(
        "plugin delete missing",
        result.stdout.strip() == "0",
        result.stdout.strip())

    result = run(cli, ["plugin-delete", str(install_dir), "../note.plugin"])
    ledger.check(
        "plugin delete rejects traversal",
        result.stdout.strip() == "0",
        result.stdout.strip())

    (install_dir / "folder.plugin").mkdir(exist_ok=True)
    result = run(cli, ["plugin-delete", str(install_dir), "folder.plugin"])
    ledger.check(
        "plugin delete rejects directory",
        result.stdout.strip() == "0",
        result.stdout.strip())

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_send_as(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "send-as-defaults")
    data = json_call(ledger, cli, ["send-as"], env, "send-as defaults")
    if data is not None:
        ledger.check(
            "send as default shown",
            data.get("hidden") is False,
            data)
    data = json_call(ledger, cli, ["send-as-set", "1"], env, "send-as set")
    if data is not None:
        ledger.check("send as set hidden", data == 1, data)
    reloaded = json_call(ledger, cli, ["send-as"], env, "send-as reload")
    if reloaded is not None:
        ledger.check(
            "send as persisted",
            reloaded.get("hidden") is True,
            reloaded)

    env = make_env(work / "send-as-alias")
    root = {
        "version": 1,
        "exteraconfig": {"hideSendAsChannel": True},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "send-as alias")
    if data is not None:
        ledger.check(
            "send as alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(ledger, cli, ["send-as"], env, "send-as alias read")
    if reloaded is not None:
        ledger.check(
            "send as alias value",
            reloaded.get("hidden") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_profile_buttons(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "profile-buttons-defaults")
    data = json_call(
        ledger, cli, ["profile-buttons"], env, "profile buttons defaults")
    if data is not None:
        ledger.check(
            "profile buttons default shown",
            data.get("gift") is False
            and data.get("call") is False
            and data.get("search") is False,
            data)
    data = json_call(
        ledger, cli, ["profile-buttons-set", "1", "0", "1"], env,
        "profile buttons set")
    if data is not None:
        ledger.check(
            "profile buttons set",
            data.get("gift") is True
            and data.get("call") is False
            and data.get("search") is True,
            data)
    reloaded = json_call(
        ledger, cli, ["profile-buttons"], env, "profile buttons reload")
    if reloaded is not None:
        ledger.check(
            "profile buttons persisted",
            reloaded.get("gift") is True
            and reloaded.get("call") is False
            and reloaded.get("search") is True,
            reloaded)

    env = make_env(work / "profile-buttons-backup")
    root = {
        "version": 1,
        "exteraconfig": {
            "OEChatsHideGiftButton": True,
            "OEChatsHideCallButton": True,
            "OEChatsHideChannelSearchButton": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "profile buttons backup")
    if data is not None:
        ledger.check(
            "profile buttons backup applied",
            data.get("applied") == 3 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["profile-buttons"], env, "profile buttons backup read")
    if reloaded is not None:
        ledger.check(
            "profile buttons backup values",
            reloaded.get("gift") is True
            and reloaded.get("call") is True
            and reloaded.get("search") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_inline_math(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "inline-math-defaults")
    data = json_call(
        ledger, cli, ["inline-math"], env, "inline-math defaults")
    if data is not None:
        ledger.check(
            "inline math default off",
            data.get("enabled") is False,
            data)
    data = json_call(
        ledger, cli, ["inline-math-set", "1"], env, "inline-math set")
    if data is not None:
        ledger.check("inline math set on", data == 1, data)
    reloaded = json_call(
        ledger, cli, ["inline-math"], env, "inline-math reload")
    if reloaded is not None:
        ledger.check(
            "inline math persisted",
            reloaded.get("enabled") is True,
            reloaded)

    cases = [
        ("2+2", "4"),
        ("2+2*2", "6"),
        ("(2+2)*2", "8"),
        ("2^10", "1024"),
        ("2^3^2", "512"),
        ("10/4", "2.5"),
        ("0.1+0.2", "0.3"),
        ("-5+2", "-3"),
        ("3%2", "1"),
        ("sqrt(16)", "4"),
        ("abs(-3)", "3"),
        ("floor(2.9)", "2"),
        ("ceil(2.1)", "3"),
        ("round(2.5)", "3"),
        ("log(100)", "2"),
        ("ln(e)", "1"),
        ("sin(0)", "0"),
        ("pi", "3.141592654"),
        ("2 + 2", "4"),
        ("1/0", "invalid"),
        ("2+", "invalid"),
        ("abc", "invalid"),
        ("", "invalid"),
        ("2 2", "invalid"),
        ("sqrt(-1)", "invalid"),
        ("1e999", "invalid"),
    ]
    for expression, expected in cases:
        result = run(cli, ["math-eval", expression])
        actual = result.stdout.strip()
        ledger.check(
            f"math eval {expression or 'empty'}",
            result.returncode == 0 and actual == expected,
            f"exit {result.returncode}, got {actual!r}, want {expected!r}")

    env = make_env(work / "inline-math-alias")
    root = {
        "version": 1,
        "exteraconfig": {"inlineMathResult": True},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "inline-math alias")
    if data is not None:
        ledger.check(
            "inline math alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["inline-math"], env, "inline-math alias read")
    if reloaded is not None:
        ledger.check(
            "inline math alias value",
            reloaded.get("enabled") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_save_subfolder(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "save-subfolder-defaults")
    data = json_call(
        ledger, cli, ["save-subfolder"], env, "save-subfolder defaults")
    if data is not None:
        ledger.check(
            "save subfolder default off",
            data.get("enabled") is False,
            data)
    data = json_call(
        ledger, cli, ["save-subfolder-set", "1"], env, "save-subfolder set")
    if data is not None:
        ledger.check("save subfolder set on", data == 1, data)
    reloaded = json_call(
        ledger, cli, ["save-subfolder"], env, "save-subfolder reload")
    if reloaded is not None:
        ledger.check(
            "save subfolder persisted",
            reloaded.get("enabled") is True,
            reloaded)

    folder_cases = [
        ("Кифил", "Кифил"),
        ("a/b", "a_b"),
        ("a:b*c?", "a_b_c_"),
        ("abc...", "abc"),
        ("  spaced  ", "spaced"),
        ("", "Chat"),
        ("x" * 80, "x" * 64),
    ]
    for name, expected in folder_cases:
        result = run(cli, ["chat-folder", name])
        actual = result.stdout.rstrip("\n")
        ledger.check(
            f"chat folder {name[:12] or 'empty'}",
            result.returncode == 0 and actual == expected,
            f"exit {result.returncode}, got {actual!r}, want {expected!r}")

    saves = work / "saves"
    target = work / "saves" / "photo.jpg"
    result = run(cli, ["chat-subfolder", str(target), "Кифил"])
    resolved = result.stdout.strip()
    expected = str(saves / "Кифил" / "photo.jpg")
    ledger.check(
        "chat subfolder path",
        result.returncode == 0 and resolved == expected,
        f"got {resolved!r}, want {expected!r}")
    ledger.check(
        "chat subfolder created",
        (saves / "Кифил").is_dir(),
        saves / "Кифил")

    (saves / "Кифил" / "photo.jpg").write_text("x", encoding="utf-8")
    result = run(cli, ["chat-subfolder", str(target), "Кифил"])
    resolved = result.stdout.strip()
    expected = str(saves / "Кифил" / "photo (2).jpg")
    ledger.check(
        "chat subfolder collision",
        result.returncode == 0 and resolved == expected,
        f"got {resolved!r}, want {expected!r}")

    env = make_env(work / "save-subfolder-alias")
    root = {
        "version": 1,
        "exteraconfig": {"saveToChatSubfolder": True},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "save-subfolder alias")
    if data is not None:
        ledger.check(
            "save subfolder alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["save-subfolder"], env, "save-subfolder alias read")
    if reloaded is not None:
        ledger.check(
            "save subfolder alias value",
            reloaded.get("enabled") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_ghost_schedule(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "ghost-schedule-defaults")
    data = json_call(
        ledger, cli, ["ghost-schedule"], env, "ghost-schedule defaults")
    if data is not None:
        ledger.check(
            "ghost schedule default off",
            data.get("enabled") is False,
            data)
    data = json_call(
        ledger, cli, ["ghost-schedule-set", "1"], env, "ghost-schedule set")
    if data is not None:
        ledger.check("ghost schedule set on", data == 1, data)
    reloaded = json_call(
        ledger, cli, ["ghost-schedule"], env, "ghost-schedule reload")
    if reloaded is not None:
        ledger.check(
            "ghost schedule persisted",
            reloaded.get("enabled") is True,
            reloaded)

    # enabled sendOnline self secret bot user -> schedule?
    cases = [
        ("private user", ["1", "0", "0", "0", "0", "1"], "1"),
        ("group", ["1", "0", "0", "0", "0", "0"], "1"),
        ("online packets still sent", ["1", "1", "0", "0", "0", "1"], "0"),
        ("feature disabled", ["0", "0", "0", "0", "0", "1"], "0"),
        ("self chat", ["1", "0", "1", "0", "0", "1"], "0"),
        ("secret chat", ["1", "0", "0", "1", "0", "1"], "0"),
        ("bot user", ["1", "0", "0", "0", "1", "1"], "0"),
    ]
    for name, arguments, expected in cases:
        result = run(cli, ["ghost-schedule-decide", *arguments])
        actual = result.stdout.strip()
        ledger.check(
            f"ghost schedule {name}",
            result.returncode == 0 and actual == expected,
            f"exit {result.returncode}, got {actual!r}, want {expected!r}")

    env = make_env(work / "ghost-schedule-alias")
    root = {
        "version": 1,
        "exteraconfig": {"GhostScheduledSend": True},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "ghost-schedule alias")
    if data is not None:
        ledger.check(
            "ghost schedule alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["ghost-schedule"], env, "ghost-schedule alias read")
    if reloaded is not None:
        ledger.check(
            "ghost schedule alias value",
            reloaded.get("enabled") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_seek_duration(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "seek-duration-defaults")
    data = json_call(
        ledger, cli, ["seek-duration"], env, "seek-duration defaults")
    if data is not None:
        ledger.check(
            "seek duration default",
            data.get("seconds") == 5,
            data)

    data = json_call(
        ledger, cli, ["seek-duration-set", "15"], env, "seek-duration set")
    if data is not None:
        ledger.check(
            "seek duration set",
            data.get("seconds") == 15,
            data)
    reloaded = json_call(
        ledger, cli, ["seek-duration"], env, "seek-duration reload")
    if reloaded is not None:
        ledger.check(
            "seek duration persisted",
            reloaded.get("seconds") == 15,
            reloaded)

    data = json_call(
        ledger, cli, ["seek-duration-set", "7"], env,
        "seek-duration invalid")
    if data is not None:
        ledger.check(
            "seek duration invalid falls back",
            data.get("seconds") == 5,
            data)

    env = make_env(work / "seek-duration-index")
    root = {
        "version": 1,
        "exteraconfig": {"doubleTapSeekDuration": 2},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "seek-duration index alias")
    if data is not None:
        ledger.check(
            "seek duration index applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["seek-duration"], env, "seek-duration index read")
    if reloaded is not None:
        ledger.check(
            "seek duration index value",
            reloaded.get("seconds") == 15,
            reloaded)

    env = make_env(work / "seek-duration-key")
    root = {
        "version": 1,
        "exteraconfig": {"OEChatsSeekDuration": 30},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "seek-duration key")
    if data is not None:
        ledger.check(
            "seek duration key applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["seek-duration"], env, "seek-duration key read")
    if reloaded is not None:
        ledger.check(
            "seek duration key value",
            reloaded.get("seconds") == 30,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_sticker_size(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "sticker-size-defaults")
    data = json_call(
        ledger, cli, ["sticker-size"], env, "sticker-size defaults")
    if data is not None:
        ledger.check(
            "sticker size default",
            data.get("stickerSize") == 14,
            data)

    data = json_call(
        ledger, cli, ["sticker-size-set", "12"], env, "sticker-size set")
    if data is not None:
        ledger.check(
            "sticker size set",
            data.get("stickerSize") == 12,
            data)
    reloaded = json_call(
        ledger, cli, ["sticker-size"], env, "sticker-size reload")
    if reloaded is not None:
        ledger.check(
            "sticker size persisted",
            reloaded.get("stickerSize") == 12,
            reloaded)

    data = json_call(
        ledger, cli, ["sticker-size-set", "1"], env,
        "sticker-size clamp low")
    if data is not None:
        ledger.check(
            "sticker size clamp low",
            data.get("stickerSize") == 4,
            data)

    data = json_call(
        ledger, cli, ["sticker-size-set", "30"], env,
        "sticker-size clamp high")
    if data is not None:
        ledger.check(
            "sticker size clamp high",
            data.get("stickerSize") == 20,
            data)

    env = make_env(work / "sticker-size-alias")
    root = {
        "version": 1,
        "exteraconfig": {"stickerSize": 13.7},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "sticker-size alias")
    if data is not None:
        ledger.check(
            "sticker size alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["sticker-size"], env, "sticker-size alias read")
    if reloaded is not None:
        ledger.check(
            "sticker size alias rounded",
            reloaded.get("stickerSize") == 14,
            reloaded)

    env = make_env(work / "sticker-size-key")
    root = {
        "version": 1,
        "exteraconfig": {"OEChatsStickerSize": 18},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "sticker-size key")
    if data is not None:
        ledger.check(
            "sticker size key applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["sticker-size"], env, "sticker-size key read")
    if reloaded is not None:
        ledger.check(
            "sticker size key value",
            reloaded.get("stickerSize") == 18,
            reloaded)
    current = json_call(
        ledger, cli, ["backup-export-current"], env,
        "sticker-size export")
    if current is not None:
        section = current.get("exteraconfig") or {}
        ledger.check(
            "sticker size export key",
            section.get("OEChatsStickerSize") == 18,
            section.get("OEChatsStickerSize"))

    env = make_env(work / "sticker-shape-defaults")
    data = json_call(
        ledger, cli, ["sticker-shape"], env, "sticker-shape defaults")
    if data is not None:
        ledger.check(
            "sticker shape default",
            data.get("stickerShape") == 0,
            data)

    data = json_call(
        ledger, cli, ["sticker-shape-set", "1"], env, "sticker-shape set")
    if data is not None:
        ledger.check(
            "sticker shape set",
            data.get("stickerShape") == 1,
            data)
    reloaded = json_call(
        ledger, cli, ["sticker-shape"], env, "sticker-shape reload")
    if reloaded is not None:
        ledger.check(
            "sticker shape persisted",
            reloaded.get("stickerShape") == 1,
            reloaded)

    data = json_call(
        ledger, cli, ["sticker-shape-set", "2"], env, "sticker-shape bubble")
    if data is not None:
        ledger.check(
            "sticker shape bubble",
            data.get("stickerShape") == 2,
            data)

    data = json_call(
        ledger, cli, ["sticker-shape-set", "3"], env,
        "sticker-shape clamp high")
    if data is not None:
        ledger.check(
            "sticker shape clamp high",
            data.get("stickerShape") == 2,
            data)

    data = json_call(
        ledger, cli, ["sticker-shape-set", "-1"], env,
        "sticker-shape clamp low")
    if data is not None:
        ledger.check(
            "sticker shape clamp low",
            data.get("stickerShape") == 0,
            data)

    env = make_env(work / "sticker-shape-alias")
    root = {
        "version": 1,
        "exteraconfig": {"stickerShape": 1.4},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "sticker-shape alias")
    if data is not None:
        ledger.check(
            "sticker shape alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["sticker-shape"], env, "sticker-shape alias read")
    if reloaded is not None:
        ledger.check(
            "sticker shape alias rounded",
            reloaded.get("stickerShape") == 1,
            reloaded)

    env = make_env(work / "sticker-shape-key")
    root = {
        "version": 1,
        "exteraconfig": {"OEChatsStickerShape": 2},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "sticker-shape key")
    if data is not None:
        ledger.check(
            "sticker shape key applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["sticker-shape"], env, "sticker-shape key read")
    if reloaded is not None:
        ledger.check(
            "sticker shape key value",
            reloaded.get("stickerShape") == 2,
            reloaded)
    current = json_call(
        ledger, cli, ["backup-export-current"], env, "sticker-shape export")
    if current is not None:
        section = current.get("exteraconfig") or {}
        ledger.check(
            "sticker shape export key",
            section.get("OEChatsStickerShape") == 2,
            section.get("OEChatsStickerShape"))

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_avatar_corners(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "avatar-corners-defaults")
    data = json_call(
        ledger, cli, ["avatar-corners"], env, "avatar corners defaults")
    if data is not None:
        ledger.check(
            "avatar corners default circle",
            data.get("radius") == -1,
            data)

    data = json_call(
        ledger, cli, ["avatar-corners-set", "8"], env,
        "avatar corners set")
    if data is not None:
        ledger.check(
            "avatar corners set rounded",
            data.get("radius") == 8,
            data)
    reloaded = json_call(
        ledger, cli, ["avatar-corners"], env, "avatar corners reload")
    if reloaded is not None:
        ledger.check(
            "avatar corners persisted",
            reloaded.get("radius") == 8,
            reloaded)

    data = json_call(
        ledger, cli, ["avatar-corners-set", "0"], env,
        "avatar corners square")
    if data is not None:
        ledger.check(
            "avatar corners square",
            data.get("radius") == 0,
            data)

    data = json_call(
        ledger, cli, ["avatar-corners-set", "99"], env,
        "avatar corners clamp")
    if data is not None:
        ledger.check(
            "avatar corners clamped to 32",
            data.get("radius") == 32,
            data)

    env = make_env(work / "avatar-corners-alias")
    root = {
        "version": 1,
        "exteraconfig": {"avatarCorners": 12},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "avatar corners alias")
    if data is not None:
        ledger.check(
            "avatar corners alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["avatar-corners"], env, "avatar corners alias read")
    if reloaded is not None:
        ledger.check(
            "avatar corners alias value",
            reloaded.get("radius") == 12,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_header_style(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "header-style-defaults")
    data = json_call(
        ledger, cli, ["header-style"], env, "header style defaults")
    if data is not None:
        ledger.check(
            "header style default upstream",
            data.get("style") == 0,
            data)

    data = json_call(
        ledger, cli, ["header-style-set", "1"], env, "header style ios")
    if data is not None:
        ledger.check("header style set ios", data.get("style") == 1, data)
    reloaded = json_call(
        ledger, cli, ["header-style"], env, "header style reload ios")
    if reloaded is not None:
        ledger.check(
            "header style ios persisted",
            reloaded.get("style") == 1,
            reloaded)

    data = json_call(
        ledger, cli, ["header-style-set", "2"], env, "header style m3")
    if data is not None:
        ledger.check("header style set m3", data.get("style") == 2, data)
    reloaded = json_call(
        ledger, cli, ["header-style"], env, "header style reload m3")
    if reloaded is not None:
        ledger.check(
            "header style m3 persisted",
            reloaded.get("style") == 2,
            reloaded)

    data = json_call(
        ledger, cli, ["header-style-set", "9"], env,
        "header style clamp")
    if data is not None:
        ledger.check(
            "header style clamped to 2",
            data.get("style") == 2,
            data)

    env = make_env(work / "header-style-alias")
    root = {
        "version": 1,
        "exteraconfig": {"iosChatHeader": True},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "header style alias")
    if data is not None:
        ledger.check(
            "header style alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["header-style"], env, "header style alias read")
    if reloaded is not None:
        ledger.check(
            "header style alias value",
            reloaded.get("style") == 1,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_section_radius(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "section-radius-defaults")
    data = json_call(
        ledger, cli, ["section-radius"], env, "section radius defaults")
    if data is not None:
        ledger.check(
            "section radius default upstream",
            data.get("radius") == -1,
            data)

    data = json_call(
        ledger, cli, ["separate-headers"], env,
        "separate headers defaults")
    if data is not None:
        ledger.check(
            "separate headers default off",
            data.get("enabled") is False,
            data)

    data = json_call(
        ledger, cli, ["section-radius-set", "8"], env,
        "section radius set")
    if data is not None:
        ledger.check(
            "section radius set rounded",
            data.get("radius") == 8,
            data)
    reloaded = json_call(
        ledger, cli, ["section-radius"], env, "section radius reload")
    if reloaded is not None:
        ledger.check(
            "section radius persisted",
            reloaded.get("radius") == 8,
            reloaded)

    data = json_call(
        ledger, cli, ["separate-headers-set", "1"], env,
        "separate headers set")
    if data is not None:
        ledger.check(
            "separate headers set on",
            data.get("enabled") is True,
            data)
    reloaded = json_call(
        ledger, cli, ["separate-headers"], env,
        "separate headers reload")
    if reloaded is not None:
        ledger.check(
            "separate headers persisted",
            reloaded.get("enabled") is True,
            reloaded)

    data = json_call(
        ledger, cli, ["section-radius-set", "99"], env,
        "section radius clamp")
    if data is not None:
        ledger.check(
            "section radius clamped to 16",
            data.get("radius") == 16,
            data)

    data = json_call(
        ledger, cli, ["section-radius-set", "-9"], env,
        "section radius low clamp")
    if data is not None:
        ledger.check(
            "section radius clamped to -1",
            data.get("radius") == -1,
            data)

    env = make_env(work / "section-radius-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "sectionRadius": 12,
            "separateHeaders": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "section radius alias")
    if data is not None:
        ledger.check(
            "section radius alias applied",
            data.get("applied") == 2 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["section-radius"], env, "section radius alias read")
    if reloaded is not None:
        ledger.check(
            "section radius alias value",
            reloaded.get("radius") == 12,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["separate-headers"], env,
        "separate headers alias read")
    if reloaded is not None:
        ledger.check(
            "separate headers alias value",
            reloaded.get("enabled") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_edited_mark(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "edited-mark-defaults")
    data = json_call(
        ledger, cli, ["edited-mark"], env, "edited-mark defaults")
    if data is not None:
        ledger.check(
            "edited mark defaults",
            data.get("icon") is False and data.get("mark") == "",
            data)

    data = json_call(
        ledger, cli, ["edited-mark-set", "1", "ред."], env,
        "edited-mark set")
    if data is not None:
        ledger.check(
            "edited mark set",
            data.get("icon") is True and data.get("mark") == "ред.",
            data)
    reloaded = json_call(
        ledger, cli, ["edited-mark"], env, "edited-mark reload")
    if reloaded is not None:
        ledger.check(
            "edited mark persisted",
            reloaded.get("icon") is True and reloaded.get("mark") == "ред.",
            reloaded)

    long_mark = "y" * 40
    data = json_call(
        ledger, cli, ["edited-mark-set", "0", long_mark], env,
        "edited-mark truncation")
    if data is not None:
        ledger.check(
            "edited mark truncated",
            data.get("mark") == "y" * 16 and data.get("icon") is False,
            data)

    env = make_env(work / "edited-mark-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "useEditedIcon": True,
            "customEditedMessage": "изм.",
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "edited-mark alias")
    if data is not None:
        ledger.check(
            "edited mark alias applied",
            data.get("applied") == 2 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["edited-mark"], env, "edited-mark alias read")
    if reloaded is not None:
        ledger.check(
            "edited mark alias values",
            reloaded.get("icon") is True
            and reloaded.get("mark") == "изм.",
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_forward_date(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "forward-date-defaults")
    data = json_call(
        ledger, cli, ["forward-date"], env, "forward-date defaults")
    if data is not None:
        ledger.check(
            "forward date default off",
            data.get("enabled") is False,
            data)
    data = json_call(
        ledger, cli, ["forward-date-set", "1"], env, "forward-date set")
    if data is not None:
        ledger.check("forward date set on", data == 1, data)
    reloaded = json_call(
        ledger, cli, ["forward-date"], env, "forward-date reload")
    if reloaded is not None:
        ledger.check(
            "forward date persisted",
            reloaded.get("enabled") is True,
            reloaded)

    env = make_env(work / "forward-date-alias")
    root = {
        "version": 1,
        "exteraconfig": {"dateOfForwardedMsg": True},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "forward-date alias")
    if data is not None:
        ledger.check(
            "forward date alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["forward-date"], env, "forward-date alias read")
    if reloaded is not None:
        ledger.check(
            "forward date alias value",
            reloaded.get("enabled") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_sender_mini_avatars(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "sender-mini-avatars-defaults")
    data = json_call(
        ledger, cli, ["sender-mini-avatars"], env,
        "sender-mini-avatars defaults")
    if data is not None:
        ledger.check(
            "sender mini avatars default off",
            data.get("enabled") is False,
            data)
    data = json_call(
        ledger, cli, ["sender-mini-avatars-set", "1"], env,
        "sender-mini-avatars set")
    if data is not None:
        ledger.check("sender mini avatars set on", data == 1, data)
    reloaded = json_call(
        ledger, cli, ["sender-mini-avatars"], env,
        "sender-mini-avatars reload")
    if reloaded is not None:
        ledger.check(
            "sender mini avatars persisted",
            reloaded.get("enabled") is True,
            reloaded)

    env = make_env(work / "sender-mini-avatars-alias")
    root = {
        "version": 1,
        "exteraconfig": {"senderMiniAvatars": True},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "sender-mini-avatars alias")
    if data is not None:
        ledger.check(
            "sender mini avatars alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["sender-mini-avatars"], env,
        "sender-mini-avatars alias read")
    if reloaded is not None:
        ledger.check(
            "sender mini avatars alias value",
            reloaded.get("enabled") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_persian(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    cases = [
        ("2024-03-20", "1", "1", "1 Farvardin 1403"),
        ("2024-03-20", "1", "0", "1 Farvardin"),
        ("2024-03-20", "0", "1", "۱ فروردین ۱۴۰۳"),
        ("2024-03-20", "0", "0", "۱ فروردین"),
        ("2026-11-06", "1", "1", "15 Aban 1405"),
        ("2026-11-06", "0", "1", "۱۵ آبان ۱۴۰۵"),
    ]
    for date, latin, year, expected in cases:
        name = f"persian date {date} latin={latin} year={year}"
        data = json_call(
            ledger, cli, ["persian-date", date, latin, year], None, name)
        if data is not None:
            ledger.check(name, data.get("text") == expected, data)

    for date in ("2024-02-30", "2024-13-01", "not-a-date", "2024-11-06x"):
        name = f"persian invalid date {date}"
        data = json_call(
            ledger, cli, ["persian-date", date, "1", "1"], None, name)
        if data is not None:
            ledger.check(name, data.get("text") == "", data)

    env = make_env(work / "persian-defaults")
    data = json_call(
        ledger, cli, ["persian-calendar"], env, "persian calendar defaults")
    if data is not None:
        ledger.check(
            "persian calendar default off",
            data.get("usePersianCalendar") is False
            and data.get("displayPersianCalendarByLatin") is False,
            data)
    data = json_call(
        ledger, cli, ["persian-calendar-set", "1", "1"], env,
        "persian calendar set")
    if data is not None:
        ledger.check(
            "persian calendar set on",
            data.get("usePersianCalendar") is True
            and data.get("displayPersianCalendarByLatin") is True,
            data)
    reloaded = json_call(
        ledger, cli, ["persian-calendar"], env, "persian calendar reload")
    if reloaded is not None:
        ledger.check(
            "persian calendar persisted",
            reloaded.get("usePersianCalendar") is True
            and reloaded.get("displayPersianCalendarByLatin") is True,
            reloaded)
    data = json_call(
        ledger, cli, ["persian-calendar-set", "0"], env,
        "persian calendar off keeps latin")
    if data is not None:
        ledger.check(
            "persian calendar latin kept",
            data.get("usePersianCalendar") is False
            and data.get("displayPersianCalendarByLatin") is True,
            data)
    result = run(cli, ["persian-calendar-set"], env=env)
    ledger.check(
        "persian calendar rejects missing flag",
        result.returncode == 2,
        f"exit {result.returncode}")

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_media_menu(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "media-menu-defaults")
    data = json_call(ledger, cli, ["media-menu"], env, "media-menu defaults")
    if data is not None:
        ledger.check(
            "media menu defaults",
            data.get("forward") is True
            and data.get("copyFrame") is True
            and data.get("copyPhoto") is True
            and data.get("setProfile") is True,
            data)

    data = json_call(
        ledger, cli, ["media-menu-set", "0", "1", "0", "1"], env,
        "media-menu set")
    if data is not None:
        ledger.check(
            "media menu set",
            data.get("forward") is False
            and data.get("copyFrame") is True
            and data.get("copyPhoto") is False
            and data.get("setProfile") is True,
            data)
    reloaded = json_call(
        ledger, cli, ["media-menu"], env, "media-menu reload")
    if reloaded is not None:
        ledger.check(
            "media menu persisted",
            reloaded.get("forward") is False
            and reloaded.get("copyPhoto") is False,
            reloaded)

    env = make_env(work / "media-menu-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "mediaViewerMenuItemForward": False,
            "mediaViewerMenuItemCopyFrame": False,
            "mediaViewerMenuItemCopyPhoto": False,
            "mediaViewerMenuItemSetProfilePhoto": False,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "media-menu alias")
    if data is not None:
        ledger.check(
            "media menu alias applied",
            data.get("applied") == 4 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["media-menu"], env, "media-menu alias read")
    if reloaded is not None:
        ledger.check(
            "media menu alias values",
            reloaded.get("forward") is False
            and reloaded.get("copyFrame") is False
            and reloaded.get("copyPhoto") is False
            and reloaded.get("setProfile") is False,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_chat_menu(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "chat-menu-defaults")
    data = json_call(ledger, cli, ["chat-menu"], env, "chat-menu defaults")
    if data is not None:
        ledger.check(
            "chat menu defaults",
            data.get("boost") is True
            and data.get("linked") is True
            and data.get("viewDeleted") is True
            and data.get("clearDeleted") is True,
            data)

    data = json_call(
        ledger, cli, ["chat-menu-set", "0", "1", "0", "1"], env,
        "chat-menu set")
    if data is not None:
        ledger.check(
            "chat menu set",
            data.get("boost") is False
            and data.get("linked") is True
            and data.get("viewDeleted") is False
            and data.get("clearDeleted") is True,
            data)
    reloaded = json_call(
        ledger, cli, ["chat-menu"], env, "chat-menu reload")
    if reloaded is not None:
        ledger.check(
            "chat menu persisted",
            reloaded.get("boost") is False
            and reloaded.get("viewDeleted") is False,
            reloaded)

    env = make_env(work / "chat-menu-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "chatMenuItemBoostGroup": False,
            "chatMenuItemLinkedChat": False,
            "chatMenuItemViewDeleted": False,
            "chatMenuItemClearDeleted": False,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "chat-menu alias")
    if data is not None:
        ledger.check(
            "chat menu alias applied",
            data.get("applied") == 4 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["chat-menu"], env, "chat-menu alias read")
    if reloaded is not None:
        ledger.check(
            "chat menu alias values",
            reloaded.get("boost") is False
            and reloaded.get("linked") is False
            and reloaded.get("viewDeleted") is False
            and reloaded.get("clearDeleted") is False,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_disable_link_preview(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "disable-link-preview-defaults")
    data = json_call(
        ledger, cli, ["disable-link-preview"], env,
        "disable-link-preview defaults")
    if data is not None:
        ledger.check(
            "disable link preview default off",
            data.get("disabled") is False,
            data)

    data = json_call(
        ledger, cli, ["disable-link-preview-set", "1"], env,
        "disable-link-preview set")
    if data is not None:
        ledger.check("disable link preview set on", data == 1, data)

    reloaded = json_call(
        ledger, cli, ["disable-link-preview"], env,
        "disable-link-preview reload")
    if reloaded is not None:
        ledger.check(
            "disable link preview persisted",
            reloaded.get("disabled") is True,
            reloaded)

    env = make_env(work / "disable-link-preview-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "disableLinkPreviewByDefault": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "disable-link-preview alias")
    if data is not None:
        ledger.check(
            "disable link preview alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["disable-link-preview"], env,
        "disable-link-preview alias read")
    if reloaded is not None:
        ledger.check(
            "disable link preview alias value",
            reloaded.get("disabled") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_hide_all_chats(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "hide-all-chats-defaults")
    data = json_call(
        ledger, cli, ["hide-all-chats"], env,
        "hide-all-chats defaults")
    if data is not None:
        ledger.check(
            "hide all chats default off",
            data.get("hidden") is False,
            data)

    data = json_call(
        ledger, cli, ["hide-all-chats-set", "1"], env,
        "hide-all-chats set")
    if data is not None:
        ledger.check("hide all chats set on", data == 1, data)

    reloaded = json_call(
        ledger, cli, ["hide-all-chats"], env,
        "hide-all-chats reload")
    if reloaded is not None:
        ledger.check(
            "hide all chats persisted",
            reloaded.get("hidden") is True,
            reloaded)

    env = make_env(work / "hide-all-chats-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "hideAllTab": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "hide-all-chats alias")
    if data is not None:
        ledger.check(
            "hide all chats alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["hide-all-chats"], env,
        "hide-all-chats alias read")
    if reloaded is not None:
        ledger.check(
            "hide all chats alias value",
            reloaded.get("hidden") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_hide_archive_sort(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "hide-archive-sort-defaults")
    data = json_call(
        ledger, cli, ["hide-archive"], env,
        "hide-archive defaults")
    if data is not None:
        ledger.check(
            "hide archive default off",
            data.get("hidden") is False,
            data)
    data = json_call(
        ledger, cli, ["sort-unread"], env,
        "sort-unread defaults")
    if data is not None:
        ledger.check(
            "sort by unread default off",
            data.get("sorted") is False,
            data)

    data = json_call(
        ledger, cli, ["hide-archive-set", "1"], env,
        "hide-archive set")
    if data is not None:
        ledger.check("hide archive set on", data == 1, data)
    data = json_call(
        ledger, cli, ["sort-unread-set", "1"], env,
        "sort-unread set")
    if data is not None:
        ledger.check("sort by unread set on", data == 1, data)

    reloaded = json_call(
        ledger, cli, ["hide-archive"], env,
        "hide-archive reload")
    if reloaded is not None:
        ledger.check(
            "hide archive persisted",
            reloaded.get("hidden") is True,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["sort-unread"], env,
        "sort-unread reload")
    if reloaded is not None:
        ledger.check(
            "sort by unread persisted",
            reloaded.get("sorted") is True,
            reloaded)

    for in_main_menu in (False, True):
        for hide_archive in (False, True):
            expected = in_main_menu or hide_archive
            data = json_call(
                ledger, cli,
                ["archive-skip",
                 "1" if in_main_menu else "0",
                 "1" if hide_archive else "0"],
                env,
                f"archive-skip {int(in_main_menu)}"
                f"{int(hide_archive)}")
            if data is not None:
                ledger.check(
                    f"archive skip {int(in_main_menu)}"
                    f"{int(hide_archive)}",
                    data.get("skip") is expected,
                    data)

    env = make_env(work / "hide-archive-sort-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "hideArchiveFolder": True,
            "SortByUnread": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "hide-archive-sort alias")
    if data is not None:
        ledger.check(
            "hide archive alias applied",
            data.get("applied") == 2 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["hide-archive"], env,
        "hide-archive alias read")
    if reloaded is not None:
        ledger.check(
            "hide archive alias value",
            reloaded.get("hidden") is True,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["sort-unread"], env,
        "sort-unread alias read")
    if reloaded is not None:
        ledger.check(
            "sort by unread alias value",
            reloaded.get("sorted") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_autodownload_rules(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "autodownload-rules-defaults")
    data = json_call(
        ledger, cli, ["autodownload-archive"], env,
        "autodownload-archive defaults")
    if data is not None:
        ledger.check(
            "archive autodownload default on",
            data.get("disabled") is False,
            data)
    data = json_call(
        ledger, cli, ["win32-exe"], env,
        "win32-exe defaults")
    if data is not None:
        ledger.check(
            "win32 executables default blocked",
            data.get("blocked") is True,
            data)

    data = json_call(
        ledger, cli, ["autodownload-archive-set", "1"], env,
        "autodownload-archive set")
    if data is not None:
        ledger.check("archive autodownload set off", data == 1, data)
    data = json_call(
        ledger, cli, ["win32-exe-set", "0"], env,
        "win32-exe set")
    if data is not None:
        ledger.check("win32 executables set allowed", data == 0, data)

    reloaded = json_call(
        ledger, cli, ["autodownload-archive"], env,
        "autodownload-archive reload")
    if reloaded is not None:
        ledger.check(
            "archive autodownload persisted",
            reloaded.get("disabled") is True,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["win32-exe"], env,
        "win32-exe reload")
    if reloaded is not None:
        ledger.check(
            "win32 executables persisted",
            reloaded.get("blocked") is False,
            reloaded)

    env = make_env(work / "autodownload-rules-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "disableAutoDownloadingArchive": True,
            "Win32ExecutableFiles": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "autodownload-rules alias")
    if data is not None:
        ledger.check(
            "autodownload alias applied",
            data.get("applied") == 2 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["autodownload-archive"], env,
        "autodownload-archive alias read")
    if reloaded is not None:
        ledger.check(
            "archive autodownload alias value",
            reloaded.get("disabled") is True,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["win32-exe"], env,
        "win32-exe alias read")
    if reloaded is not None:
        ledger.check(
            "win32 executables alias value",
            reloaded.get("blocked") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_wide_posts(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "wide-posts-defaults")
    data = json_call(
        ledger, cli, ["wide-posts"], env,
        "wide-posts defaults")
    if data is not None:
        ledger.check(
            "wide channel posts default off",
            data.get("wideChannelPosts") is False,
            data)

    data = json_call(
        ledger, cli, ["wide-posts-set", "1"], env,
        "wide-posts set")
    if data is not None:
        ledger.check("wide channel posts set on", data == 1, data)

    reloaded = json_call(
        ledger, cli, ["wide-posts"], env,
        "wide-posts reload")
    if reloaded is not None:
        ledger.check(
            "wide channel posts persisted",
            reloaded.get("wideChannelPosts") is True,
            reloaded)

    env = make_env(work / "wide-posts-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "wideChannelPosts": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "wide-posts alias")
    if data is not None:
        ledger.check(
            "wide channel posts alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["wide-posts"], env,
        "wide-posts alias read")
    if reloaded is not None:
        ledger.check(
            "wide channel posts alias value",
            reloaded.get("wideChannelPosts") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_always_show_download(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "always-show-download-defaults")
    data = json_call(
        ledger, cli, ["always-show-download"], env,
        "always-show-download defaults")
    if data is not None:
        ledger.check(
            "always show download default off",
            data.get("enabled") is False,
            data)

    data = json_call(
        ledger, cli, ["always-show-download-set", "1"], env,
        "always-show-download set")
    if data is not None:
        ledger.check("always show download set on", data == 1, data)

    reloaded = json_call(
        ledger, cli, ["always-show-download"], env,
        "always-show-download reload")
    if reloaded is not None:
        ledger.check(
            "always show download persisted",
            reloaded.get("enabled") is True,
            reloaded)

    env = make_env(work / "always-show-download-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "alwaysShowDownloadIcon": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "always-show-download alias")
    if data is not None:
        ledger.check(
            "always show download alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["always-show-download"], env,
        "always-show-download alias read")
    if reloaded is not None:
        ledger.check(
            "always show download alias value",
            reloaded.get("enabled") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_show_results_before_voting(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "show-results-before-voting-defaults")
    data = json_call(
        ledger, cli, ["show-results-before-voting"], env,
        "show-results-before-voting defaults")
    if data is not None:
        ledger.check(
            "show results before voting default off",
            data.get("enabled") is False,
            data)

    data = json_call(
        ledger, cli, ["show-results-before-voting-set", "1"], env,
        "show-results-before-voting set")
    if data is not None:
        ledger.check("show results before voting set on", data == 1, data)

    reloaded = json_call(
        ledger, cli, ["show-results-before-voting"], env,
        "show-results-before-voting reload")
    if reloaded is not None:
        ledger.check(
            "show results before voting persisted",
            reloaded.get("enabled") is True,
            reloaded)

    env = make_env(work / "show-results-before-voting-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "showResultsBeforeVoting": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "show-results-before-voting alias")
    if data is not None:
        ledger.check(
            "show results before voting alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["show-results-before-voting"], env,
        "show-results-before-voting alias read")
    if reloaded is not None:
        ledger.check(
            "show results before voting alias value",
            reloaded.get("enabled") is True,
            reloaded)

    env = make_env(work / "show-results-before-voting-oe")
    root = {
        "version": 1,
        "exteraconfig": {
            "OEChatsShowResultsBeforeVoting": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "show-results-before-voting OE")
    if data is not None:
        ledger.check(
            "show results before voting OE key applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["show-results-before-voting"], env,
        "show-results-before-voting OE read")
    if reloaded is not None:
        ledger.check(
            "show results before voting OE value",
            reloaded.get("enabled") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_reply_style(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "reply-style-defaults")
    data = json_call(
        ledger, cli, ["reply-style"], env, "reply-style defaults")
    if data is not None:
        ledger.check(
            "reply style defaults on",
            data.get("colors") is True
            and data.get("emoji") is True
            and data.get("background") is True,
            data)

    data = json_call(
        ledger, cli, ["reply-style-set", "0", "1", "0"], env,
        "reply-style set")
    if data is not None:
        ledger.check(
            "reply style set applied",
            data.get("colors") is False
            and data.get("emoji") is True
            and data.get("background") is False,
            data)

    reloaded = json_call(
        ledger, cli, ["reply-style"], env, "reply-style reload")
    if reloaded is not None:
        ledger.check(
            "reply style persisted",
            reloaded.get("colors") is False
            and reloaded.get("emoji") is True
            and reloaded.get("background") is False,
            reloaded)

    env = make_env(work / "reply-style-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "replyColors": False,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "reply-style alias")
    if data is not None:
        ledger.check(
            "reply style alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["reply-style"], env, "reply-style alias read")
    if reloaded is not None:
        ledger.check(
            "reply style alias value",
            reloaded.get("colors") is False
            and reloaded.get("emoji") is True
            and reloaded.get("background") is True,
            reloaded)

    env = make_env(work / "reply-style-legacy")
    root = {
        "version": 1,
        "exteraconfig": {
            "messageColoredBackground": False,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "reply-style legacy")
    if data is not None:
        ledger.check(
            "reply style legacy applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["reply-style"], env, "reply-style legacy read")
    if reloaded is not None:
        ledger.check(
            "reply style legacy value",
            reloaded.get("background") is False,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_false_bottom(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "false-bottom-defaults")
    data = json_call(
        ledger, cli, ["false-bottom"], env,
        "false-bottom defaults")
    if data is not None:
        ledger.check(
            "false bottom default off",
            data.get("enabled") is False,
            data)

    data = json_call(
        ledger, cli, ["false-bottom-set", "1"], env,
        "false-bottom set")
    if data is not None:
        ledger.check("false bottom set on", data == 1, data)

    reloaded = json_call(
        ledger, cli, ["false-bottom"], env,
        "false-bottom reload")
    if reloaded is not None:
        ledger.check(
            "false bottom persisted",
            reloaded.get("enabled") is True,
            reloaded)

    env = make_env(work / "false-bottom-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "falseBottom": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "false-bottom alias")
    if data is not None:
        ledger.check(
            "false bottom alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["false-bottom"], env,
        "false-bottom alias read")
    if reloaded is not None:
        ledger.check(
            "false bottom alias value",
            reloaded.get("enabled") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_decoration(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "decoration-defaults")
    data = json_call(
        ledger, cli, ["decoration"], env,
        "decoration defaults")
    if data is not None:
        ledger.check("decoration default off", data.get("value") == 0, data)

    data = json_call(
        ledger, cli, ["decoration-set", "1"], env,
        "decoration set")
    if data is not None:
        ledger.check("decoration set snow", data == 1, data)

    reloaded = json_call(
        ledger, cli, ["decoration"], env,
        "decoration reload")
    if reloaded is not None:
        ledger.check(
            "decoration persisted",
            reloaded.get("value") == 1,
            reloaded)

    env = make_env(work / "decoration-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "actionBarDecoration": 2,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "decoration alias")
    if data is not None:
        ledger.check(
            "decoration alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["decoration"], env,
        "decoration alias read")
    if reloaded is not None:
        ledger.check(
            "decoration alias value",
            reloaded.get("value") == 2,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_shortcuts(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "shortcuts-defaults")
    data = json_call(ledger, cli, ["shortcuts"], env, "shortcuts defaults")
    if data is not None:
        ledger.check(
            "shortcuts default off",
            data == {
                "administrators": False,
                "recentActions": False,
                "statistics": False,
                "permissions": False,
                "members": False,
            },
            data)

    data = json_call(
        ledger, cli, ["shortcuts-set", "1", "1", "1", "1", "1"], env,
        "shortcuts set")
    if data is not None:
        ledger.check(
            "shortcuts set all on",
            all(data.get(key) is True for key in (
                "administrators", "recentActions", "statistics",
                "permissions", "members")),
            data)

    reloaded = json_call(
        ledger, cli, ["shortcuts"], env, "shortcuts reload")
    if reloaded is not None:
        ledger.check(
            "shortcuts persisted",
            reloaded.get("administrators") is True
            and reloaded.get("members") is True,
            reloaded)

    env = make_env(work / "shortcuts-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "ChannelAdministrators": True,
            "EventLog": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "shortcuts alias")
    if data is not None:
        ledger.check(
            "shortcuts alias applied",
            data.get("applied") == 2 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["shortcuts"], env, "shortcuts alias read")
    if reloaded is not None:
        ledger.check(
            "shortcuts alias value",
            reloaded.get("administrators") is True
            and reloaded.get("recentActions") is True
            and reloaded.get("statistics") is False,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_message_menu_style(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "menu-style-defaults")
    data = json_call(
        ledger, cli, ["message-menu-style"], env,
        "menu-style defaults")
    if data is not None:
        ledger.check(
            "menu style default off",
            data.get("grouped") is False
            and data.get("defaultDelete") is False,
            data)

    data = json_call(
        ledger, cli, ["message-menu-style-set", "1", "1"], env,
        "menu-style set")
    if data is not None:
        ledger.check(
            "menu style set on",
            data.get("grouped") is True
            and data.get("defaultDelete") is True,
            data)

    reloaded = json_call(
        ledger, cli, ["message-menu-style"], env,
        "menu-style reload")
    if reloaded is not None:
        ledger.check(
            "menu style persisted",
            reloaded.get("grouped") is True
            and reloaded.get("defaultDelete") is True,
            reloaded)

    env = make_env(work / "menu-style-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "groupMessageMenu": True,
            "DefaultDeleteMenu": 2,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "menu-style alias")
    if data is not None:
        ledger.check(
            "menu style alias applied",
            data.get("applied") == 2 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["message-menu-style"], env,
        "menu-style alias read")
    if reloaded is not None:
        ledger.check(
            "menu style alias value",
            reloaded.get("grouped") is True
            and reloaded.get("defaultDelete") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_chat_misc(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "chat-misc-defaults")
    data = json_call(
        ledger, cli, ["greeting-sticker"], env,
        "greeting sticker defaults")
    if data is not None:
        ledger.check(
            "greeting sticker default shown",
            data.get("hidden") is False,
            data)
    data = json_call(
        ledger, cli, ["panels-on-scroll"], env,
        "panels on scroll defaults")
    if data is not None:
        ledger.check(
            "panels on scroll default shown",
            data.get("hidden") is False,
            data)
    data = json_call(
        ledger, cli, ["channel-bottom-button"], env,
        "channel bottom button defaults")
    if data is not None:
        ledger.check(
            "channel bottom button default mute",
            data.get("mode") == 1,
            data)

    data = json_call(
        ledger, cli, ["greeting-sticker-set", "1"], env,
        "greeting sticker set")
    if data is not None:
        ledger.check(
            "greeting sticker set hidden",
            data.get("hidden") is True,
            data)
    data = json_call(
        ledger, cli, ["panels-on-scroll-set", "1"], env,
        "panels on scroll set")
    if data is not None:
        ledger.check(
            "panels on scroll set hidden",
            data.get("hidden") is True,
            data)
    data = json_call(
        ledger, cli, ["channel-bottom-button-set", "2"], env,
        "channel bottom button set")
    if data is not None:
        ledger.check(
            "channel bottom button set discuss",
            data.get("mode") == 2,
            data)

    reloaded = json_call(
        ledger, cli, ["greeting-sticker"], env,
        "greeting sticker reload")
    if reloaded is not None:
        ledger.check(
            "greeting sticker persisted",
            reloaded.get("hidden") is True,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["panels-on-scroll"], env,
        "panels on scroll reload")
    if reloaded is not None:
        ledger.check(
            "panels on scroll persisted",
            reloaded.get("hidden") is True,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["channel-bottom-button"], env,
        "channel bottom button reload")
    if reloaded is not None:
        ledger.check(
            "channel bottom button persisted",
            reloaded.get("mode") == 2,
            reloaded)

    env = make_env(work / "chat-misc-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "disableGreetingSticker": True,
            "hideKeyboardOnScroll": True,
            "bottomButton": 2,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "chat misc alias")
    if data is not None:
        ledger.check(
            "chat misc alias applied",
            data.get("applied") == 3 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["greeting-sticker"], env,
        "greeting sticker alias read")
    if reloaded is not None:
        ledger.check(
            "greeting sticker alias value",
            reloaded.get("hidden") is True,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["panels-on-scroll"], env,
        "panels on scroll alias read")
    if reloaded is not None:
        ledger.check(
            "panels on scroll alias value",
            reloaded.get("hidden") is True,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["channel-bottom-button"], env,
        "channel bottom button alias read")
    if reloaded is not None:
        ledger.check(
            "channel bottom button alias value",
            reloaded.get("mode") == 2,
            reloaded)

    env = make_env(work / "chat-misc-legacy")
    root = {
        "version": 1,
        "exteraconfig": {
            "DisableChannelMuteButton": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "chat misc legacy")
    if data is not None:
        ledger.check(
            "chat misc legacy applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["channel-bottom-button"], env,
        "channel bottom button legacy read")
    if reloaded is not None:
        ledger.check(
            "legacy mute button hides bottom button",
            reloaded.get("mode") == 0,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_subtitles(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    def parse(name):
        path = FIXTURES / name
        result = run(cli, ["subtitles-parse", str(path)])
        if result.returncode != 0:
            ledger.check(
                f"subtitles parse {name}",
                False,
                f"exit {result.returncode}, "
                f"stderr {result.stderr.strip()!r}")
            return None
        try:
            payload = json.loads(result.stdout)
        except json.JSONDecodeError as error:
            ledger.check(f"subtitles parse {name}", False, f"json: {error}")
            return None
        cues = payload.get("cues")
        if not isinstance(cues, list):
            ledger.check(f"subtitles parse {name}", False, payload)
            return None
        ledger.check(f"subtitles parse {name}", True)
        return cues

    def active(name, position):
        result = run(
            cli, ["subtitles-active", str(FIXTURES / name), str(position)])
        if result.returncode != 0:
            ledger.check(
                f"subtitles active {name} @{position}",
                False,
                f"exit {result.returncode}, "
                f"stderr {result.stderr.strip()!r}")
            return None
        try:
            text = json.loads(result.stdout).get("text")
        except json.JSONDecodeError as error:
            ledger.check(
                f"subtitles active {name} @{position}",
                False,
                f"json: {error}")
            return None
        ledger.check(
            f"subtitles active {name} @{position}",
            isinstance(text, str),
            text)
        return text if isinstance(text, str) else None

    cues = parse("basic.srt")
    if cues is not None:
        ledger.check("basic cue count", len(cues) == 3, len(cues))
        bounds = [(cue.get("start"), cue.get("end")) for cue in cues]
        ledger.check(
            "basic cue times",
            bounds == [(1000, 3500), (5000, 7000), (9250, 11000)],
            bounds)
        text = cues[0].get("text") if cues else None
        ledger.check(
            "basic multiline text",
            text == "First line\nSecond line",
            repr(text))
        ledger.check(
            "basic crlf normalized",
            all("\r" not in str(cue.get("text")) for cue in cues),
            [cue.get("text") for cue in cues])

    text = active("basic.srt", 1000)
    ledger.check(
        "basic active at cue start",
        text == "First line\nSecond line",
        repr(text))
    text = active("basic.srt", 9250)
    ledger.check("basic active at last start", text == "Last cue", repr(text))
    text = active("basic.srt", 999)
    ledger.check("basic inactive before first cue", text == "", repr(text))
    gap_mid = (3500 + 5000) // 2
    text = active("basic.srt", gap_mid)
    ledger.check(
        "basic inactive between cues",
        text == "",
        repr(text))

    cues = parse("tags.srt")
    if cues is not None:
        texts = [cue.get("text") for cue in cues]
        ledger.check("tags cue count", len(cues) == 2, len(cues))
        ledger.check(
            "tags markup stripped",
            texts == ["Italic and bold", "Top positioned"],
            texts)

    cues = parse("bom-utf16.srt")
    if cues is not None:
        texts = [cue.get("text") for cue in cues]
        ledger.check("bom cue count", len(cues) == 2, len(cues))
        ledger.check(
            "bom utf-16 decoded",
            texts == ["UTF-16 cue one", "UTF-16 cue two"],
            texts)
        ledger.check(
            "bom marker dropped",
            all("\ufeff" not in str(text) for text in texts),
            texts)

    cues = parse("basic.vtt")
    if cues is not None:
        texts = [cue.get("text") for cue in cues]
        ledger.check("vtt cue count", len(cues) == 2, len(cues))
        ledger.check(
            "vtt header and note skipped",
            texts == ["First vtt cue", "Second vtt cue"],
            texts)
        bounds = [(cue.get("start"), cue.get("end")) for cue in cues]
        ledger.check(
            "vtt cue times",
            bounds == [(1000, 3000), (5000, 6500)],
            bounds)

    cues = parse("overlap.srt")
    if cues is not None:
        bounds = [(cue.get("start"), cue.get("end")) for cue in cues]
        ledger.check("overlap cue count", len(cues) == 2, len(cues))
        ledger.check(
            "overlap cue times",
            bounds == [(1000, 5000), (3000, 7000)],
            bounds)
        text = active("overlap.srt", 4000)
        if text is not None:
            first = text.find("First overlapping")
            second = text.find("Second overlapping")
            ledger.check(
                "overlap cues joined in order",
                first >= 0 and second > first,
                repr(text))
    text = active("overlap.srt", 2000)
    ledger.check(
        "overlap first cue alone",
        text is not None
        and "First overlapping" in text
        and "Second overlapping" not in text,
        repr(text))
    text = active("overlap.srt", 6000)
    ledger.check(
        "overlap second cue alone",
        text is not None
        and "Second overlapping" in text
        and "First overlapping" not in text,
        repr(text))

    cues = parse("broken.srt")
    if cues is not None:
        texts = [cue.get("text") for cue in cues]
        ledger.check("broken cue count", len(cues) == 3, len(cues))
        ledger.check(
            "broken bad timing skipped",
            texts == ["Valid one", "Valid two", "No trailing blank line"],
            texts)
        bounds = [(cue.get("start"), cue.get("end")) for cue in cues]
        ledger.check(
            "broken cue times",
            bounds == [(1000, 2000), (5000, 6000), (7000, 8000)],
            bounds)
    text = active("broken.srt", 7000)
    ledger.check(
        "broken last cue without blank line",
        text == "No trailing blank line",
        repr(text))

    result = run(
        cli, ["subtitles-parse", str(FIXTURES / "missing-subtitles.srt")])
    ledger.check(
        "subtitles parse missing file",
        result.returncode == 2 and bool(result.stderr.strip()),
        f"exit {result.returncode}, stderr {result.stderr!r}")

    keys = {}
    for doc_id in ("12345", "67890", "12345"):
        result = run(cli, ["subtitles-key", doc_id])
        if result.returncode != 0:
            ledger.check(
                f"subtitles key {doc_id}",
                False,
                f"exit {result.returncode}, "
                f"stderr {result.stderr.strip()!r}")
            continue
        try:
            key = json.loads(result.stdout).get("key")
        except json.JSONDecodeError as error:
            ledger.check(f"subtitles key {doc_id}", False, f"json: {error}")
            continue
        ledger.check(
            f"subtitles key {doc_id}",
            isinstance(key, str) and bool(key) and doc_id in key,
            repr(key))
        if isinstance(key, str) and key:
            ledger.check(
                f"subtitles key {doc_id} filename safe",
                all(ch not in key for ch in "/\\:*?\"<>| \t\n"),
                repr(key))
            keys.setdefault(doc_id, set()).add(key)
    ledger.check(
        "subtitles key stable",
        all(len(values) == 1 for values in keys.values()),
        keys)
    representatives = {
        doc_id: next(iter(values))
        for doc_id, values in keys.items() if values
    }
    if len(representatives) == 2:
        ledger.check(
            "subtitles key distinct per document",
            len(set(representatives.values())) == 2,
            representatives)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_lyrics(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    def parse(name):
        result = run(cli, ["lyrics-parse", str(FIXTURES / name)])
        if result.returncode != 0:
            ledger.check(
                f"lyrics parse {name}",
                False,
                f"exit {result.returncode}, "
                f"stderr {result.stderr.strip()!r}")
            return None
        try:
            payload = json.loads(result.stdout)
        except json.JSONDecodeError as error:
            ledger.check(f"lyrics parse {name}", False, f"json: {error}")
            return None
        lines = payload.get("lines")
        if (not isinstance(lines, list)
                or not isinstance(payload.get("synced"), bool)):
            ledger.check(f"lyrics parse {name}", False, payload)
            return None
        for line in lines:
            if (not isinstance(line, dict)
                    or not isinstance(line.get("ms"), int)
                    or not isinstance(line.get("text"), str)):
                ledger.check(f"lyrics parse {name}", False, line)
                return None
        ledger.check(f"lyrics parse {name}", True)
        return payload

    def active(name, position):
        result = run(
            cli, ["lyrics-active", str(FIXTURES / name), str(position)])
        if result.returncode != 0:
            ledger.check(
                f"lyrics active {name} @{position}",
                False,
                f"exit {result.returncode}, "
                f"stderr {result.stderr.strip()!r}")
            return None
        try:
            payload = json.loads(result.stdout)
        except json.JSONDecodeError as error:
            ledger.check(
                f"lyrics active {name} @{position}",
                False,
                f"json: {error}")
            return None
        index = payload.get("index")
        text = payload.get("text")
        if not isinstance(index, int) or not isinstance(text, str):
            ledger.check(
                f"lyrics active {name} @{position}", False, payload)
            return None
        return index, text

    payload = parse("basic.lrc")
    if payload is not None:
        lines = payload["lines"]
        ledger.check("basic lyrics synced", payload["synced"] is True)
        ledger.check("basic lyrics line count", len(lines) == 4, len(lines))
        times = [line["ms"] for line in lines]
        ledger.check(
            "basic lyrics offset applied",
            times == [2000, 6000, 10250, 31000],
            times)
        ledger.check(
            "basic lyrics sorted",
            all(first <= second for first, second in zip(times, times[1:])),
            times)
        texts = [line["text"] for line in lines]
        ledger.check(
            "basic lyrics duplicated multi timestamp",
            texts == ["First line", "Second line", "Last cue", "Last cue"],
            texts)
        ledger.check(
            "basic lyrics tags stripped",
            all("[" not in text and "]" not in text for text in texts),
            texts)

    cases = [
        ("before first line", 1999, (-1, "")),
        ("at first line", 2000, (0, "First line")),
        ("inside first line", 5999, (0, "First line")),
        ("at third line", 10250, (2, "Last cue")),
        ("at last line", 31000, (3, "Last cue")),
        ("past end", 10000000, (3, "Last cue")),
    ]
    for name, position, expected in cases:
        current = active("basic.lrc", position)
        ledger.check(
            f"basic lyrics {name}", current == expected, current)

    payload = parse("wordtime.lrc")
    if payload is not None:
        lines = payload["lines"]
        ledger.check("wordtime lyrics synced", payload["synced"] is True)
        texts = [line["text"] for line in lines]
        ledger.check(
            "wordtime tags removed",
            texts == ["Hello world", "Plain line"],
            texts)
        times = [line["ms"] for line in lines]
        ledger.check("wordtime lyrics times", times == [2000, 4500], times)

    payload = parse("unsynced.lrc")
    if payload is not None:
        lines = payload["lines"]
        ledger.check("unsynced lyrics synced false", payload["synced"] is False)
        ledger.check("unsynced lyrics single line", len(lines) == 1, len(lines))
        text = lines[0]["text"] if lines else None
        ledger.check(
            "unsynced lyrics joined text",
            text == "Plain first line\nPlain second line\nPlain third line",
            repr(text))
        start = lines[0]["ms"] if lines else None
        ledger.check("unsynced lyrics start at zero", start == 0, start)
    current = active("unsynced.lrc", 0)
    ledger.check(
        "unsynced lyrics active at zero",
        current is not None and current[0] == 0,
        current)

    payload = parse("bom-utf16.lrc")
    if payload is not None:
        lines = payload["lines"]
        texts = [line["text"] for line in lines]
        ledger.check("bom lyrics line count", len(lines) == 2, len(lines))
        ledger.check(
            "bom lyrics utf-16 decoded",
            texts == ["UTF-16 line one", "UTF-16 line two"],
            texts)
        ledger.check(
            "bom lyrics marker dropped",
            all("\ufeff" not in text for text in texts),
            texts)

    payload = parse("broken.lrc")
    if payload is not None:
        lines = payload["lines"]
        texts = [line["text"] for line in lines]
        ledger.check(
            "broken lyrics skipped",
            texts == ["Valid one", "Valid two", "Valid three"],
            texts)
        times = [line["ms"] for line in lines]
        ledger.check(
            "broken lyrics times",
            times == [1000, 3000, 9000],
            times)
    current = active("broken.lrc", 3000)
    ledger.check(
        "broken lyrics active", current == (1, "Valid two"), current)

    result = run(cli, ["lyrics-parse", str(FIXTURES / "missing.lrc")])
    ledger.check(
        "lyrics parse missing file",
        result.returncode == 2 and bool(result.stderr.strip()),
        f"exit {result.returncode}, stderr {result.stderr!r}")

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_lrclib(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    def parse_search(name):
        result = run(cli, ["lyrics-search-parse", str(FIXTURES / name)])
        if result.returncode != 0:
            ledger.check(
                f"lrclib search parse {name}",
                False,
                f"exit {result.returncode}, "
                f"stderr {result.stderr.strip()!r}")
            return None
        try:
            payload = json.loads(result.stdout)
        except json.JSONDecodeError as error:
            ledger.check(
                f"lrclib search parse {name}", False, f"json: {error}")
            return None
        results = payload.get("results")
        if not isinstance(results, list):
            ledger.check(f"lrclib search parse {name}", False, payload)
            return None
        for item in results:
            if (not isinstance(item, dict)
                    or not isinstance(item.get("id"), int)
                    or not isinstance(item.get("track"), str)
                    or not isinstance(item.get("artist"), str)
                    or not isinstance(item.get("hasSynced"), bool)):
                ledger.check(f"lrclib search parse {name}", False, item)
                return None
        ledger.check(f"lrclib search parse {name}", True)
        return results

    def parse_get(name):
        result = run(cli, ["lyrics-get-parse", str(FIXTURES / name)])
        if result.returncode != 0:
            ledger.check(
                f"lrclib get parse {name}",
                False,
                f"exit {result.returncode}, "
                f"stderr {result.stderr.strip()!r}")
            return None
        try:
            payload = json.loads(result.stdout)
        except json.JSONDecodeError as error:
            ledger.check(f"lrclib get parse {name}", False, f"json: {error}")
            return None
        if (not isinstance(payload.get("ok"), bool)
                or not isinstance(payload.get("synced"), bool)
                or not isinstance(payload.get("text"), str)):
            ledger.check(f"lrclib get parse {name}", False, payload)
            return None
        ledger.check(f"lrclib get parse {name}", True)
        return payload

    search = parse_search("lrclib-search.json")
    if search is not None:
        ledger.check(
            "lrclib search result count", len(search) == 2, len(search))
        first = search[0] if search else {}
        ledger.check(
            "lrclib search first id", first.get("id") == 3396226, first)
        ledger.check(
            "lrclib search first track",
            first.get("track") == "Bohemian Rhapsody",
            first)
        ledger.check(
            "lrclib search first artist",
            first.get("artist") == "Queen",
            first)
        ledger.check(
            "lrclib search first has synced",
            first.get("hasSynced") is True,
            first)
        second = search[1] if len(search) > 1 else {}
        ledger.check(
            "lrclib search second lacks synced",
            second.get("hasSynced") is False,
            second)

    lyrics = parse_get("lrclib-get-synced.json")
    if lyrics is not None:
        ledger.check("lrclib get synced ok", lyrics["ok"] is True, lyrics)
        ledger.check(
            "lrclib get synced flag", lyrics["synced"] is True, lyrics)
        ledger.check(
            "lrclib get prefers synced lyrics",
            lyrics["text"]
            == "[00:01.00] First synced line\n[00:05.00] Second synced line",
            lyrics["text"])

    lyrics = parse_get("lrclib-get-plain.json")
    if lyrics is not None:
        ledger.check("lrclib get plain ok", lyrics["ok"] is True, lyrics)
        ledger.check(
            "lrclib get plain not synced",
            lyrics["synced"] is False,
            lyrics)
        ledger.check(
            "lrclib get plain text",
            lyrics["text"] == "First plain line\nSecond plain line",
            lyrics["text"])

    lyrics = parse_get("lrclib-get-instrumental.json")
    if lyrics is not None:
        ledger.check(
            "lrclib get instrumental not ok",
            lyrics["ok"] is False,
            lyrics)
        ledger.check(
            "lrclib get instrumental empty text",
            lyrics["text"] == "",
            lyrics["text"])

    lyrics = parse_get("lrclib-broken.json")
    if lyrics is not None:
        ledger.check(
            "lrclib get broken not ok", lyrics["ok"] is False, lyrics)
        ledger.check(
            "lrclib get broken empty",
            lyrics["synced"] is False and lyrics["text"] == "",
            lyrics)

    search = parse_search("lrclib-broken.json")
    if search is not None:
        ledger.check("lrclib search broken empty", search == [], search)

    result = run(
        cli, ["lyrics-get-parse", str(FIXTURES / "missing-lrclib.json")])
    ledger.check(
        "lrclib get missing file",
        result.returncode == 2 and bool(result.stderr.strip()),
        f"exit {result.returncode}, stderr {result.stderr!r}")

    env = make_env(work / "lrclib-consent")
    data = json_call(
        ledger, cli,
        ["config-fromjson", json.dumps({
            "version": 1,
            "exteraconfig": {"OELyricsAllowNetwork": True},
        })],
        env, "lrclib consent fromJson")
    if data is not None:
        ledger.check(
            "lrclib consent accepted",
            data.get("accepted") is True,
            data.get("accepted"))
        ledger.check(
            "lrclib consent unknown empty",
            data.get("unknown") == [],
            data.get("unknown"))
    raw = json_call(ledger, cli, ["config-raw"], env, "lrclib consent raw")
    if raw is not None:
        values = raw.get("exteraconfig") or {}
        ledger.check(
            "lrclib consent stored",
            values.get("OELyricsAllowNetwork") is True,
            values.get("OELyricsAllowNetwork"))

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_extra_pills(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    def parse(domain, path, code=None):
        arguments = ["extra-pills-parse", domain, str(path)]
        if code is not None:
            arguments.append(code)
        result = run(cli, arguments)
        if result.returncode != 0:
            ledger.check(
                f"extra pills {domain} {path.name}",
                False,
                f"exit {result.returncode}, "
                f"stderr {result.stderr.strip()!r}")
            return None
        try:
            payload = json.loads(result.stdout)
        except json.JSONDecodeError as error:
            ledger.check(
                f"extra pills {domain} {path.name}",
                False,
                f"json: {error}")
            return None
        text = payload.get("text")
        if not isinstance(text, str):
            ledger.check(
                f"extra pills {domain} {path.name}", False, payload)
            return None
        ledger.check(f"extra pills {domain} {path.name}", True)
        return text

    weather = parse("weather", FIXTURES / "extra-pills-weather.json")
    if weather is not None:
        ledger.check(
            "extra pills weather positive",
            weather == "+23\u00b0",
            weather)
    cold = parse("weather", FIXTURES / "extra-pills-weather-negative.json")
    if cold is not None:
        ledger.check(
            "extra pills weather negative",
            cold == "-6\u00b0",
            cold)

    coin = parse("coinbase", FIXTURES / "extra-pills-coinbase.json")
    if coin is not None:
        ledger.check("extra pills coinbase spot", coin == "75789", coin)
    small = parse("coinbase", FIXTURES / "extra-pills-coinbase-small.json")
    if small is not None:
        ledger.check("extra pills coinbase small", small == "0.52", small)

    gold = parse("gold", FIXTURES / "extra-pills-gold.json")
    if gold is not None:
        ledger.check("extra pills gold price", gold == "4072", gold)

    for domain, name in [
            ("weather", "extra-pills-broken.json"),
            ("coinbase", "extra-pills-broken.json"),
            ("gold", "extra-pills-broken.json"),
            ("weather", "extra-pills-empty.json"),
            ("coinbase", "extra-pills-empty.json"),
            ("gold", "extra-pills-empty.json")]:
        text = parse(domain, FIXTURES / name)
        if text is not None:
            ledger.check(
                f"extra pills {domain} {name} empty", text == "", text)

    ecb = work / "extra-pills-ecb"
    ecb.mkdir(parents=True, exist_ok=True)
    envelope = (
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<gesmes:Envelope"
        " xmlns:gesmes='http://www.gesmes.org/xml/2002-08-01'"
        " xmlns='http://www.ecb.int/vocabulary/2002-08-01/eurofxref'>\n"
        "\t<Cube>\n"
        "\t\t<Cube time='2026-11-06'>\n"
        "%s"
        "\t\t</Cube>\n"
        "\t</Cube>\n"
        "</gesmes:Envelope>\n")
    with_rub = ecb / "with-rub.xml"
    with_rub.write_text(
        envelope % (
            "\t\t\t<Cube currency='USD' rate='1.1165'/>\n"
            "\t\t\t<Cube currency='RUB' rate='101.5'/>\n"),
        encoding="utf-8")
    no_rub = ecb / "no-rub.xml"
    no_rub.write_text(
        envelope % (
            "\t\t\t<Cube currency='USD' rate='1.1165'/>\n"
            "\t\t\t<Cube currency='JPY' rate='176.77'/>\n"),
        encoding="utf-8")
    broken_xml = ecb / "broken.xml"
    broken_xml.write_text(
        "<Cube currency='USD' rate='1.1165'>", encoding="utf-8")

    usd = parse("ecb", with_rub, "USD")
    if usd is not None:
        ledger.check("extra pills ecb usd cross", usd == "90.91", usd)
    eur = parse("ecb", with_rub, "EUR")
    if eur is not None:
        ledger.check("extra pills ecb eur rate", eur == "101.5", eur)
    default_usd = parse("ecb", with_rub)
    if default_usd is not None:
        ledger.check(
            "extra pills ecb default code usd",
            default_usd == "90.91",
            default_usd)
    missing_rub = parse("ecb", no_rub, "USD")
    if missing_rub is not None:
        ledger.check(
            "extra pills ecb without rub empty",
            missing_rub == "",
            missing_rub)
    invalid_xml = parse("ecb", broken_xml, "USD")
    if invalid_xml is not None:
        ledger.check(
            "extra pills ecb broken empty",
            invalid_xml == "",
            invalid_xml)

    cbr = work / "extra-pills-cbr"
    cbr.mkdir(parents=True, exist_ok=True)
    cbr_nominal = cbr / "nominal.xml"
    cbr_nominal.write_text(
        "<?xml version='1.0' encoding='windows-1251'?>\n"
        "<ValCurs Date='07.11.2026' name='Foreign Currency Market'>\n"
        "<Valute><CharCode>USD</CharCode><Nominal>2</Nominal>"
        "<Value>181,8200</Value></Valute>\n"
        "<Valute><CharCode>EUR</CharCode><Nominal>1</Nominal>"
        "<Value>98,5000</Value></Valute>\n"
        "</ValCurs>\n",
        encoding="cp1251")
    cbr_without_eur = cbr / "no-eur.xml"
    cbr_without_eur.write_text(
        "<ValCurs>"
        "<Valute><CharCode>USD</CharCode><Nominal>1</Nominal>"
        "<Value>90,9100</Value></Valute>"
        "<Valute><CharCode>JPY</CharCode><Nominal>100</Nominal>"
        "<Value>5930,0000</Value></Valute>"
        "</ValCurs>\n",
        encoding="cp1251")
    cbr_broken = cbr / "broken.xml"
    cbr_broken.write_text(
        "<Valute><CharCode>USD</CharCode>", encoding="cp1251")

    cbr_usd = parse("cbr", FIXTURES / "extra-pills-cbr.xml")
    if cbr_usd is not None:
        ledger.check("extra pills cbr usd rate", cbr_usd == "90.91", cbr_usd)
    cbr_eur = parse("cbr", FIXTURES / "extra-pills-cbr.xml", "EUR")
    if cbr_eur is not None:
        ledger.check("extra pills cbr eur rate", cbr_eur == "98.5", cbr_eur)
    cbr_divided = parse("cbr", cbr_nominal)
    if cbr_divided is not None:
        ledger.check(
            "extra pills cbr nominal division",
            cbr_divided == "90.91",
            cbr_divided)
    cbr_partial = parse("cbr", cbr_without_eur)
    if cbr_partial is not None:
        ledger.check(
            "extra pills cbr without eur empty",
            cbr_partial == "",
            cbr_partial)
    cbr_unknown = parse("cbr", FIXTURES / "extra-pills-cbr.xml", "GBP")
    if cbr_unknown is not None:
        ledger.check(
            "extra pills cbr unknown code empty",
            cbr_unknown == "",
            cbr_unknown)
    cbr_invalid = parse("cbr", cbr_broken)
    if cbr_invalid is not None:
        ledger.check(
            "extra pills cbr broken empty",
            cbr_invalid == "",
            cbr_invalid)

    result = run(
        cli,
        ["extra-pills-parse", "weather", str(FIXTURES / "pills-missing.json")])
    ledger.check(
        "extra pills missing file",
        result.returncode == 2 and bool(result.stderr.strip()),
        f"exit {result.returncode}, stderr {result.stderr!r}")
    result = run(
        cli,
        ["extra-pills-parse", "bogus",
         str(FIXTURES / "extra-pills-weather.json")])
    ledger.check(
        "extra pills unknown domain",
        result.returncode == 2 and bool(result.stderr.strip()),
        f"exit {result.returncode}, stderr {result.stderr!r}")

    env = make_env(work / "extra-pills-mask-full")
    data = json_call(
        ledger, cli,
        ["config-fromjson", json.dumps({
            "version": 1,
            "exteraconfig": {"OEPillStackPills": 0x3FFF},
        })],
        env, "extra pills mask config")
    if data is not None:
        ledger.check(
            "extra pills mask accepted",
            data.get("accepted") is True,
            data.get("accepted"))
    exported = json_call(
        ledger, cli, ["backup-export-current"], env,
        "extra pills mask export")
    if exported is not None:
        pills = exported.get("pillstackconfig") or {}
        ledger.check(
            "extra pills mask roundtrip",
            pills.get("OEPillStackPills") == 0x3FFF,
            pills)

    subset = 0x100 | 0x200 | 0x800 | 0x2000
    env = make_env(work / "extra-pills-mask-subset")
    json_call(
        ledger, cli,
        ["config-fromjson", json.dumps({
            "version": 1,
            "exteraconfig": {"OEPillStackPills": subset},
        })],
        env, "extra pills mask subset config")
    exported = json_call(
        ledger, cli, ["backup-export-current"], env,
        "extra pills mask subset export")
    if exported is not None:
        pills = exported.get("pillstackconfig") or {}
        ledger.check(
            "extra pills mask subset roundtrip",
            pills.get("OEPillStackPills") == subset,
            pills)

    env = make_env(work / "extra-pills-mask-import")
    data = json_call(
        ledger, cli,
        ["backup-import-json", json.dumps({
            "version": 1,
            "pillstackconfig": {"OEPillStackPills": 0x3FFF},
        })],
        env, "extra pills mask import")
    if data is not None:
        ledger.check(
            "extra pills mask import applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    env = make_env(work / "extra-pills-mask-overflow")
    data = json_call(
        ledger, cli,
        ["backup-import-json", json.dumps({
            "version": 1,
            "pillstackconfig": {"OEPillStackPills": 0x4000},
        })],
        env, "extra pills mask overflow")
    if data is not None:
        ledger.check(
            "extra pills mask overflow rejected",
            data.get("applied") == 0
            and sorted(data.get("unknown") or [])
            == ["pillstackconfig.OEPillStackPills"],
            data)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_download_boost(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "download-boost-defaults")
    data = json_call(
        ledger, cli, ["download-boost"], env,
        "download boost defaults")
    if data is not None:
        ledger.check(
            "download boost default off",
            data.get("mode") == 0,
            data)
    data = json_call(
        ledger, cli, ["upload-boost"], env,
        "upload boost defaults")
    if data is not None:
        ledger.check(
            "upload boost default off",
            data.get("enabled") is False,
            data)

    data = json_call(
        ledger, cli, ["download-boost-set", "2"], env,
        "download boost set")
    if data is not None:
        ledger.check(
            "download boost set ultra",
            data.get("mode") == 2,
            data)
    data = json_call(
        ledger, cli, ["upload-boost-set", "1"], env,
        "upload boost set")
    if data is not None:
        ledger.check("upload boost set on", data == 1, data)

    reloaded = json_call(
        ledger, cli, ["download-boost"], env,
        "download boost reload")
    if reloaded is not None:
        ledger.check(
            "download boost persisted",
            reloaded.get("mode") == 2,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["upload-boost"], env,
        "upload boost reload")
    if reloaded is not None:
        ledger.check(
            "upload boost persisted",
            reloaded.get("enabled") is True,
            reloaded)

    data = json_call(
        ledger, cli, ["download-boost-set", "5"], env,
        "download boost clamp")
    if data is not None:
        ledger.check(
            "download boost clamped to ultra",
            data.get("mode") == 2,
            data)

    env = make_env(work / "download-boost-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "downloadSpeedBoost": 2,
            "uploadBoost": True,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "download boost alias")
    if data is not None:
        ledger.check(
            "download boost alias applied",
            data.get("applied") == 2 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["download-boost"], env,
        "download boost alias read")
    if reloaded is not None:
        ledger.check(
            "download boost alias value",
            reloaded.get("mode") == 2,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["upload-boost"], env,
        "upload boost alias read")
    if reloaded is not None:
        ledger.check(
            "upload boost alias value",
            reloaded.get("enabled") is True,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_media_quality(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "media-quality-defaults")
    data = json_call(
        ledger, cli, ["media-quality"], env,
        "media quality defaults")
    if data is not None:
        ledger.check(
            "media quality default auto",
            data.get("quality") == 0,
            data)
    data = json_call(
        ledger, cli, ["hw-accel"], env,
        "hw accel defaults")
    if data is not None:
        ledger.check(
            "hw accel default on",
            data.get("enabled") is True,
            data)
    data = json_call(
        ledger, cli, ["gif-as-video"], env,
        "gif as video defaults")
    if data is not None:
        ledger.check(
            "gif as video default off",
            data.get("enabled") is False,
            data)
    data = json_call(
        ledger, cli, ["audio-bitrate"], env,
        "audio bitrate defaults")
    if data is not None:
        ledger.check(
            "audio bitrate default 32",
            data.get("bitrate") == 32,
            data)

    data = json_call(
        ledger, cli, ["media-quality-set", "2"], env,
        "media quality set")
    if data is not None:
        ledger.check(
            "media quality set 1080p",
            data.get("quality") == 2,
            data)
    data = json_call(
        ledger, cli, ["hw-accel-set", "0"], env,
        "hw accel set")
    if data is not None:
        ledger.check(
            "hw accel set off",
            data.get("enabled") is False,
            data)
    data = json_call(
        ledger, cli, ["gif-as-video-set", "1"], env,
        "gif as video set")
    if data is not None:
        ledger.check(
            "gif as video set on",
            data.get("enabled") is True,
            data)
    data = json_call(
        ledger, cli, ["audio-bitrate-set", "24"], env,
        "audio bitrate set")
    if data is not None:
        ledger.check(
            "audio bitrate set 24",
            data.get("bitrate") == 24,
            data)

    reloaded = json_call(
        ledger, cli, ["media-quality"], env,
        "media quality reload")
    if reloaded is not None:
        ledger.check(
            "media quality persisted",
            reloaded.get("quality") == 2,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["hw-accel"], env,
        "hw accel reload")
    if reloaded is not None:
        ledger.check(
            "hw accel persisted",
            reloaded.get("enabled") is False,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["gif-as-video"], env,
        "gif as video reload")
    if reloaded is not None:
        ledger.check(
            "gif as video persisted",
            reloaded.get("enabled") is True,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["audio-bitrate"], env,
        "audio bitrate reload")
    if reloaded is not None:
        ledger.check(
            "audio bitrate persisted",
            reloaded.get("bitrate") == 24,
            reloaded)

    data = json_call(
        ledger, cli, ["media-quality-set", "9"], env,
        "media quality clamp")
    if data is not None:
        ledger.check(
            "media quality clamped to 1080p",
            data.get("quality") == 2,
            data)
    data = json_call(
        ledger, cli, ["audio-bitrate-set", "99"], env,
        "audio bitrate clamp")
    if data is not None:
        ledger.check(
            "audio bitrate sanitized to 32",
            data.get("bitrate") == 32,
            data)

    env = make_env(work / "media-quality-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "defaultHlsVideoQuality": 1,
            "playerDecoder": False,
            "takeGIFasVideo": True,
            "customAudioBitrate": 48,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "media quality alias")
    if data is not None:
        ledger.check(
            "media quality alias applied",
            data.get("applied") == 4 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["media-quality"], env,
        "media quality alias read")
    if reloaded is not None:
        ledger.check(
            "media quality alias value",
            reloaded.get("quality") == 1,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["hw-accel"], env,
        "hw accel alias read")
    if reloaded is not None:
        ledger.check(
            "hw accel alias value",
            reloaded.get("enabled") is False,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["gif-as-video"], env,
        "gif as video alias read")
    if reloaded is not None:
        ledger.check(
            "gif as video alias value",
            reloaded.get("enabled") is True,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["audio-bitrate"], env,
        "audio bitrate alias read")
    if reloaded is not None:
        ledger.check(
            "audio bitrate alias value",
            reloaded.get("bitrate") == 48,
            reloaded)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_update_channel(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "update-channel-defaults")
    data = json_call(
        ledger, cli, ["update-channel"], env, "update channel defaults")
    if data is not None:
        ledger.check(
            "update channel default release",
            data.get("channel") == 1,
            data)

    data = json_call(
        ledger, cli, ["update-channel-set", "2"], env,
        "update channel set beta")
    if data is not None:
        ledger.check(
            "update channel set beta",
            data.get("channel") == 2,
            data)
    reloaded = json_call(
        ledger, cli, ["update-channel"], env, "update channel reload")
    if reloaded is not None:
        ledger.check(
            "update channel persisted",
            reloaded.get("channel") == 2,
            reloaded)

    data = json_call(
        ledger, cli, ["update-channel-set", "0"], env,
        "update channel set off")
    if data is not None:
        ledger.check(
            "update channel set off",
            data.get("channel") == 0,
            data)
    data = json_call(
        ledger, cli, ["update-channel-set", "9"], env,
        "update channel clamp high")
    if data is not None:
        ledger.check(
            "update channel clamped to beta",
            data.get("channel") == 2,
            data)
    data = json_call(
        ledger, cli, ["update-channel-set", "-3"], env,
        "update channel clamp low")
    if data is not None:
        ledger.check(
            "update channel clamped to off",
            data.get("channel") == 0,
            data)

    env = make_env(work / "update-channel-alias")
    root = {
        "version": 1,
        "exteraconfig": {"autoUpdateChannel": 2},
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "update channel alias")
    if data is not None:
        ledger.check(
            "update channel alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["update-channel"], env, "update channel alias read")
    if reloaded is not None:
        ledger.check(
            "update channel alias value",
            reloaded.get("channel") == 2,
            reloaded)

    def release(tag, url="", prerelease=False, draft=False):
        item = {
            "tag_name": tag,
            "prerelease": prerelease,
            "draft": draft,
        }
        if url:
            item["html_url"] = url
        return item

    data = json_call(
        ledger, cli, [
            "update-check",
            json.dumps([release("v2.3.0", "https://example.invalid/new")]),
            "2.2.0",
            "0",
        ], env, "update check newer")
    if data is not None:
        ledger.check(
            "update check newer available",
            data.get("available") is True
            and data.get("version") == "v2.3.0"
            and data.get("url") == "https://example.invalid/new",
            data)

    data = json_call(
        ledger, cli, [
            "update-check",
            json.dumps([release("v2.1.0", "https://example.invalid/old")]),
            "2.2.0",
            "0",
        ], env, "update check older")
    if data is not None:
        ledger.check(
            "update check older unavailable",
            data.get("available") is False and data.get("version") == "",
            data)

    data = json_call(
        ledger, cli, ["update-check", "[]", "2.2.0", "0"], env,
        "update check empty")
    if data is not None:
        ledger.check(
            "update check empty unavailable",
            data.get("available") is False and data.get("version") == "",
            data)

    data = json_call(
        ledger, cli, ["update-check", "not json", "2.2.0", "0"], env,
        "update check invalid json")
    if data is not None:
        ledger.check(
            "update check invalid json unavailable",
            data.get("available") is False,
            data)

    beta = json.dumps([
        release("v3.0.0-beta.1", "https://example.invalid/beta",
                prerelease=True),
    ])
    data = json_call(
        ledger, cli, ["update-check", beta, "2.2.0", "0"], env,
        "update check stable filter")
    if data is not None:
        ledger.check(
            "update check skips prerelease",
            data.get("available") is False,
            data)
    data = json_call(
        ledger, cli, ["update-check", beta, "2.2.0", "1"], env,
        "update check prerelease allowed")
    if data is not None:
        ledger.check(
            "update check accepts prerelease",
            data.get("available") is True
            and data.get("version") == "v3.0.0-beta.1",
            data)

    draft = json.dumps([
        release("v9.9.9", "https://example.invalid/draft", draft=True),
    ])
    data = json_call(
        ledger, cli, ["update-check", draft, "2.2.0", "1"], env,
        "update check draft")
    if data is not None:
        ledger.check(
            "update check skips draft",
            data.get("available") is False,
            data)

    mixed = json.dumps([
        release("v9.9.9", "https://example.invalid/draft", draft=True),
        release("v3.0.0-beta.1", "https://example.invalid/beta",
                prerelease=True),
        release("v2.5.0", "https://example.invalid/stable"),
    ])
    data = json_call(
        ledger, cli, ["update-check", mixed, "2.2.0", "0"], env,
        "update check mixed")
    if data is not None:
        ledger.check(
            "update check picks newest stable",
            data.get("available") is True
            and data.get("version") == "v2.5.0"
            and data.get("url") == "https://example.invalid/stable",
            data)

    for name, arguments in [
            ("update channel set without value", ["update-channel-set"]),
            ("update channel set non numeric",
             ["update-channel-set", "beta"]),
            ("update check with wrong count",
             ["update-check", "[]", "2.2.0"])]:
        result = run(cli, arguments, env=env)
        ledger.check(
            f"cli {name}",
            result.returncode == 2 and bool(result.stderr.strip()),
            f"exit {result.returncode}, stderr {result.stderr!r}")

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_translation(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "translation-defaults")
    data = json_call(
        ledger, cli, ["translation-provider"], env,
        "translation provider defaults")
    if data is not None:
        ledger.check(
            "translation provider default auto",
            data.get("provider") == 0,
            data)

    data = json_call(
        ledger, cli, ["translation-formality"], env,
        "translation formality defaults")
    if data is not None:
        ledger.check(
            "translation formality default",
            data.get("formality") == 0,
            data)

    data = json_call(
        ledger, cli, ["translation-keys"], env,
        "translation keys defaults")
    if data is not None:
        ledger.check(
            "translation keys default empty",
            data.get("google") == "" and data.get("deepl") == "",
            data)

    data = json_call(
        ledger, cli, ["translation-provider-set", "2"], env,
        "translation provider set deepl")
    if data is not None:
        ledger.check(
            "translation provider set deepl",
            data.get("provider") == 2,
            data)
    reloaded = json_call(
        ledger, cli, ["translation-provider"], env,
        "translation provider reload")
    if reloaded is not None:
        ledger.check(
            "translation provider persisted",
            reloaded.get("provider") == 2,
            reloaded)

    data = json_call(
        ledger, cli, ["translation-provider-set", "9"], env,
        "translation provider clamp high")
    if data is not None:
        ledger.check(
            "translation provider clamped high",
            data.get("provider") == 3,
            data)
    data = json_call(
        ledger, cli, ["translation-provider-set", "-3"], env,
        "translation provider clamp low")
    if data is not None:
        ledger.check(
            "translation provider clamped low",
            data.get("provider") == 0,
            data)
    data = json_call(
        ledger, cli, ["translation-provider-set", "3"], env,
        "translation provider set llm")
    if data is not None:
        ledger.check(
            "translation provider accepts 3",
            data.get("provider") == 3,
            data)

    data = json_call(
        ledger, cli, ["translation-formality-set", "2"], env,
        "translation formality set less")
    if data is not None:
        ledger.check(
            "translation formality set less",
            data.get("formality") == 2,
            data)
    reloaded = json_call(
        ledger, cli, ["translation-formality"], env,
        "translation formality reload")
    if reloaded is not None:
        ledger.check(
            "translation formality persisted",
            reloaded.get("formality") == 2,
            reloaded)
    data = json_call(
        ledger, cli, ["translation-formality-set", "5"], env,
        "translation formality clamp high")
    if data is not None:
        ledger.check(
            "translation formality clamped high",
            data.get("formality") == 2,
            data)
    data = json_call(
        ledger, cli, ["translation-formality-set", "-1"], env,
        "translation formality clamp low")
    if data is not None:
        ledger.check(
            "translation formality clamped low",
            data.get("formality") == 0,
            data)

    google_key = "AIzaSyA-bcdefghijklmnopqrstuvwxyz0123456"
    deepl_key = "abcd-1234-efgh-5678:fx"
    data = json_call(
        ledger, cli, ["translation-keys-set", "google", google_key], env,
        "translation keys set google")
    if data is not None:
        ledger.check(
            "translation google key masked",
            data.get("google") == "AIza****",
            data)
        ledger.check(
            "translation google key not leaked",
            google_key not in json.dumps(data),
            data)
    data = json_call(
        ledger, cli, ["translation-keys-set", "deepl", deepl_key], env,
        "translation keys set deepl")
    if data is not None:
        ledger.check(
            "translation deepl key masked",
            data.get("deepl") == "abcd****",
            data)
        ledger.check(
            "translation deepl key not leaked",
            deepl_key not in json.dumps(data),
            data)
    reloaded = json_call(
        ledger, cli, ["translation-keys"], env,
        "translation keys reload")
    if reloaded is not None:
        ledger.check(
            "translation keys persisted masked",
            reloaded.get("google") == "AIza****"
            and reloaded.get("deepl") == "abcd****",
            reloaded)
        ledger.check(
            "translation keys reload not leaked",
            google_key not in json.dumps(reloaded)
            and deepl_key not in json.dumps(reloaded),
            reloaded)
    data = json_call(
        ledger, cli, ["translation-keys-set", "google", "abc"], env,
        "translation keys short")
    if data is not None:
        ledger.check(
            "translation short key fully masked",
            data.get("google") == "****",
            data)
    json_call(
        ledger, cli, ["translation-keys-set", "google", google_key], env,
        "translation keys restore google")

    env = make_env(work / "translation-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "translationProvider": 2,
            "deepLFormality": 1,
            "googleCloudTranslateKey": google_key,
            "deepLTranslateKey": deepl_key,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "translation alias import")
    if data is not None:
        ledger.check(
            "translation alias applied",
            data.get("applied") == 4 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["translation-provider"], env,
        "translation alias provider")
    if reloaded is not None:
        ledger.check(
            "translation alias provider value",
            reloaded.get("provider") == 2,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["translation-formality"], env,
        "translation alias formality")
    if reloaded is not None:
        ledger.check(
            "translation alias formality value",
            reloaded.get("formality") == 1,
            reloaded)
    reloaded = json_call(
        ledger, cli, ["translation-keys"], env,
        "translation alias keys")
    if reloaded is not None:
        ledger.check(
            "translation alias keys masked",
            reloaded.get("google") == "AIza****"
            and reloaded.get("deepl") == "abcd****",
            reloaded)

    data = json_call(
        ledger, cli, ["backup-export", "0", "1", "0", "1", "0", "1", "1"],
        env, "translation export")
    if data is not None:
        section = backup_sections(data)["exteraconfig"]
        ledger.check(
            "translation export provider",
            section.get("OETranslationProvider") == 2
            and section.get("OETranslationDeepLFormality") == 1,
            (section.get("OETranslationProvider"),
             section.get("OETranslationDeepLFormality")))
        ledger.check(
            "translation export has no google key",
            "OETranslationGoogleCloudKey" not in section,
            sorted(section))
        ledger.check(
            "translation export has no deepl key",
            "OETranslationDeepLKey" not in section,
            sorted(section))
        dump = json.dumps(data)
        ledger.check(
            "translation export leaks no key material",
            google_key not in dump and deepl_key not in dump,
            "secret found in backup export")

    for name, arguments in [
            ("translation provider set without value",
             ["translation-provider-set"]),
            ("translation provider set non numeric",
             ["translation-provider-set", "google"]),
            ("translation formality set non numeric",
             ["translation-formality-set", "more"]),
            ("translation keys set unknown target",
             ["translation-keys-set", "bing", "key"]),
            ("translation keys set without key",
             ["translation-keys-set", "google"])]:
        result = run(cli, arguments, env=env)
        ledger.check(
            f"cli {name}",
            result.returncode == 2 and bool(result.stderr.strip()),
            f"exit {result.returncode}, stderr {result.stderr!r}")

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_ai(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "ai-defaults")
    data = json_call(ledger, cli, ["ai-config"], env, "ai config defaults")
    if data is not None:
        ledger.check(
            "ai endpoint default",
            data.get("endpoint") == "https://api.openai.com/v1",
            data)
        ledger.check(
            "ai model default",
            data.get("model") == "gpt-4o-mini",
            data)
        ledger.check(
            "ai key default absent",
            data.get("hasKey") is False,
            data)

    data = json_call(
        ledger, cli, ["ai-config-set-endpoint", "https://example.test/v1"],
        env, "ai endpoint set")
    if data is not None:
        ledger.check(
            "ai endpoint set",
            data.get("endpoint") == "https://example.test/v1",
            data)
    data = json_call(
        ledger, cli, ["ai-config-set-model", "test-model"], env,
        "ai model set")
    if data is not None:
        ledger.check("ai model set", data.get("model") == "test-model", data)
    ai_key = "sk-test-0123456789abcdef"
    data = json_call(
        ledger, cli, ["ai-config-set-key", ai_key], env, "ai key set")
    if data is not None:
        ledger.check("ai key set has key", data.get("hasKey") is True, data)
        ledger.check(
            "ai key not printed",
            ai_key not in json.dumps(data),
            "secret found in ai config output")
    reloaded = json_call(ledger, cli, ["ai-config"], env, "ai config reload")
    if reloaded is not None:
        ledger.check(
            "ai config persisted",
            reloaded.get("endpoint") == "https://example.test/v1"
            and reloaded.get("model") == "test-model"
            and reloaded.get("hasKey") is True,
            reloaded)
        ledger.check(
            "ai config reload not leaked",
            ai_key not in json.dumps(reloaded),
            reloaded)

    prompt = "Hello from checks"
    data = json_call(
        ledger, cli, ["ai-request-json", prompt], env, "ai request json")
    if data is not None:
        messages = data.get("messages")
        ledger.check(
            "ai request model",
            data.get("model") == "test-model",
            data)
        ledger.check(
            "ai request user message",
            isinstance(messages, list) and len(messages) == 1
            and messages[0].get("role") == "user"
            and messages[0].get("content") == prompt,
            messages)
        ledger.check(
            "ai request not streaming",
            data.get("stream") is False,
            data)
        ledger.check(
            "ai request has no secret",
            ai_key not in json.dumps(data),
            "secret found in request body")

    data = json_call(
        ledger, cli, ["backup-export", "0", "1", "0", "1", "0", "1", "1"],
        env, "ai export")
    if data is not None:
        section = backup_sections(data)["exteraconfig"]
        ledger.check(
            "ai export endpoint",
            section.get("OEAiEndpoint") == "https://example.test/v1",
            section.get("OEAiEndpoint"))
        ledger.check(
            "ai export model",
            section.get("OEAiModel") == "test-model",
            section.get("OEAiModel"))
        ledger.check(
            "ai export has no key",
            "OEAiApiKey" not in section,
            sorted(section))
        ledger.check(
            "ai export leaks no key material",
            ai_key not in json.dumps(data),
            "secret found in backup export")

    env = make_env(work / "ai-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "aiEndpoint": "https://alias.test/v1",
            "aiModel": "alias-model",
            "aiApiKey": ai_key,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "ai alias import")
    if data is not None:
        ledger.check(
            "ai alias applied",
            data.get("applied") == 3 and data.get("unknown") == [],
            data)
    reloaded = json_call(ledger, cli, ["ai-config"], env, "ai alias reload")
    if reloaded is not None:
        ledger.check(
            "ai alias values",
            reloaded.get("endpoint") == "https://alias.test/v1"
            and reloaded.get("model") == "alias-model"
            and reloaded.get("hasKey") is True,
            reloaded)

    for name, arguments in [
            ("ai endpoint set without value", ["ai-config-set-endpoint"]),
            ("ai model set without value", ["ai-config-set-model"]),
            ("ai key set without value", ["ai-config-set-key"]),
            ("ai request without prompt", ["ai-request-json"])]:
        result = run(cli, arguments, env=env)
        ledger.check(
            f"cli {name}",
            result.returncode == 2 and bool(result.stderr.strip()),
            f"exit {result.returncode}, stderr {result.stderr!r}")

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_ai_stream(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    def stream(arguments, name):
        result = run(cli, arguments)
        if result.returncode != 0:
            ledger.check(
                name,
                False,
                f"exit {result.returncode}, "
                f"stderr {result.stderr.strip()!r}")
            return None
        try:
            payload = json.loads(result.stdout)
        except json.JSONDecodeError as error:
            ledger.check(name, False, f"json: {error}")
            return None
        if not isinstance(payload, dict):
            ledger.check(name, False, payload)
            return None
        return payload

    basic = stream(
        ["ai-stream-parse", str(FIXTURES / "ai-stream-basic.txt")],
        "ai stream basic parse")
    if basic is not None:
        ledger.check(
            "ai stream basic text",
            basic.get("text") == "Hello, world",
            repr(basic.get("text")))
        ledger.check(
            "ai stream basic done", basic.get("done") is True, basic)

    expected = "\u041f\u0440\u0438\u0432\u0435\u0442 \u043c\u0438\u0440\nline2"
    split = stream(
        ["ai-stream-parse", str(FIXTURES / "ai-stream-split.txt")],
        "ai stream split parse")
    if split is not None:
        ledger.check(
            "ai stream split whole text",
            split.get("text") == expected,
            repr(split.get("text")))
        ledger.check(
            "ai stream split whole done", split.get("done") is True, split)

    for size in (1, 7):
        chunked = stream(
            ["ai-stream-chunk", str(FIXTURES / "ai-stream-split.txt"),
             str(size)],
            f"ai stream split chunk {size}")
        if chunked is not None:
            ledger.check(
                f"ai stream chunk {size} text",
                chunked.get("text") == expected,
                repr(chunked.get("text")))
            ledger.check(
                f"ai stream chunk {size} done",
                chunked.get("done") is True,
                chunked)

    chunked = stream(
        ["ai-stream-chunk", str(FIXTURES / "ai-stream-basic.txt"), "3"],
        "ai stream basic chunk 3")
    if chunked is not None:
        ledger.check(
            "ai stream basic chunk text",
            chunked.get("text") == "Hello, world",
            repr(chunked.get("text")))

    error = stream(
        ["ai-stream-parse", str(FIXTURES / "ai-stream-error.txt")],
        "ai stream error parse")
    if error is not None:
        ledger.check(
            "ai stream error empty text",
            error.get("text") == "",
            repr(error.get("text")))
        ledger.check(
            "ai stream error not done", error.get("done") is False, error)

    empty_dir = work / "ai-stream"
    empty_dir.mkdir(parents=True, exist_ok=True)
    empty_path = empty_dir / "empty.txt"
    empty_path.write_bytes(b"")
    empty = stream(
        ["ai-stream-parse", str(empty_path)], "ai stream empty parse")
    if empty is not None:
        ledger.check(
            "ai stream empty text",
            empty.get("text") == "",
            repr(empty.get("text")))
        ledger.check(
            "ai stream empty not done", empty.get("done") is False, empty)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_ai_roles(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    roles = json.loads(
        (FIXTURES / "ai-roles.json").read_text(encoding="utf-8"))
    encoded_roles = json.dumps(roles)

    env = make_env(work / "ai-roles-defaults")
    data = json_call(ledger, cli, ["ai-roles"], env, "ai roles defaults")
    if data is not None:
        ledger.check("ai roles default empty", data.get("roles") == [], data)
    data = json_call(ledger, cli, ["ai-role"], env, "ai role defaults")
    if data is not None:
        ledger.check(
            "ai role default none",
            data.get("name") == ""
            and data.get("found") is False
            and data.get("prompt") == "",
            data)
    data = json_call(ledger, cli, ["ai-history"], env, "ai history default")
    if data is not None:
        ledger.check(
            "ai history default enabled",
            data.get("enabled") is True,
            data)

    data = json_call(
        ledger, cli, ["ai-roles-set", encoded_roles], env, "ai roles set")
    if data is not None:
        ledger.check(
            "ai roles set stored", data.get("roles") == roles, data)
    data = json_call(ledger, cli, ["ai-roles"], env, "ai roles reload")
    if data is not None:
        ledger.check("ai roles persisted", data.get("roles") == roles, data)

    data = json_call(
        ledger, cli, ["ai-role", roles[1]["name"]], env, "ai role by name")
    if data is not None:
        ledger.check(
            "ai role by name prompt",
            data.get("found") is True
            and data.get("prompt") == roles[1]["prompt"],
            data)

    data = json_call(
        ledger, cli, ["ai-role-set", roles[0]["name"]], env, "ai role set")
    if data is not None:
        ledger.check(
            "ai role set active",
            data.get("name") == roles[0]["name"]
            and data.get("found") is True
            and data.get("prompt") == roles[0]["prompt"],
            data)
    data = json_call(ledger, cli, ["ai-role"], env, "ai role reload")
    if data is not None:
        ledger.check(
            "ai role persisted",
            data.get("name") == roles[0]["name"]
            and data.get("found") is True,
            data)

    data = json_call(
        ledger, cli, ["ai-role-set", "missing"], env, "ai role set unknown")
    if data is not None:
        ledger.check(
            "ai role unknown not found",
            data.get("name") == "missing" and data.get("found") is False,
            data)

    for name, arguments in [
            ("ai roles-set invalid json", ["ai-roles-set", "{"]),
            ("ai roles-set not array", ["ai-roles-set", '{"name":"a"}']),
            ("ai roles-set bad role", ["ai-roles-set", '[{"name":1}]']),
            ("ai roles-set too many", ["ai-roles-set", json.dumps(
                [{"name": f"r{i}", "prompt": "p"} for i in range(33)])]),
            ("ai roles-set prompt too long", ["ai-roles-set", json.dumps(
                [{"name": "long", "prompt": "x" * 4097}])]),
            ("ai roles-set without value", ["ai-roles-set"]),
            ("ai history-set invalid", ["ai-history-set", "2"])]:
        result = run(cli, arguments, env=env)
        ledger.check(
            f"cli {name}",
            result.returncode == 2 and bool(result.stderr.strip()),
            f"exit {result.returncode}, stderr {result.stderr!r}")

    data = json_call(
        ledger, cli, ["ai-roles"], env, "ai roles after invalid")
    if data is not None:
        ledger.check(
            "ai roles invalid not stored", data.get("roles") == roles, data)

    data = json_call(ledger, cli, ["ai-history-set", "0"], env,
                     "ai history off")
    if data is not None:
        ledger.check("ai history set off", data.get("enabled") is False, data)
    data = json_call(ledger, cli, ["ai-history"], env, "ai history reload")
    if data is not None:
        ledger.check(
            "ai history persisted", data.get("enabled") is False, data)
    data = json_call(ledger, cli, ["ai-history-set", "1"], env,
                     "ai history on")
    if data is not None:
        ledger.check("ai history set on", data.get("enabled") is True, data)

    data = json_call(
        ledger, cli, ["backup-export", "0", "1", "0", "1", "0", "1", "1"],
        env, "ai roles export")
    if data is not None:
        section = backup_sections(data)["exteraconfig"]
        ledger.check(
            "ai roles export keys",
            section.get("OEAiRoles") == encoded_roles
            and section.get("OEAiActiveRole") == "missing"
            and section.get("OEAiHistory") is True,
            (section.get("OEAiRoles"),
             section.get("OEAiActiveRole"),
             section.get("OEAiHistory")))

    env = make_env(work / "ai-roles-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "aiRoles": encoded_roles,
            "aiActiveRole": roles[0]["name"],
            "aiHistory": False,
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "ai roles alias import")
    if data is not None:
        ledger.check(
            "ai roles alias applied",
            data.get("applied") == 3 and data.get("unknown") == [],
            data)
    data = json_call(ledger, cli, ["ai-roles"], env, "ai roles alias reload")
    if data is not None:
        ledger.check(
            "ai roles alias roles", data.get("roles") == roles, data)
    data = json_call(ledger, cli, ["ai-role"], env, "ai role alias reload")
    if data is not None:
        ledger.check(
            "ai role alias active",
            data.get("name") == roles[0]["name"]
            and data.get("found") is True,
            data)
    data = json_call(
        ledger, cli, ["ai-history"], env, "ai history alias reload")
    if data is not None:
        ledger.check(
            "ai history alias off", data.get("enabled") is False, data)

    env = make_env(work / "ai-roles-bad-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "aiRoles": '[{"name":1}]',
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "ai roles bad alias import")
    if data is not None:
        ledger.check(
            "ai roles bad alias rejected",
            data.get("ok") is True
            and data.get("applied") == 0
            and data.get("unknown") == ["aiRoles"],
            data)

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_recognition(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "recognition-defaults")
    data = json_call(
        ledger, cli, ["recognition-language"], env,
        "recognition language defaults")
    if data is not None:
        ledger.check(
            "recognition language default ru",
            data.get("language") == "ru",
            data)

    data = json_call(
        ledger, cli, ["recognition-language-set", "en"], env,
        "recognition language set en")
    if data is not None:
        ledger.check(
            "recognition language set en",
            data.get("language") == "en",
            data)
    reloaded = json_call(
        ledger, cli, ["recognition-language"], env,
        "recognition language reload")
    if reloaded is not None:
        ledger.check(
            "recognition language persisted",
            reloaded.get("language") == "en",
            reloaded)

    env = make_env(work / "recognition-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "recognitionLanguage": "de",
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "recognition alias import")
    if data is not None:
        ledger.check(
            "recognition alias applied",
            data.get("applied") == 1 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["recognition-language"], env,
        "recognition alias reload")
    if reloaded is not None:
        ledger.check(
            "recognition alias value",
            reloaded.get("language") == "de",
            reloaded)

    for name, arguments in [
            ("recognition language set without value",
             ["recognition-language-set"])]:
        result = run(cli, arguments, env=env)
        ledger.check(
            f"cli {name}",
            result.returncode == 2 and bool(result.stderr.strip()),
            f"exit {result.returncode}, stderr {result.stderr!r}")

    return (
        ledger.checks - before_checks,
        len(ledger.failures) - before_failures)


def check_network(ledger, cli, work):
    before_checks = ledger.checks
    before_failures = len(ledger.failures)

    env = make_env(work / "network-defaults")
    data = json_call(
        ledger, cli, ["network"], env, "network defaults")
    if data is not None:
        ledger.check(
            "network ipv6 default boolean",
            isinstance(data.get("useIPv6"), bool),
            data.get("useIPv6"))
        ledger.check(
            "network dns default builtin doh",
            data.get("dnsType") == 0,
            data.get("dnsType"))
        ledger.check(
            "network doh default empty",
            data.get("customDoh") == "",
            data.get("customDoh"))

    data = json_call(
        ledger, cli, ["network-set-ipv6", "1"], env,
        "network set ipv6")
    if data is not None:
        ledger.check(
            "network set ipv6 on",
            data.get("useIPv6") is True,
            data)
    reloaded = json_call(
        ledger, cli, ["network"], env, "network ipv6 reload")
    if reloaded is not None:
        ledger.check(
            "network ipv6 persisted",
            reloaded.get("useIPv6") is True,
            reloaded)

    data = json_call(
        ledger, cli, ["network-set-dns", "1"], env, "network set dns")
    if data is not None:
        ledger.check(
            "network set dns system",
            data.get("dnsType") == 1,
            data)
    reloaded = json_call(
        ledger, cli, ["network"], env, "network dns reload")
    if reloaded is not None:
        ledger.check(
            "network dns persisted",
            reloaded.get("dnsType") == 1,
            reloaded)

    data = json_call(
        ledger, cli, ["network-set-dns", "5"], env,
        "network set dns clamp high")
    if data is not None:
        ledger.check(
            "network dns clamped high",
            data.get("dnsType") == 1,
            data)
    data = json_call(
        ledger, cli, ["network-set-dns", "-3"], env,
        "network set dns clamp low")
    if data is not None:
        ledger.check(
            "network dns clamped low",
            data.get("dnsType") == 0,
            data)

    doh_url = "https://dns.example/dns-query"
    data = json_call(
        ledger, cli, ["network-set-doh", doh_url], env,
        "network set doh")
    if data is not None:
        ledger.check(
            "network set doh url",
            data.get("customDoh") == doh_url,
            data)
    reloaded = json_call(
        ledger, cli, ["network"], env, "network doh reload")
    if reloaded is not None:
        ledger.check(
            "network doh persisted",
            reloaded.get("customDoh") == doh_url,
            reloaded)
    data = json_call(
        ledger, cli, ["network-set-doh", "-"], env,
        "network clear doh")
    if data is not None:
        ledger.check(
            "network doh cleared",
            data.get("customDoh") == "",
            data)

    env = make_env(work / "network-alias")
    root = {
        "version": 1,
        "exteraconfig": {
            "useIPv6": True,
            "dnsType": 1,
            "customDoH": "https://alias.test/dns-query",
        },
    }
    data = json_call(
        ledger, cli, ["backup-import-json", json.dumps(root)], env,
        "network alias import")
    if data is not None:
        ledger.check(
            "network alias applied",
            data.get("applied") == 3 and data.get("unknown") == [],
            data)
    reloaded = json_call(
        ledger, cli, ["network"], env, "network alias reload")
    if reloaded is not None:
        ledger.check(
            "network alias values",
            reloaded.get("useIPv6") is True
            and reloaded.get("dnsType") == 1
            and reloaded.get("customDoh")
            == "https://alias.test/dns-query",
            reloaded)

    for name, arguments in [
            ("network set ipv6 invalid flag",
             ["network-set-ipv6", "2"]),
            ("network set dns without value", ["network-set-dns"]),
            ("network set doh without value", ["network-set-doh"])]:
        result = run(cli, arguments, env=env)
        ledger.check(
            f"cli {name}",
            result.returncode == 2 and bool(result.stderr.strip()),
            f"exit {result.returncode}, stderr {result.stderr!r}")

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
        bot_privacy = check_bot_privacy(ledger, cli, work)
        deleted = check_deleted(ledger, cli, work)
        deleted_entry = check_deleted_entry(ledger, cli, work)
        reset = check_reset(ledger, cli, work)
        workdir = check_workdir(ledger, cli, work)
        menu = check_message_menu(ledger, cli, work)
        doubletap = check_doubletap(ledger, cli, work)
        stickertime = check_sticker_time(ledger, cli, work)
        pillorder = check_pill_order(ledger, cli, work)
        ghost = check_ghost(ledger, cli, work)
        deleted_mark = check_deleted_mark(ledger, cli, work)
        vpn = check_vpn(ledger, cli)
        proxy_rules = check_proxy_rules(ledger, cli)
        pangu = check_pangu(ledger, cli, work)
        zalgo = check_zalgo(ledger, cli, work)
        spoilers_direct = check_spoilers_direct(ledger, cli, work)
        formatting_buttons = check_formatting_buttons(ledger, cli, work)
        silent = check_silent(ledger, cli, work)
        sections = check_settings_sections(ledger, cli, work)
        ai_editor = check_ai_editor(ledger, cli, work)
        pluginops = check_plugin_installer(ledger, cli, work)
        send_as = check_send_as(ledger, cli, work)
        profile_buttons = check_profile_buttons(ledger, cli, work)
        inline_math = check_inline_math(ledger, cli, work)
        save_subfolder = check_save_subfolder(ledger, cli, work)
        ghost_schedule = check_ghost_schedule(ledger, cli, work)
        seek_duration = check_seek_duration(ledger, cli, work)
        sticker_size = check_sticker_size(ledger, cli, work)
        avatar_corners = check_avatar_corners(ledger, cli, work)
        header_style = check_header_style(ledger, cli, work)
        section_radius = check_section_radius(ledger, cli, work)
        edited_mark = check_edited_mark(ledger, cli, work)
        forward_date = check_forward_date(ledger, cli, work)
        sender_mini_avatars = check_sender_mini_avatars(
            ledger, cli, work)
        media_menu = check_media_menu(ledger, cli, work)
        chat_menu = check_chat_menu(ledger, cli, work)
        disable_link_preview = check_disable_link_preview(ledger, cli, work)
        hide_all_chats = check_hide_all_chats(ledger, cli, work)
        hide_archive_sort = check_hide_archive_sort(ledger, cli, work)
        autodownload_rules = check_autodownload_rules(ledger, cli, work)
        wide_posts = check_wide_posts(ledger, cli, work)
        always_show_download = check_always_show_download(ledger, cli, work)
        show_results_before_voting = check_show_results_before_voting(
            ledger, cli, work)
        reply_style = check_reply_style(ledger, cli, work)
        false_bottom = check_false_bottom(ledger, cli, work)
        decoration = check_decoration(ledger, cli, work)
        shortcuts = check_shortcuts(ledger, cli, work)
        menu_style = check_message_menu_style(ledger, cli, work)
        chat_misc = check_chat_misc(ledger, cli, work)
        persian = check_persian(ledger, cli, work)
        media_quality = check_media_quality(ledger, cli, work)
        update_channel = check_update_channel(ledger, cli, work)
        translation = check_translation(ledger, cli, work)
        ai = check_ai(ledger, cli, work)
        ai_stream = check_ai_stream(ledger, cli, work)
        ai_roles = check_ai_roles(ledger, cli, work)
        recognition = check_recognition(ledger, cli, work)
        network = check_network(ledger, cli, work)
        download_boost = check_download_boost(ledger, cli, work)
        subtitles = check_subtitles(ledger, cli, work)
        lyrics = check_lyrics(ledger, cli, work)
        lrclib = check_lrclib(ledger, cli, work)
        extra_pills = check_extra_pills(ledger, cli, work)
        regex_filters = check_regex_filters(ledger, cli, work)
        backup = check_backup(ledger, cli, work)
    print(f"strip: {strips[0] - strips[1]} passed")
    print(f"reactions: {reactions[0] - reactions[1]} passed")
    print(f"cli: {cli_args[0] - cli_args[1]} passed")
    print(f"config: {config[0] - config[1]} passed")
    print(f"general: {general[0] - general[1]} passed")
    print(f"bot-privacy: {bot_privacy[0] - bot_privacy[1]} passed")
    print(f"deleted: {deleted[0] - deleted[1]} passed")
    print(f"deleted-entry: {deleted_entry[0] - deleted_entry[1]} passed")
    print(f"reset: {reset[0] - reset[1]} passed")
    print(f"workdir: {workdir[0] - workdir[1]} passed")
    print(f"menu: {menu[0] - menu[1]} passed")
    print(f"doubletap: {doubletap[0] - doubletap[1]} passed")
    print(f"stickertime: {stickertime[0] - stickertime[1]} passed")
    print(f"pillorder: {pillorder[0] - pillorder[1]} passed")
    print(f"ghost: {ghost[0] - ghost[1]} passed")
    print(f"deletedmark: {deleted_mark[0] - deleted_mark[1]} passed")
    print(f"vpn: {vpn[0] - vpn[1]} passed")
    print(f"proxy-rules: {proxy_rules[0] - proxy_rules[1]} passed")
    print(f"pangu: {pangu[0] - pangu[1]} passed")
    print(f"zalgo: {zalgo[0] - zalgo[1]} passed")
    print(f"spoilers-direct: "
          f"{spoilers_direct[0] - spoilers_direct[1]} passed")
    print(f"formatting-buttons: "
          f"{formatting_buttons[0] - formatting_buttons[1]} passed")
    print(f"silent: {silent[0] - silent[1]} passed")
    print(f"sections: {sections[0] - sections[1]} passed")
    print(f"ai-editor: {ai_editor[0] - ai_editor[1]} passed")
    print(f"pluginops: {pluginops[0] - pluginops[1]} passed")
    print(f"send-as: {send_as[0] - send_as[1]} passed")
    print(f"profile-buttons: "
          f"{profile_buttons[0] - profile_buttons[1]} passed")
    print(f"inline-math: {inline_math[0] - inline_math[1]} passed")
    print(f"save-subfolder: {save_subfolder[0] - save_subfolder[1]} passed")
    print(f"ghost-schedule: {ghost_schedule[0] - ghost_schedule[1]} passed")
    print(f"seek-duration: {seek_duration[0] - seek_duration[1]} passed")
    print(f"sticker-size: {sticker_size[0] - sticker_size[1]} passed")
    print(f"avatar-corners: {avatar_corners[0] - avatar_corners[1]} passed")
    print(f"header-style: {header_style[0] - header_style[1]} passed")
    print(f"section-radius: "
          f"{section_radius[0] - section_radius[1]} passed")
    print(f"edited-mark: {edited_mark[0] - edited_mark[1]} passed")
    print(f"forward-date: {forward_date[0] - forward_date[1]} passed")
    print(f"sender-mini-avatars: "
          f"{sender_mini_avatars[0] - sender_mini_avatars[1]} passed")
    print(f"media-menu: {media_menu[0] - media_menu[1]} passed")
    print(f"chat-menu: {chat_menu[0] - chat_menu[1]} passed")
    print(f"disable-link-preview: "
          f"{disable_link_preview[0] - disable_link_preview[1]} passed")
    print(f"hide-all-chats: "
          f"{hide_all_chats[0] - hide_all_chats[1]} passed")
    print(f"hide-archive-sort: "
          f"{hide_archive_sort[0] - hide_archive_sort[1]} passed")
    print(f"autodownload-rules: "
          f"{autodownload_rules[0] - autodownload_rules[1]} passed")
    print(f"wide-posts: {wide_posts[0] - wide_posts[1]} passed")
    print(f"always-show-download: "
          f"{always_show_download[0] - always_show_download[1]} passed")
    print(f"show-results-before-voting: "
          f"{show_results_before_voting[0] - show_results_before_voting[1]} "
          f"passed")
    print(f"reply-style: {reply_style[0] - reply_style[1]} passed")
    print(f"false-bottom: {false_bottom[0] - false_bottom[1]} passed")
    print(f"decoration: {decoration[0] - decoration[1]} passed")
    print(f"shortcuts: {shortcuts[0] - shortcuts[1]} passed")
    print(f"message-menu-style: "
          f"{menu_style[0] - menu_style[1]} passed")
    print(f"chat-misc: {chat_misc[0] - chat_misc[1]} passed")
    print(f"persian: {persian[0] - persian[1]} passed")
    print(f"media-quality: {media_quality[0] - media_quality[1]} passed")
    print(f"update-channel: "
          f"{update_channel[0] - update_channel[1]} passed")
    print(f"translation: {translation[0] - translation[1]} passed")
    print(f"ai: {ai[0] - ai[1]} passed")
    print(f"ai-stream: {ai_stream[0] - ai_stream[1]} passed")
    print(f"ai-roles: {ai_roles[0] - ai_roles[1]} passed")
    print(f"recognition: {recognition[0] - recognition[1]} passed")
    print(f"network: {network[0] - network[1]} passed")
    print(f"download-boost: {download_boost[0] - download_boost[1]} passed")
    print(f"subtitles: {subtitles[0] - subtitles[1]} passed")
    print(f"lyrics: {lyrics[0] - lyrics[1]} passed")
    print(f"lrclib: {lrclib[0] - lrclib[1]} passed")
    print(f"extra-pills: {extra_pills[0] - extra_pills[1]} passed")
    print(f"regex-filters: {regex_filters[0] - regex_filters[1]} passed")
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
