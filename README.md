# Exteraless Desktop

Linux-клиент Telegram на базе [Telegram Desktop](https://github.com/telegramdesktop/tdesktop),
в который переносятся возможности Android-клиента
[exteraless](https://github.com/exteraless/exteraless).

Сейчас это ранний каркас: сборка, брендинг и Nix-упаковка. Функции exteraless
добавляются поэтапно.

Текущий статус и список готового: [STATUS.md](STATUS.md).

## Состояние

- [x] Форк tdesktop 7.3.1
- [x] Сборка под Linux
- [x] Брендинг приложения
- [x] Flake, пакет и NixOS-модуль
- [x] Раздел настроек Exteraless
- [x] Запятая после упоминания
- [x] Авто-отключение прокси по типу сети
- [x] Импорт/экспорт `.extera`
- [ ] Ghost Mode и сохранение удалённых сообщений
- [ ] Pill Stack и drawer
- [ ] Движок Python-плагинов с моделью разрешений

## Сборка

Для сборки нужны Nix и собственные `api_id`/`api_hash` из
[my.telegram.org](https://my.telegram.org/auth).

```bash
nix develop
cmake -S . -B out -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DTDESKTOP_API_ID=YOUR_API_ID \
  -DTDESKTOP_API_HASH=YOUR_API_HASH
cmake --build out --target Telegram -j"$(nproc)"
./out/Telegram
```

Debug-сборка занимает меньше времени, чем Release. Для локальной проверки можно
использовать тестовые ключи tdesktop (`-DTDESKTOP_API_TEST=ON`), но для реального
входа нужны свои.

На машинах с 16 ГБ RAM собирайте не более чем в 4 потока — один процесс
`cc1plus` с PCH занимает около гигабайта:

```bash
cmake --build out --target Telegram -j3
nix build .#exteraless-desktop --cores 3 --max-jobs 1
```

## NixOS

```bash
nix build .#exteraless-desktop
./result/bin/Telegram
```

Модуль NixOS:

```nix
{
  inputs.exteraless-desktop.url = "github:exteraless/exteraless-desktop";

  outputs = { self, nixpkgs, exteraless-desktop, ... }: {
    nixosConfigurations.host = nixpkgs.lib.nixosSystem {
      modules = [
        exteraless-desktop.nixosModules.default
        {
          programs.exteraless-desktop.enable = true;
        }
      ];
    };
  };
}
```

Пакет по умолчанию использует API-ключи Snap-сборки tdesktop. Для своего
дистрибутива передай `apiId` и `apiHash`:

```nix
exteraless-desktop.packages.x86_64-linux.default.override {
  apiId = "YOUR_API_ID";
  apiHash = "YOUR_API_HASH";
}
```

## Разработка

Nix-пакет собирает upstream tdesktop и применяет
`nix/patches/exteraless.patch`. После изменений в исходниках обнови патч:

```bash
git add -N exteraless
git diff --binary -- CMakeLists.txt Telegram exteraless > nix/patches/exteraless.patch
```

Патч должен применяться к чистому исходнику tdesktop той же версии.

## Лицензия

GPL-3.0. Оригинальный tdesktop распространяется под GPL-3.0 с исключением
OpenSSL; см. `LICENSE` и `LEGAL`.
