#pragma once

#include "exteraless/lyrics.h"

#include <QString>

#include <map>

namespace Exteraless {

class MediaLyrics final {
public:
	struct State {
		QString path;
		QString label;
		Lyrics lyrics;
	};

	[[nodiscard]] static MediaLyrics &Instance();

	[[nodiscard]] static bool ValidVideoKey(const QString &key);

	[[nodiscard]] State *find(qint64 docId);
	[[nodiscard]] bool load(qint64 docId, const QString &path);
	void disable(qint64 docId);

	[[nodiscard]] int activeIndex(qint64 docId, qint64 positionMs) const;
	[[nodiscard]] QString activeText(qint64 docId, qint64 positionMs) const;

private:
	void persist(qint64 docId) const;

	std::map<qint64, State> _states;

};

} // namespace Exteraless
