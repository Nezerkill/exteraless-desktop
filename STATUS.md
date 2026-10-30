# Статус порта Exteraless Desktop

Обновлено: 2026-10-30.

## Готово

### M0. База

- Форк `telegramdesktop/tdesktop` 7.3.1 в `/home/nezerkill/exteraless-desktop`.
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
- `nix build .#exteraless-desktop` — собрано.
- Release smoke-тест: `result/bin/Telegram` запускается под Xvfb.
- Debug smoke-тест: `out/bin/Telegram` запускается под Xvfb.

### Артефакты

- Коммиты: `4d4d258`, `86ef5c7`, `509c211`, `fa63bf1`.
- Nix-пакет: `/nix/store/pxcrrn79wmvgv61yxbab2pxayzgdshaz-exteraless-desktop-7.3.1-exteraless.0.1`
- Локальная Debug-сборка: `out/bin/Telegram`.

## В работе / дальше

### MVP2

- [x] Ghost Mode: read receipts, stories, online/offline, typing/upload progress — код готов, идёт сборка/проверка.
- [x] Сохранение удалённых сообщений: JSON-sidecar, контекстное меню и бокс — код готов, идёт сборка/проверка.
- [ ] Per-chat исключения Ghost Mode (ключи конфига уже есть).
- [ ] Pill Stack: CPU/RAM, сеть, DC ping, proxy, cache, ghost, last seen — исследование готово.
- [ ] Drawer: аккаунты, меню, unread-бейджи, порядок пунктов — исследование готово.

### MVP3

- [ ] Python-плагины: отдельный host-процесс, манифест, разрешения, аудит, sandbox.
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
- Ghost Mode и удалённые сообщения ещё не реализованы.

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
