#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace Exteraless {

struct LrclibResult {
	qint64 id = 0;
	QString track;
	QString artist;
	bool hasSynced = false;
};

struct LrclibLyrics {
	bool ok = false;
	bool synced = false;
	bool instrumental = false;
	QString text;
};

[[nodiscard]] QString BuildSearchUrl(const QString &query);
[[nodiscard]] QString BuildGetUrl(
	const QString &artist,
	const QString &track,
	const QString &album,
	int duration);
[[nodiscard]] QList<LrclibResult> ParseSearch(const QByteArray &data);
[[nodiscard]] LrclibLyrics ParseGet(const QByteArray &data);

} // namespace Exteraless
