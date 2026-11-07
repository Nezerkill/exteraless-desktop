#pragma once

#include "exteraless/lyrics.h"

#include "base/basic_types.h"

#include <QString>

#include <map>

class QNetworkReply;

namespace Exteraless {

class MediaLyrics final {
public:
	struct State {
		QString path;
		QString label;
		Lyrics lyrics;
	};

	struct FetchResult {
		bool ok = false;
		QString error;
	};

	[[nodiscard]] static MediaLyrics &Instance();

	[[nodiscard]] static bool ValidVideoKey(const QString &key);

	[[nodiscard]] State *find(qint64 docId);
	[[nodiscard]] bool load(qint64 docId, const QString &path);
	void disable(qint64 docId);

	void fetchOnline(
		qint64 docId,
		const QString &artist,
		const QString &title,
		int durationSeconds,
		Fn<void(FetchResult)> done);

	[[nodiscard]] int activeIndex(qint64 docId, qint64 positionMs) const;
	[[nodiscard]] QString activeText(qint64 docId, qint64 positionMs) const;

private:
	[[nodiscard]] bool installOnline(qint64 docId, Lyrics lyrics);
	void abortPending(qint64 docId);
	void persist(qint64 docId) const;

	std::map<qint64, State> _states;
	std::map<qint64, QNetworkReply*> _pending;

};

} // namespace Exteraless
