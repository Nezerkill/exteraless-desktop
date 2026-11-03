# Благодарности

Exteraless Desktop — форк [Telegram Desktop](https://github.com/telegramdesktop/tdesktop),
в который переносятся возможности Android-клиента
[exteraless](https://github.com/exteraless/exteraless). Ниже — происхождение
проекта, благодарности авторам и границы заимствований.

Список проектов сверен с [README exteraless на коммите
`4771c553`](https://github.com/exteraless/exteraless/blob/4771c553fee84618f8fa8dfc034c9e85317c2c72/README.md).
Файл `LICENSE` в репозитории exteraless — канонический текст GNU GPL v3.0.
Exteraless Desktop распространяется под GPL-3.0, как у exteraless; дополнительно
сохранено исключение OpenSSL из upstream Telegram Desktop (`LICENSE`, `LEGAL`).

Отдельная благодарность — авторам и участникам Android-клиента exteraless:
без их работы у Exteraless Desktop не было бы переносимых функций.

## Прямая цепочка

Кодовая цепочка Android-клиента:

Telegram для Android → Telegram-FOSS → NekoX → Nagram → NagramX → exteraless.

- [Telegram для Android](https://github.com/DrKLO/Telegram) — официальный
  Android-клиент, авторские права Nikolai Kudashov; исходное основание цепочки.
- [Telegram-FOSS](https://github.com/Telegram-FOSS-Team/Telegram-FOSS) —
  FOSS-форк официального Telegram для Android, на котором основан NekoX.
- [NekoX](https://github.com/NekoX-Dev/NekoX) — сторонний клиент на исходниках
  Telegram-FOSS, на котором основан Nagram.
- [Nagram](https://github.com/NextAlone/Nagram) — сторонний Android-клиент,
  основанный на NekoX.
- [NagramX](https://github.com/risin42/NagramX) — вариант Nagram с
  дополнительными функциями; непосредственная база Android-клиента exteraless
  (не всей цепочки).
- [exteraless](https://github.com/exteraless/exteraless) — Android-клиент, из
  которого в Exteraless Desktop переносятся функции. В нём появились открытая
  реализация функций exteraGram и модель разрешений плагинов.

Десктопная основа:

- [Telegram Desktop](https://github.com/telegramdesktop/tdesktop) — официальный
  десктоп-клиент, база Exteraless Desktop (форк 7.3.1).

## Проекты из README exteraless

Все девять пунктов списка exteraless из README перечислены:
Nagram и NagramX — в прямой цепочке выше, остальные семь — ниже.

- [exteraGram](https://github.com/exteraSquad/exteraGram) — Android-клиент, чьи
  функции и оформление exteraless реализует открыто; источник функций и
  дизайна, а не заявленная кодовая база Exteraless Desktop.
- [Nekogram](https://github.com/Nekogram/Nekogram) — сторонний Android-клиент;
  его функции вошли в цепочку через NekoX/Nagram и учтены в благодарностях
  exteraless.
- [AyuGram](https://github.com/AyuGram/AyuGram4A) — проект из благодарностей
  exteraless и NagramX.
- [NotSoAndroidAyuGram](https://github.com/Dr4iv3rNope/NotSoAndroidAyuGram) —
  проект Dr4iv3rNope из благодарностей exteraless и NagramX.
- [Cherrygram](https://github.com/arsLan4k1390/Cherrygram) — проект из
  благодарностей exteraless и NagramX.
- [OctoGram](https://github.com/OctoGramApp/OctoGram) — сторонний Android-клиент
  из благодарностей exteraless и NagramX.
- [Inugram](https://github.com/teidesu/inugram) — проект из благодарностей
  exteraless.

## Дизайн

Иконки и оформление Android-клиента exteraless унаследованы от exteraGram и
созданы дизайнером [@the8055u](https://t.me/the8055u) и студией
[@BlueprintDsgn](https://t.me/BlueprintDsgn). Права на эти материалы
принадлежат авторам. Кредиты относятся к оформлению Android-клиента
exteraless/exteraGram, а не к оформлению Exteraless Desktop.

## Границы заимствований и сторонние компоненты

Прямая кодовая преемственность подтверждена только для цепочки Telegram для
Android → Telegram-FOSS → NekoX → Nagram → NagramX → exteraless и для
Telegram Desktop → Exteraless Desktop. Остальные проекты перечислены как
источники идей, функций и оформления; этот документ не утверждает, что их код
скопирован.

Оригинальные уведомления об авторских правах и лицензиях сторонних компонентов
сохраняются в исходных файлах и каталогах репозитория; этот документ их не
заменяет и не перелицензирует. Exteraless Desktop переносит функции
Android-клиента exteraless, который остаётся отдельным репозиторием со своими
уведомлениями и лицензией.
