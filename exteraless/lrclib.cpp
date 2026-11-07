#include "exteraless/lrclib.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QStringList>
#include <QUrl>

namespace Exteraless {
namespace {

constexpr auto kApiUrl = "https://lrclib.net/api/";

[[nodiscard]] QString QueryParam(const QString &name, const QString &value) {
	return name
		+ QLatin1Char('=')
		+ QString::fromLatin1(QUrl::toPercentEncoding(value));
}

[[nodiscard]] QString StringField(
		const QJsonObject &object,
		const QString &key) {
	const auto value = object.value(key);
	return value.isString() ? value.toString() : QString();
}

[[nodiscard]] bool ParseObject(
		const QByteArray &data,
		QJsonObject *object) {
	auto error = QJsonParseError{ 0, QJsonParseError::NoError };
	const auto document = QJsonDocument::fromJson(data, &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return false;
	}
	*object = document.object();
	return true;
}

} // namespace

QString BuildSearchUrl(const QString &query) {
	return QString::fromLatin1(kApiUrl)
		+ QStringLiteral("search?")
		+ QueryParam(QStringLiteral("q"), query);
}

QString BuildGetUrl(
		const QString &artist,
		const QString &track,
		const QString &album,
		int duration) {
	auto params = QStringList();
	params.append(QueryParam(QStringLiteral("artist_name"), artist));
	params.append(QueryParam(QStringLiteral("track_name"), track));
	if (!album.isEmpty()) {
		params.append(QueryParam(QStringLiteral("album_name"), album));
	}
	if (duration > 0) {
		params.append(
			QueryParam(
				QStringLiteral("duration"),
				QString::number(duration)));
	}
	return QString::fromLatin1(kApiUrl)
		+ QStringLiteral("get?")
		+ params.join(QLatin1Char('&'));
}

QList<LrclibResult> ParseSearch(const QByteArray &data) {
	auto result = QList<LrclibResult>();
	auto error = QJsonParseError{ 0, QJsonParseError::NoError };
	const auto document = QJsonDocument::fromJson(data, &error);
	if (error.error != QJsonParseError::NoError || !document.isArray()) {
		return result;
	}
	for (const auto &entry : document.array()) {
		if (!entry.isObject()) {
			continue;
		}
		const auto object = entry.toObject();
		auto item = LrclibResult();
		item.id = object.value(QStringLiteral("id"))
			.toVariant()
			.toLongLong();
		item.track = StringField(object, QStringLiteral("trackName"));
		item.artist = StringField(object, QStringLiteral("artistName"));
		item.hasSynced = !StringField(
			object,
			QStringLiteral("syncedLyrics")).isEmpty();
		result.append(item);
	}
	return result;
}

LrclibLyrics ParseGet(const QByteArray &data) {
	auto result = LrclibLyrics();
	auto object = QJsonObject();
	if (!ParseObject(data, &object)) {
		return result;
	}
	if (object.value(QStringLiteral("instrumental")).toBool()) {
		result.instrumental = true;
		return result;
	}
	const auto synced = StringField(object, QStringLiteral("syncedLyrics"));
	if (!synced.isEmpty()) {
		result.ok = true;
		result.synced = true;
		result.text = synced;
		return result;
	}
	const auto plain = StringField(object, QStringLiteral("plainLyrics"));
	if (!plain.isEmpty()) {
		result.ok = true;
		result.text = plain;
	}
	return result;
}

} // namespace Exteraless
