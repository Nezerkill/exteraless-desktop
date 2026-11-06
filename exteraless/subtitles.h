#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace Exteraless {

struct Cue {
	qint64 start = 0;
	qint64 end = 0;
	QString text;
};

[[nodiscard]] QList<Cue> ParseSubtitles(const QByteArray &data);

[[nodiscard]] QString ActiveCueText(const QList<Cue> &cues, qint64 position);

[[nodiscard]] QString VideoKey(qint64 docId);

} // namespace Exteraless
