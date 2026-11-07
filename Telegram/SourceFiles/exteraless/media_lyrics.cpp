#include "exteraless/media_lyrics.h"

#include "exteraless/config.h"
#include "exteraless/subtitles.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <optional>

namespace Exteraless {
namespace {

constexpr auto kMaxStoredDocs = 128;

[[nodiscard]] QJsonObject ReadStore() {
	auto error = QJsonParseError{ 0, QJsonParseError::NoError };
	const auto document = QJsonDocument::fromJson(
		Config::Instance().lyricsFiles().toUtf8(),
		&error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return {};
	}
	return document.object();
}

void WriteStore(const QJsonObject &store) {
	Config::Instance().setLyricsFiles(QString::fromUtf8(
		QJsonDocument(store).toJson(QJsonDocument::Compact)));
}

[[nodiscard]] QString StoredPath(const QJsonObject &store, qint64 docId) {
	const auto key = VideoKey(docId);
	if (!MediaLyrics::ValidVideoKey(key)) {
		return {};
	}
	const auto path = store.value(key).toObject().value(
		u"path"_q).toString();
	return QFileInfo(path).isFile() ? path : QString();
}

[[nodiscard]] std::optional<MediaLyrics::State> ReadState(
		const QString &path) {
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return std::nullopt;
	}
	auto lyrics = ParseLyrics(file.readAll());
	if (lyrics.instrumental || lyrics.lines.isEmpty()) {
		return std::nullopt;
	}
	return MediaLyrics::State{
		.path = path,
		.label = QFileInfo(path).fileName(),
		.lyrics = std::move(lyrics),
	};
}

} // namespace

MediaLyrics &MediaLyrics::Instance() {
	static auto instance = MediaLyrics();
	return instance;
}

bool MediaLyrics::ValidVideoKey(const QString &key) {
	if (!key.startsWith(u"doc_"_q)) {
		return false;
	}
	auto ok = false;
	const auto docId = key.mid(4).toLongLong(&ok);
	return ok && docId >= 0 && (VideoKey(docId) == key);
}

MediaLyrics::State *MediaLyrics::find(qint64 docId) {
	if (docId <= 0) {
		return nullptr;
	}
	if (const auto i = _states.find(docId); i != _states.end()) {
		return &i->second;
	}
	const auto path = StoredPath(ReadStore(), docId);
	if (path.isEmpty()) {
		return nullptr;
	}
	auto state = ReadState(path);
	if (!state) {
		return nullptr;
	}
	const auto inserted = _states.emplace(docId, std::move(*state));
	return &inserted.first->second;
}

bool MediaLyrics::load(qint64 docId, const QString &path) {
	if (docId <= 0) {
		return false;
	}
	auto state = ReadState(path);
	if (!state) {
		return false;
	}
	while (!_states.empty() && _states.size() >= kMaxStoredDocs) {
		_states.erase(_states.begin());
	}
	_states[docId] = std::move(*state);
	persist(docId);
	return true;
}

void MediaLyrics::disable(qint64 docId) {
	_states.erase(docId);
	persist(docId);
}

int MediaLyrics::activeIndex(qint64 docId, qint64 positionMs) const {
	const auto i = _states.find(docId);
	if (i == _states.end()) {
		return -1;
	}
	return ActiveIndex(i->second.lyrics, positionMs);
}

QString MediaLyrics::activeText(qint64 docId, qint64 positionMs) const {
	const auto index = activeIndex(docId, positionMs);
	if (index < 0) {
		return QString();
	}
	const auto &lines = _states.find(docId)->second.lyrics.lines;
	return lines[index].text;
}

void MediaLyrics::persist(qint64 docId) const {
	auto store = ReadStore();
	const auto videoKey = VideoKey(docId);
	const auto i = _states.find(docId);
	if (i == _states.end()) {
		if (!store.contains(videoKey)) {
			return;
		}
		store.remove(videoKey);
	} else {
		auto entry = QJsonObject();
		entry[u"path"_q] = i->second.path;
		entry[u"label"_q] = i->second.label;
		store[videoKey] = entry;
	}
	for (const auto &storedKey : store.keys()) {
		if (store.size() <= kMaxStoredDocs) {
			break;
		} else if (storedKey != videoKey) {
			store.remove(storedKey);
		}
	}
	WriteStore(store);
}

} // namespace Exteraless
