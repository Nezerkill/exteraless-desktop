#include "exteraless/media_subtitles.h"

#include "base/basic_types.h"
#include "exteraless/config.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <optional>

namespace Exteraless {
namespace {

constexpr auto kMaxStoredVideos = 128;

[[nodiscard]] QJsonObject ReadStore() {
	auto error = QJsonParseError{ 0, QJsonParseError::NoError };
	const auto document = QJsonDocument::fromJson(
		Config::Instance().subtitlesFiles().toUtf8(),
		&error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return {};
	}
	return document.object();
}

void WriteStore(const QJsonObject &store) {
	Config::Instance().setSubtitlesFiles(QString::fromUtf8(
		QJsonDocument(store).toJson(QJsonDocument::Compact)));
}

[[nodiscard]] QString StoredPath(const QJsonObject &store, qint64 docId) {
	const auto key = VideoKey(docId);
	if (!Subtitles::ValidVideoKey(key)) {
		return {};
	}
	const auto path = store.value(key).toObject().value(
		u"path"_q).toString();
	return QFileInfo(path).isFile() ? path : QString();
}

[[nodiscard]] std::optional<Subtitles::State> ReadState(const QString &path) {
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return std::nullopt;
	}
	auto cues = ParseSubtitles(file.readAll());
	if (cues.isEmpty()) {
		return std::nullopt;
	}
	return Subtitles::State{
		.path = path,
		.label = QFileInfo(path).fileName(),
		.cues = std::move(cues),
	};
}

} // namespace

bool Subtitles::ValidVideoKey(const QString &key) {
	if (!key.startsWith(u"doc_"_q)) {
		return false;
	}
	auto ok = false;
	const auto docId = key.mid(4).toLongLong(&ok);
	return ok && docId >= 0 && (VideoKey(docId) == key);
}

Subtitles::State *Subtitles::find(qint64 docId) {
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

bool Subtitles::load(qint64 docId, const QString &path) {
	auto state = ReadState(path);
	if (!state) {
		return false;
	}
	_states[docId] = std::move(*state);
	persist(docId);
	return true;
}

void Subtitles::disable(qint64 docId) {
	_states.erase(docId);
	persist(docId);
}

QString Subtitles::textAt(qint64 docId, qint64 position) {
	const auto state = find(docId);
	return state ? ActiveCueText(state->cues, position) : QString();
}

void Subtitles::persist(qint64 docId) const {
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
		if (store.size() <= kMaxStoredVideos) {
			break;
		} else if (storedKey != videoKey) {
			store.remove(storedKey);
		}
	}
	WriteStore(store);
}

} // namespace Exteraless
