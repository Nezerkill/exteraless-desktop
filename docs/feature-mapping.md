# Feature mapping: exteraless Android → Exteraless Desktop

This document fixes what "port" means for the MVP. Android UI is not copied
literally; the behavior and the settings schema are reproduced in tdesktop's
Qt interface.

## MVP1: base, settings, proxy, backup

Implemented:

- `Exteraless` settings section with `Relative last seen` and
  `Comma after mention`;
- per-network proxy auto-disable (VPN, mobile data, Wi-Fi);
- `.extera` import/export with the Android invisible-Unicode envelope;
- JSON config at `tdata/exteraless.json` under the app data directory.

### Settings

New `Exteraless` section in tdesktop Settings with subsections:

- **General**
  - relative last seen (`OERelativeLastSeen`, default `false`);
  - comma after mention (`OEAddCommaAfterMention`, default `true`).
- **Appearance**
  - avatar corners (`OEAppearanceAvatarCorners`);
  - square FAB (`OEAppearanceSquareFab`, default `true`);
  - section radius (`OEAppearanceSectionRadius`);
  - separate headers (`OEAppearanceSeparateHeaders`);
  - navigation drawer — not implemented as a setting: tdesktop's main menu is
    the drawer itself (see Drawer below), there is no
    `OEAppearanceNavigationDrawer` key.
- **Chats**
  - wide channel posts (`OEChatsWideChannelPosts`, default `false`);
  - remove message tail (`OEChatsRemoveMessageTail`, default `true`);
  - hide reactions in channels/groups/private (`OEChatsHideReactions*`);
  - strip tracking on open/paste (`OEChatsStripTrackingOnOpen/Paste`, default `true`).
- **Proxy** — see below.
- **Backup** — import/export `.extera`.

All keys keep their `OE*` names. On import, unknown keys are reported, not
silently dropped. Existing tdesktop settings remain untouched.

### Proxy

Android conditions from `ProxyDisableCondition`:

| Flag | Value | Desktop source |
|---|---:|---|
| VPN | 1 | tunnel interface (`tun*`, `tap*`, `wg*`, `ppp*`) or NetworkManager |
| Mobile data | 2 | `QNetworkInformation::TransportMedium::Cellular` |
| Wi-Fi | 4 | `QNetworkInformation::TransportMedium::WiFi` |

The proxy list, editing and MTProto/SOCKS5 transport already exist in tdesktop.
The port adds display names, ping display and the auto-disable mask
(`OEProxyDisableConditions`). If detection is unavailable, the condition is
inactive and the setting UI says so.

### Backup

`.extera` is a JSON document wrapped in invisible Unicode characters. Import
maps known `OE*` keys and writes a report with unknown/unsupported keys. Export
writes only Exteraless settings in MVP1; Telegram account data is not included.

## MVP2: Ghost, deleted messages, Pill Stack, drawer

### Ghost

Android toggles from `NekoConfig`:

- `sendReadMessagePackets` — read receipts and message contents;
- `sendReadStoriesPackets` — story views;
- `sendOnlinePackets` — online/offline status;
- `sendUploadProgress` — typing and upload progress;
- `sendOfflinePacketAfterOnline` — return offline after sending.

Per-chat exclusions: `ghostModeReadExclusion_<id>` and
`ghostModeTypingExclusion_<id>`.

Desktop implementation: an outbound MTP request filter before
`MTP::Instance::send`, with an explicit allow-list bypass for internal reads
(same idea as `AyuState.setAllowReadPacket`). The filter must not touch
non-Ghost requests.

### Deleted messages

AyuGram stores deleted messages in a separate local database
(`DeletedMessage`, `DeletedMessageFull`, `DeletedMessageDao`). Desktop keeps a
separate SQLite table keyed by account, peer and message id. On
`UpdateDeleteMessages`, the message is saved before the local history is
cleared. UI: a deleted-message mark in the chat and a per-peer list. Import of
the old Android database is not part of MVP2.

### Pill Stack

MVP pills:

- CPU, RAM (`/proc/stat`, `/proc/meminfo`);
- network speed (`/proc/net/dev`);
- DC ping (tdesktop MTP DC state);
- proxy state;
- cache size;
- Ghost state;
- own last seen.

Later: weather, fiat/crypto/gold rates, telemetry. Pills are a custom Qt widget
in the search row; data providers are independent of the widget.

### Drawer

tdesktop's own main menu is the drawer; the port does not add a separate
overlay. Accounts and unread badges live in that menu: a single account is
listed as well, and the active account gets a short unread badge
(`unreadWithMentionsBadge`). `Settings::SetupAccounts` exposes this through
`AccountsOptions` (`showSingle`, `badgeActive`); Settings Information keeps the
defaults, so it still hides a single account and shows no active badge.

The accounts section starts expanded by default (after a saved collapse it
starts collapsed), and its state is stored in `OEDrawerAccountsExpanded`
(default `true`), the drawer's actual source of truth. Upstream
`mainMenuAccountsShown` is written both from the main menu and by
`Main::Domain::add` when a second account is added. Known limitation: that
upstream auto-expand and the synchronization of already open main menus are not
reflected in the drawer yet. Menu item order is `OEMainMenuOrder`. Account
order, login, logout and the account switcher stay upstream.

## Not ported

`glyph`, `appicons`, `shortcuts`, `updater`, `camera` (CameraX), Android crash
and jank tooling. Desktop equivalents are separate tasks.

## Plugins

Stage 1 (MVP3): manifest and inspection only.

- Plugins live in `tdata/plugins` (`cWorkingDir() + "tdata/plugins"`).
- Recognized manifests: `.plugin` and `.py`. `.elyx` and `.eaf` are listed
  with an `Unsupported plugin format` error; other extensions and directories
  are ignored. Symlinks are not followed.
- One `python3 -I -B` process per directory parses each file with `ast.parse`
  and reads metadata with `ast.literal_eval`; plugin code is never imported or
  executed.
- Required metadata: `__id__` (validated as `^[A-Za-z][A-Za-z0-9_-]{1,31}$`,
  2–32 characters) and `__name__`; `__description__`, `__author__`,
  `__version__` and `__permissions__` are optional.
- Known permissions: `ui`, `messages.read`, `messages.send`, `network`,
  `files`, `intents`, `settings`, `hooks`, `native`. An unknown permission is
  an error for that entry only.
- Files larger than 2 MiB are rejected per file; process failures (no
  `python3`, timeout, invalid JSON, non-zero exit) become one global error.
- UI: a `Plugins` subsection in the Exteraless settings opens a box with the
  plugin count, metadata, per-file errors or an empty-state message. The box
  has only `Open folder` and `Close`.

Out of scope for stage 1: the separate Python host process, install/uninstall
and enable/disable, IPC between the app and plugins, audit journal, OS
sandbox, the Android compatibility shim, and any `.elyx`/`.eaf` runtime.
Android plugins that use `from java import ...` or Xposed hooks stay
incompatible; the shared Python code (`base_plugin.py`, metadata/capability
scan, audit policy) is reused only when the host process is designed.
