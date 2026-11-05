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
        ("inline-math-set with wrong count", ["inline-math-set"]),
        ("save-subfolder-set with wrong count", ["save-subfolder-set"]),
        ("ghost-schedule-set with wrong count", ["ghost-schedule-set"]),
        ("seek-duration-set without value", ["seek-duration-set"]),
        ("edited-mark-set with wrong count", ["edited-mark-set", "1"]),
        ("forward-date-set with wrong count", ["forward-date-set"]),
        ("media-menu-set with wrong count", ["media-menu-set", "1"]),
        ("chat-menu-set with wrong count", ["chat-menu-set", "1"]),
        ("disable-link-preview-set with wrong count",
         ["disable-link-preview-set"]),
        ("hide-all-chats-set with wrong count",
         ["hide-all-chats-set"]),
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
            len(section) == 79,
            sorted(section))
        ledger.check(
            "backup pill keys",
            exported["pillstackconfig"] == {
                "OEPillStackEnabled": False,
                "OEPillStackPills": 255,
                "OEPillStackOrder":
                    "cpu,ram,net,dc,proxy,cache,ghost,lastseen",
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
            sum(len(value) for value in exported.values()) == 84,
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
                data.get("applied") == 84,
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
        "pillstackconfig": {
            "OEPillStackEnabled": True,
            "OEPillStackPills": 5,
            "OEPillStackOrder": "ghost,cache,cpu,ram,net,dc,proxy,lastseen",
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
            data.get("applied") == 84,
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
            "backup full pill keys",
            sections["pillstackconfig"] == {
                "OEPillStackEnabled": True,
                "OEPillStackPills": 5,
                "OEPillStackOrder":
                    "ghost,cache,cpu,ram,net,dc,proxy,lastseen",
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
            "backup legacy eight completed to 84",
            (len(sections["exteraconfig"]),
             len(sections["pillstackconfig"]),
             len(sections["mainconfig"])) == (79, 3, 2),
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
        ("translate maps to none", "3", "none"),
        ("reply", "4", "reply"),
        ("save maps to none", "5", "none"),
        ("repeat maps to none", "6", "none"),
        ("edit maps to none", "8", "none"),
        ("delete maps to none", "10", "none"),
        ("forward maps to none", "11", "none"),
        ("download maps to none", "12", "none"),
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
               "lastseen"]

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
              "lastseen"]
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
                 "lastseen"]
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
                "lastseen"],
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
            == "cpu,ram,net,dc,proxy,cache,ghost,lastseen",
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
                "cache"],
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
        silent = check_silent(ledger, cli, work)
        sections = check_settings_sections(ledger, cli, work)
        ai_editor = check_ai_editor(ledger, cli, work)
        pluginops = check_plugin_installer(ledger, cli, work)
        send_as = check_send_as(ledger, cli, work)
        inline_math = check_inline_math(ledger, cli, work)
        save_subfolder = check_save_subfolder(ledger, cli, work)
        ghost_schedule = check_ghost_schedule(ledger, cli, work)
        seek_duration = check_seek_duration(ledger, cli, work)
        edited_mark = check_edited_mark(ledger, cli, work)
        forward_date = check_forward_date(ledger, cli, work)
        media_menu = check_media_menu(ledger, cli, work)
        chat_menu = check_chat_menu(ledger, cli, work)
        disable_link_preview = check_disable_link_preview(ledger, cli, work)
        hide_all_chats = check_hide_all_chats(ledger, cli, work)
        always_show_download = check_always_show_download(ledger, cli, work)
        false_bottom = check_false_bottom(ledger, cli, work)
        decoration = check_decoration(ledger, cli, work)
        shortcuts = check_shortcuts(ledger, cli, work)
        menu_style = check_message_menu_style(ledger, cli, work)
        backup = check_backup(ledger, cli, work)
    print(f"strip: {strips[0] - strips[1]} passed")
    print(f"reactions: {reactions[0] - reactions[1]} passed")
    print(f"cli: {cli_args[0] - cli_args[1]} passed")
    print(f"config: {config[0] - config[1]} passed")
    print(f"general: {general[0] - general[1]} passed")
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
    print(f"silent: {silent[0] - silent[1]} passed")
    print(f"sections: {sections[0] - sections[1]} passed")
    print(f"ai-editor: {ai_editor[0] - ai_editor[1]} passed")
    print(f"pluginops: {pluginops[0] - pluginops[1]} passed")
    print(f"send-as: {send_as[0] - send_as[1]} passed")
    print(f"inline-math: {inline_math[0] - inline_math[1]} passed")
    print(f"save-subfolder: {save_subfolder[0] - save_subfolder[1]} passed")
    print(f"ghost-schedule: {ghost_schedule[0] - ghost_schedule[1]} passed")
    print(f"seek-duration: {seek_duration[0] - seek_duration[1]} passed")
    print(f"edited-mark: {edited_mark[0] - edited_mark[1]} passed")
    print(f"forward-date: {forward_date[0] - forward_date[1]} passed")
    print(f"media-menu: {media_menu[0] - media_menu[1]} passed")
    print(f"chat-menu: {chat_menu[0] - chat_menu[1]} passed")
    print(f"disable-link-preview: "
          f"{disable_link_preview[0] - disable_link_preview[1]} passed")
    print(f"hide-all-chats: "
          f"{hide_all_chats[0] - hide_all_chats[1]} passed")
    print(f"always-show-download: "
          f"{always_show_download[0] - always_show_download[1]} passed")
    print(f"false-bottom: {false_bottom[0] - false_bottom[1]} passed")
    print(f"decoration: {decoration[0] - decoration[1]} passed")
    print(f"shortcuts: {shortcuts[0] - shortcuts[1]} passed")
    print(f"message-menu-style: "
          f"{menu_style[0] - menu_style[1]} passed")
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
