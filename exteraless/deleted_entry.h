#pragma once

#include <QtGlobal>

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace Exteraless {

struct DeletedEntryEntity {
	int type = 0;
	int offset = 0;
	int length = 0;
	QString data;
};

struct DeletedEntry {
	qint64 peerId = 0;
	QString peerName;
	qint64 msgId = 0;
	qint64 fromId = 0;
	QString fromName;
	qint64 date = 0;
	qint64 editedDate = 0;
	QString text;
	QString media;
	QString replyToText;
	QString forwardedFrom;
	QStringList reactions;
	std::vector<DeletedEntryEntity> entities;
	qint64 replyToMsgId = 0;
	qint64 forwardedDate = 0;
	bool out = false;
	bool read = true;
};

[[nodiscard]] QJsonObject SerializeDeletedEntry(const DeletedEntry &entry);
[[nodiscard]] std::optional<DeletedEntry> DeserializeDeletedEntry(
	const QJsonObject &object);

} // namespace Exteraless
