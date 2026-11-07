#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace Exteraless {

struct Line {
	qint64 ms = 0;
	QString text;
};

struct Lyrics {
	QList<Line> lines;
	bool synced = false;
	bool instrumental = false;
};

[[nodiscard]] Lyrics ParseLyrics(const QByteArray &data);

[[nodiscard]] int ActiveIndex(const Lyrics &lyrics, qint64 positionMs);

} // namespace Exteraless
