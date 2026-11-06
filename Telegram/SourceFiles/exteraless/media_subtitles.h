#pragma once

#include "exteraless/subtitles.h"

#include <QList>
#include <QString>

#include <map>

namespace Exteraless {

class Subtitles final {
public:
	struct State {
		QString path;
		QString label;
		QList<Cue> cues;
	};

	[[nodiscard]] static bool ValidVideoKey(const QString &key);

	State *find(qint64 docId);
	[[nodiscard]] bool load(qint64 docId, const QString &path);
	void disable(qint64 docId);

	[[nodiscard]] QString textAt(qint64 docId, qint64 position);

private:
	void persist(qint64 docId) const;

	std::map<qint64, State> _states;

};

} // namespace Exteraless
