# Feature mapping: exteraless Android → Exteraless Desktop

This document fixes what "port" means for the MVP. Android UI is not copied
literally; the behavior and the settings schema are reproduced in tdesktop's
Qt interface. The full inventory and prioritized backlog live in
`docs/nagram-feature-audit.md` and `docs/nagram-porting-backlog.md`.

## MVP1: base, settings, proxy, backup

Implemented:

- `Exteraless` settings section with `Relative last seen` and
  `Comma after mention`;
- per-network proxy auto-disable (VPN, mobile data, Wi-Fi);
- `.extera` import/export with the Android invisible-Unicode envelope;
- JSON config at `tdata/exteraless.json` under the working directory, so it
  follows `-workdir`; without that option it is the app data directory. On the
  first run with a different working directory the old file is copied, not
  moved, when the new one does not exist yet.

### Settings

New `Exteraless` section in tdesktop Settings with subsections:

- **General**
  - relative last seen (`OERelativeLastSeen`, default `false`);
  - comma after mention (`OEAddCommaAfterMention`, default `true`);
  - exact counters instead of `K/M/B` (`OEGeneralDisableNumberRounding`,
    default `false`);
  - show seconds in timestamps (`OEGeneralShowSeconds`, default `false`);
  - hide phone number in profile (`OEGeneralHidePhone`, default `false`).
- **Appearance** — not ported: section radius, separate headers and square FAB
  have no desktop analog, because tdesktop rounds and arranges those surfaces
  with its own styles, and no `OEAppearance*` key is implemented. Avatar
  corners are not implemented either, and the navigation drawer is not a
  setting: tdesktop's main menu is the drawer itself (see Drawer below), there
  is no `OEAppearanceNavigationDrawer` key.
- **Chats**
  - remove message tail (`OEChatsRemoveMessageTail`, default `true`);
  - hide reactions in channels (`OEChatsHideReactionsChannels`, default
    `false`), in groups and supergroups (`OEChatsHideReactionsGroups`, default
    `false`) and in private chats and bots (`OEChatsHideReactionsPrivate`,
    default `false`);
  - strip tracking on open (`OEChatsStripTrackingOnOpen`, default `true`);
  - strip tracking on paste (`OEChatsStripTrackingOnPaste`, default `true`);
  - compact unread messages label (`OEChatsUnreadCapsule`, default `false`);
  - save deleted messages (`OESaveDeletedMessages`, default `false`);
  - visible message menu items: 11 checkboxes under "Visible message menu items"
    (`OEChatsMenuReply`, `Edit`, `Pin`, `CopyText`, `CopyLink`, `CopyImage`,
    `Forward`, `Select`, `Translate`, `Report`, `Delete`), all default `true`;
  - double tap action separately for incoming (`OEChatsDoubleTapIn`) and
    outgoing (`OEChatsDoubleTapOut`) messages, default `-1` (use the Telegram
    setting); `0` = do nothing, `1`/`2` = send the favorite reaction, `4` =
    reply. Other Android actions are tracked as NB-91;
  - sticker time `As usual` / `Beside sticker` / `Hidden`
    (`OEChatsStickerTimeSide`, `OEChatsHideStickerTime`, both default
    `false`); the side timestamp stays clickable;
  - pill order: drag pills on the strip above the chat list; the order is
    saved in `OEPillStackOrder` and survives a restart.
- **Proxy** — see below.
- **Backup** — import/export `.extera`, plus reset of all Exteraless settings
  with confirmation.

`OEGeneralDisableNumberRounding` affects counters formatted through
`Lang::FormatCountToShort`, so views, replies, reaction counts and other shared
shortened counters show exact numbers. Counters with hand-written
`K`/`M` formatting and counters that already use exact formatting stay
unchanged. The setting is exported/imported as the Android backup key
`disableNumberRounding`.

Desktop behavior of the Chats keys:

- `OEChatsRemoveMessageTail` and `OEChatsHideReactions*` are read when a chat
  view is built, so a change applies after the chat is closed and reopened, or
  after a restart. Reactions in the pinned bar can keep their old state until
  a restart. Hiding reactions removes the reactions list and its click target;
  server-side reactions and counters are not changed, and no overlay or extra
  box is added. The unread capsule is read on every paint and applies without
  reopening the chat.
- `OEChatsStripTrackingOnOpen` applies only to clicked `http`/`https` links;
  `OEChatsStripTrackingOnPaste` applies to plain-text/URL paste without
  `text/html`, preserving rich-text formatting. Both are desktop-only keys and
  are exported under their `OE*` names.
- `OEChatsUnreadCapsule` draws the unread bar text as a centered rounded
  capsule and omits the full-width background and separator line. The bar
  height, margins, scrolling geometry and accessibility stay unchanged. It is
  a desktop-only key and is exported under its `OE*` name.
- The tracking allowlist is conservative: the `utm_*` family plus `fbclid`,
  `gclid`, `dclid`, `gbraid`, `wbraid`, `msclkid`, `yclid`, `twclid`,
  `ttclid`, `igshid`, `li_fat_id`, `mc_cid`, `mc_eid`, `_hsenc`, `_hsmi`,
  `mkt_tok`, `rb_clickid`, `s_cid`, `vero_id`, `wickedid`, `oly_anon_id` and
  `oly_enc_id`. Ambiguous parameters such as `ref`, `source`, `s`, `si`,
  `start`, `feature`, `share`, `trk`, `ns_campaign`, `pk_campaign`,
  `at_medium`, `spm`, `sc_cid` and `WT.mc_id` are kept, and percent-encoded
  names are never decoded.
- `.extera` backup carries all 49 desktop keys in three sections
  (`exteraconfig` 44, `pillstackconfig` 3, `mainconfig` 2); the old
  `hideReactionsInPrivate` name is reported as unknown on import.
- `OEChatsWideChannelPosts` is not ported yet, so no such key exists.
- Verification: the chats runner has 292 checks, the plugin manifest runner 32,
  and the plugin host runner 213; Xvfb smoke covered the Chats subsection,
  tail on/off, reactions hidden in a channel and in a group, and the unread
  capsule toggle. The smoke profile did not expose an unread divider, so the
  capsule paint itself was not captured; it is covered by build/code review.
  Private-chat reactions were verified only by the CLI, because the smoke
  profile had no private chat with reactions.

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

`.extera` is a JSON document wrapped in invisible Unicode characters. It
contains all 49 desktop keys in three sections: `exteraconfig` (44 keys),
`pillstackconfig` (3 keys) and `mainconfig` (2 keys). Import validates every
known key, applies only the right type and range, and reports the rest as
unknown; entries in `pillstackconfig`/`mainconfig` and unknown top-level
sections are reported with their section name. `OEProxyAutoDisabled` is
exported as a snapshot but reset on import because it derives from the current
network. Plugin states are included as enable flags only; plugin files and
Telegram account data are not included. Importing a backup enables the plugin
ids listed in it, so only import files you trust. Treat the file as sensitive:
Ghost exclusions contain peer ids.

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
non-Ghost requests. The main menu item toggles all four switches in one click
and shows `On` / `Off` / `Partial` when `OEGhostShowStatus` is enabled
(`showGhostModeStatus` in `.extera`); `IsGhostActive` is strict (all four).
Each of the four switches (plus the offline-after-online one) has a lock
(`OEGhostSend*Locked`): a locked switch is skipped by the batch toggle and
excluded from the `On/Off/Partial` calculation.

### Deleted messages

AyuGram stores deleted messages in a separate local database
(`DeletedMessage`, `DeletedMessageFull`, `DeletedMessageDao`). Desktop keeps a
per-account JSON sidecar in the cache path keyed by peer and message id.
Recording is off by default (`OESaveDeletedMessages`, toggle in
Exteraless → Chats), capped at 1000 entries with 100 per peer, and rotated on
add and load. Since schema v2 each entry stores 17 fields: reactions, reply
id and quoted text, forward source and date, text entities, edited date and a
read flag, next to the basic author/date/text/media fields. Old v1 files load
with defaults. On item destruction the message is saved before the local
history is cleared. UI: a per-peer list from the chat context menu showing
reactions and reply/forward markers; a deleted-message mark in the chat is not
implemented yet (NB-20). Import of the old Android database is not part of
MVP2.

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
and jank tooling. Desktop equivalents are separate tasks. The `OEAppearance*`
keys (section radius, separate headers, square FAB, avatar corners) have no
desktop analog and are not ported.

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
  plugin count, metadata, per-file errors or an empty-state message.

Stage 2 (MVP3): controlled host for enabled plugins.

- `OEPluginStates` in `tdata/exteraless.json` is the preference: `id -> true`,
  default `false`, only valid ids and boolean `true` survive sanitizing. The
  runtime status is shown separately from the toggle.
- One host process per application: `bwrap` runs `python3 -I -B -c <embedded
  script> /plugins`, with no shell and no per-plugin process. IPC is UTF-8
  JSON lines with `seq` and `ok`: `load`/`unload`/`shutdown`, host
  `ready`/`loaded`/`unloaded`/`error`/`audit`/`log`. Responses must carry the
  request `seq` and `ok: true`, otherwise the host is treated as broken.
- bwrap profile: `--unshare-all --unshare-user --disable-userns
  --die-with-parent --new-session --cap-drop ALL`, private `/proc`, `/dev` and
  `/tmp`, `--clearenv`, read-only `/` and `/plugins`, read-only `/nix`,
  `/run/current-system/sw`, `/usr`, `/bin`, `/lib` and selected `/etc` files,
  `HOME=/tmp`, a minimal `PATH`, and no network. The plugins directory is
  bound read-only at `/plugins`; plugin paths in IPC are `/plugins/<file>`.
- Before installing the audit hook the host joins its own session keyring
  (`KEYCTL_JOIN_SESSION_KEYRING`) best-effort; reading `/proc/keys` from a
  plugin is denied, so the inherited host keyring is not exposed.
- The Python audit hook is a best-effort policy layer over a known set of
  audit events, not a security boundary: network, subprocess, `os.system`,
  exec/fork/spawn, `ctypes`, `mmap`, explicit file descriptors, writes outside
  `/plugins`, reads outside `/plugins` and the Python stdlib, `sys.addaudithook`,
  `gc` introspection, trace/profile, `fcntl`, foreign imports and host keyring
  access. Denied events are appended to `tdata/plugins-audit.log` as JSON
  lines (`time`, `kind`, `plugin`, `event`, `detail`) and raise
  `PermissionError`. The hook can be disabled by a plugin: the host script
  runs as `__main__`, so `import __main__; __main__._IN_AUDIT[0] = True`
  turns every check off, and a plugin may also mutate other host globals.
  bwrap remains the actual boundary.
- Opening the Plugins box loads persisted enabled plugins; app startup never
  starts the host, closing the box keeps it running, and app exit stops it.
  Toggles call load/unload first and then persist the preference, so a failed
  load is shown as `Enabled (not running): <error>` and retried on the next
  open.
- Path rules: only a direct regular child of `tdata/plugins`, only
  `.plugin`/`.py`, canonical path required, no symlinks or `..`; C++ validates
  the path and the host re-checks the parent, `O_NOFOLLOW`, `fstat` and the
  2 MiB limit before reading the file.
- Manifest permissions (`network`, `native`, `files`, ...) are informational
  in stage 2 and grant no capabilities; one audit policy applies to every
  plugin.

### Plugin security boundaries

- bwrap is Linux-only; when `bwrap` is unavailable the host fails closed with
  `Plugin sandbox is unavailable.` and no plugin code runs. There is no
  non-Linux fallback.
- The audit hook is a best-effort policy layer, not an OS sandbox and not a
  security boundary: native code, inherited descriptors, non-audited C paths
  and best-effort stdlib filename exceptions can bypass it. A plugin can also
  disable it outright, because the host script is `__main__` and exposes its
  state: `import __main__; __main__._IN_AUDIT[0] = True` makes the hook return
  immediately (after that a plugin can create sockets or write files that the
  hook would have denied; the bwrap mounts and network namespace still apply).
  Do not treat deny-by-default as an enforced guarantee.
- Threads are currently allowed: `threading.Thread` is not an audit event,
  and Python 3.14 starts threads through `_thread.start_joinable_thread`,
  which is not in the deny set. The `threading.Thread` deny entry is dead,
  and only a direct `_thread.start_new_thread` call is denied.
- No seccomp or landlock profile is applied, `/nix` stays readable read-only
  inside the sandbox, and the keyring join is best-effort rather than full
  keyring isolation.
- One host serves all plugins: a timeout or crash kills it and unloads every
  plugin. UI load/unload calls are synchronous and can block the main thread
  for up to 10 seconds; there is no background queue.
- The Nix package wraps `bin/Telegram` with `wrapProgram` so `bubblewrap` and
  `python3` are on `PATH`; it does not yet have a full build after stage 2,
  only `postFixup` evaluation, `nix flake check --no-build` and patch-apply
  checks.
- Out of scope: install/remove, repository/updates, audit log viewer and
  rotation, permissions enforcement, multi-file plugin packages, and the
  Android `.elyx`/`.eaf` shim. Android plugins using `from java import ...` or
  Xposed hooks stay incompatible.
