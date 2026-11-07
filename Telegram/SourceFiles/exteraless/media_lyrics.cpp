#include "exteraless/media_lyrics.h"

#include "exteraless/config.h"
#include "exteraless/lrclib.h"
#include "exteraless/subtitles.h"
#include "settings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <optional>

namespace Exteraless {
namespace {

constexpr auto kMaxStoredDocs = 128;
constexpr auto kOnlineCacheTtlMs = qint64(30) * 24 * 60 * 60 * 1000;

[[nodiscard]] QString OnlineCacheDirectory() {
	return cWorkingDir() + u"tdata/lrclib"_q;
}

[[nodiscard]] QString OnlineCachePath(qint64 docId) {
	return OnlineCacheDirectory() + u"/%1.json"_q.arg(docId);
}

[[nodiscard]] bool IsOnlinePath(const QString &path) {
	return path.startsWith(u"lrclib:"_q);
}

[[nodiscard]] QString OnlinePath(qint64 docId) {
	return u"lrclib:%1"_q.arg(docId);
}

[[nodiscard]] qint64 OnlineDocId(const QString &path) {
	auto ok = false;
	const auto docId = path.mid(u"lrclib:"_q.size()).toLongLong(&ok);
	return (ok && docId > 0) ? docId : 0;
}

[[nodiscard]] std::optional<Lyrics> OnlineLyrics(const QByteArray &data) {
	const auto parsed = ParseGet(data);
	if (!parsed.ok || parsed.instrumental || parsed.text.isEmpty()) {
		return std::nullopt;
	}
	if (parsed.synced) {
		auto lyrics = ParseLyrics(parsed.text.toUtf8());
		if (lyrics.instrumental || lyrics.lines.isEmpty()) {
			return std::nullopt;
		}
		return lyrics;
	}
	return Lyrics{ .lines = { Line{ 0, parsed.text } } };
}

[[nodiscard]] std::optional<Lyrics> ReadOnlineCache(qint64 docId) {
	auto file = QFile(OnlineCachePath(docId));
	if (!file.open(QIODevice::ReadOnly)) {
		return std::nullopt;
	}
	return OnlineLyrics(file.readAll());
}

[[nodiscard]] bool OnlineCacheFresh(qint64 docId) {
	const auto info = QFileInfo(OnlineCachePath(docId));
	if (!info.isFile()) {
		return false;
	}
	const auto age = info.lastModified().msecsTo(QDateTime::currentDateTime());
	return age >= 0 && age < kOnlineCacheTtlMs;
}

bool WriteOnlineCache(qint64 docId, const QByteArray &data) {
	if (!QDir().mkpath(OnlineCacheDirectory())) {
		return false;
	}
	auto file = QFile(OnlineCachePath(docId));
	if (!file.open(QIODevice::WriteOnly)) {
		return false;
	}
	return (file.write(data) == data.size());
}

[[nodiscard]] QNetworkAccessManager &NetworkManager() {
	static const auto manager = new QNetworkAccessManager(
		QCoreApplication::instance());
	return *manager;
}

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
	if (IsOnlinePath(path)) {
		return path;
	}
	return QFileInfo(path).isFile() ? path : QString();
}

[[nodiscard]] std::optional<MediaLyrics::State> ReadState(
		const QString &path) {
	if (IsOnlinePath(path)) {
		const auto docId = OnlineDocId(path);
		if (!docId) {
			return std::nullopt;
		}
		auto lyrics = ReadOnlineCache(docId);
		if (!lyrics || lyrics->lines.isEmpty()) {
			return std::nullopt;
		}
		return MediaLyrics::State{
			.path = path,
			.label = u"lrclib.net"_q,
			.lyrics = std::move(*lyrics),
		};
	}
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
	abortPending(docId);
	_states.erase(docId);
	persist(docId);
}

void MediaLyrics::fetchOnline(
		qint64 docId,
		const QString &artist,
		const QString &title,
		int durationSeconds,
		Fn<void(FetchResult)> done) {
	const auto finish = [done = std::move(done)](FetchResult result) {
		if (done) {
			done(std::move(result));
		}
	};
	if (!Config::Instance().lyricsAllowNetwork()) {
		finish({ .error = u"disabled"_q });
		return;
	}
	if (docId <= 0) {
		finish({ .error = u"invalid"_q });
		return;
	}
	if (OnlineCacheFresh(docId)) {
		if (auto cached = ReadOnlineCache(docId)) {
			if (installOnline(docId, std::move(*cached))) {
				finish({ .ok = true });
				return;
			}
		}
	}
	const auto track = title.trimmed();
	if (track.isEmpty()) {
		finish({ .error = u"notfound"_q });
		return;
	}
	const auto performer = artist.trimmed();
	const auto url = QUrl(BuildGetUrl(
		performer,
		track,
		QString(),
		durationSeconds));

	abortPending(docId);
	auto request = QNetworkRequest(url);
	request.setHeader(
		QNetworkRequest::UserAgentHeader,
		u"exteraless-desktop"_q);
	const auto reply = NetworkManager().get(request);
	_pending[docId] = reply;
	QObject::connect(reply, &QObject::destroyed, [=] {
		const auto i = _pending.find(docId);
		if (i != _pending.end() && i->second == reply) {
			_pending.erase(i);
		}
	});
	QObject::connect(reply, &QNetworkReply::finished, reply, [=] {
		reply->deleteLater();
		const auto i = _pending.find(docId);
		if (i != _pending.end() && i->second == reply) {
			_pending.erase(i);
		}
		const auto error = reply->error();
		if (error == QNetworkReply::OperationCanceledError) {
			return;
		} else if (error != QNetworkReply::NoError) {
			finish({ .error = u"network"_q });
			return;
		}
		const auto data = reply->readAll();
		auto lyrics = OnlineLyrics(data);
		if (!lyrics || !installOnline(docId, std::move(*lyrics))) {
			finish({ .error = u"notfound"_q });
			return;
		}
		WriteOnlineCache(docId, data);
		finish({ .ok = true });
	});
}

bool MediaLyrics::installOnline(qint64 docId, Lyrics lyrics) {
	if (docId <= 0 || lyrics.instrumental || lyrics.lines.isEmpty()) {
		return false;
	}
	while (!_states.empty() && _states.size() >= kMaxStoredDocs) {
		_states.erase(_states.begin());
	}
	_states[docId] = State{
		.path = OnlinePath(docId),
		.label = u"lrclib.net"_q,
		.lyrics = std::move(lyrics),
	};
	persist(docId);
	return true;
}

void MediaLyrics::abortPending(qint64 docId) {
	const auto i = _pending.find(docId);
	if (i == _pending.end()) {
		return;
	}
	const auto reply = i->second;
	_pending.erase(i);
	reply->abort();
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
