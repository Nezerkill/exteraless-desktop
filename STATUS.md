# Статус порта Exteraless Desktop

Обновлено: 2026-11-04.

## Готово

### M0. База

- Форк `telegramdesktop/tdesktop` 7.3.1 в `~/exteraless-desktop`.
- Брендинг: `Exteraless Desktop`, отдельное имя приложения и data-каталог.
- Сборка под Linux (Debug и Release).
- Smoke-тест под Xvfb: окно, QRhi/OpenGL, подключение к DC.
- Flake, пакет `exteraless-desktop`, NixOS-модуль, CI-workflow.

### M1. Настройки, прокси, backup

- Раздел **Exteraless** в настройках tdesktop.
- `Relative last seen`: абсолютное/относительное время последнего визита.
- `Comma after mention`: запятая после упоминания в автодополнении и контекстном меню.
- Авто-отключение прокси по VPN / мобильной сети / Wi-Fi с восстановлением после смены сети.
- Импорт/экспорт `.extera` с Android-совместимым кодированием невидимыми символами.
- Конфиг: `~/.local/share/ExteralessDesktop/tdata/exteraless.json`.
- Nix-пакет собирает upstream tdesktop и применяет `nix/patches/exteraless.patch`.

### Проверки

- `nix flake check --no-build` — проходит.
- `nix build .#exteraless-desktop` — собрано, включая Pill Stack.
- Release smoke-тест: `result/bin/Telegram` запускается под Xvfb.
- Debug smoke-тест: `out/bin/Telegram` запускается под Xvfb.
- Pill Stack smoke-тест: `out/bin/Telegram` и `result/bin/Telegram` с `OEPillStackEnabled=true` под Xvfb, скриншот 1280x1024.
- Pill Stack selection: с `OEPillStackPills=3` показываются только CPU и RAM.
- Per-chat Ghost exclusions: подменю и запись в `OEGhostReadExclusions` проверены под Xvfb; сетевой эффект отдельно не измерялся.
- Drawer order: под Xvfb задан порядок `ghost,exteraless,pillstack,proxy,storage`, drag даёт `exteraless,pillstack,proxy,ghost,storage`; запись в `exteraless.json` и сохранение после перезапуска подтверждены.
- Drawer accounts — проверено под Xvfb на итоговом бинарнике:
  - свежий профиль без ключа: секция стартует раскрытой, строка единственного
    аккаунта видна, активный аккаунт получает короткий unread badge `128.4K`;
  - Settings Information при одном аккаунте строку не показывает и бейдж
    активного аккаунта не включает;
  - collapse/expand пишет в `exteraless.json` `false`/`true` и переживает
    restart.
- Финальная полная Ninja-сборка завершила компиляцию, финальный линк выполнен
  LLD из-за debug-info overflow у GNU ld; `nix flake check --no-build`
  проходит.
- Drawer accounts, поведение по коду (в профиле smoke один аккаунт): бейджи
  неактивных аккаунтов сохраняются — `badgeActive` лишь добавляет бейдж
  активному и не снимает остальные.
- Плагины, этап 1, backend: `exteraless/tests/plugins/run_checks.py` —
  32 проверки PASS на текущем CLI; покрыты фикстуры, рекурсия, лимит 2 MiB,
  отсутствие `python3`, невалидный JSON, ненулевой exit, таймаут, один
  процесс на каталог, отсутствие исполнения и пропуск symlink/каталогов/`.txt`.
- Плагины, этап 1, UI под Xvfb: непустой каталог — `Plugins (6)`, метаданные
  и per-file ошибки; пустой — `Plugins (0)` и `No plugins in ...`; файл с
  исполняемым кодом показан как `Exec Check`, маркер исполнения не создан.
- Плагины, этап 1, сборка: изменённые TU компилируются, бинарник слинкован
  LLD 21.1.8 (GNU ld упирается в debug-info overflow), `out/bin/Telegram`
  sha256 `7559fd57…`, `ninja -C out -n Telegram` не показывает compile/link
  edges кроме glob/CMake re-run.
- `nix/patches/exteraless.patch` применяется к чистому tdesktop 7.3.1.

### Артефакты

- История разработки — в git репозитория.
- Nix-пакет не пересобран после per-chat Ghost exclusions, выбора пилюль, drawer order и drawer accounts.
- Nix-пакет: локальная сборка flake (`./result`), store-путь не фиксируется.
- Локальная Debug-сборка: `out/bin/Telegram`.

## В работе / дальше

### MVP2

- [x] Ghost Mode: read receipts, stories, online/offline, typing/upload progress.
- [x] Сохранение удалённых сообщений: JSON-sidecar, контекстное меню и бокс.
- [x] Per-chat исключения Ghost Mode: read receipts и typing через контекстное меню.
- [x] Pill Stack: CPU/RAM, сеть, DC ping, proxy, cache, ghost, last seen; выбор видимых пилюль.
- [x] Drawer: пункты Exteraless в главном меню (Ghost, Pill Stack, proxy, cache).
- [x] Drawer: порядок пунктов в главном меню (`OEMainMenuOrder`, drag-and-drop).
- [x] Drawer: аккаунты и unread-бейджи.

### MVP3

- [ ] Python-плагины: отдельный host-процесс, манифест, разрешения, аудит, sandbox.
- [x] Плагины, этап 1: манифест и просмотр (`tdata/plugins`, AST без исполнения, разрешения, ошибки, бокс).
- [ ] Совместимость с частью Android-плагинов через shim.

### После MVP

- [ ] AI-клиент.
- [ ] Feed.
- [ ] Плеер и now playing/MPRIS.
- [ ] Иконпаки.
- [ ] Vosk/распознавание речи.
- [ ] Flatpak/AppImage.

## Ограничения

- Пакет по умолчанию использует API-ключи Snap-сборки tdesktop; для своего
  дистрибутива нужны собственные `apiId`/`apiHash`.
- `exteraless.json` сейчас привязан к `QStandardPaths::AppLocalDataLocation`
  и не учитывает `-workdir`.
- Плагины Android-версии несовместимы: нужен новый desktop API.

## Сборка с ограничением ОЗУ

На машине 16 ГБ RAM без свапа:

```bash
nix develop
cmake -S . -B out -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DTDESKTOP_API_ID=YOUR_API_ID \
  -DTDESKTOP_API_HASH=YOUR_API_HASH
cmake --build out --target Telegram -j3

nix build .#exteraless-desktop --cores 4 --max-jobs 1
```

Для длинных сборок используется systemd-юнит с `MemoryMax`, чтобы не
словить OOM.

## Запуск

```bash
cd ~/exteraless-desktop
./result/bin/Telegram
# или
nix run .#exteraless-desktop
```

Из Android-репозитория `~/exteraless` запускать нельзя: flake лежит в
`~/exteraless-desktop`.
