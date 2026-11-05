#include "exteraless/deleted_entry.h"

#include <QJsonArray>

namespace Exteraless {
namespace {

// WHY: mirrors the last EntityType value; the sidecar must not depend on lib_ui.
constexpr auto kMaxEntityType = 28;

[[nodiscard]] QString IdString(qint64 value) {
	return QString::number(value);
}

[[nodiscard]] qint64 IdValue(const QJsonObject &object, const char *key) {
	return object.value(key).toString().toLongLong();
}

} // namespace

QJsonObject SerializeDeletedEntry(const DeletedEntry &entry) {
	auto object = QJsonObject();
	object["peerId"] = IdString(entry.peerId);
	object["peerName"] = entry.peerName;
	object["msgId"] = IdString(entry.msgId);
	object["fromId"] = IdString(entry.fromId);
	object["fromName"] = entry.fromName;
	object["date"] = double(entry.date);
	object["editedDate"] = double(entry.editedDate);
	object["text"] = entry.text;
	object["media"] = entry.media;
	object["out"] = entry.out;
	object["read"] = entry.read;
	object["replyToMsgId"] = IdString(entry.replyToMsgId);
	object["replyToText"] = entry.replyToText;
	object["forwardedFrom"] = entry.forwardedFrom;
	object["forwardedDate"] = double(entry.forwardedDate);
	object["reactions"] = QJsonArray::fromStringList(entry.reactions);
	auto entities = QJsonArray();
	for (const auto &entity : entry.entities) {
		entities.append(QJsonObject{
			{ "type", entity.type },
			{ "offset", entity.offset },
			{ "length", entity.length },
			{ "data", entity.data },
		});
	}
	object["entities"] = entities;
	return object;
}

std::optional<DeletedEntry> DeserializeDeletedEntry(
		const QJsonObject &object) {
	auto entry = DeletedEntry();
	entry.peerId = IdValue(object, "peerId");
	entry.peerName = object.value("peerName").toString();
	entry.msgId = IdValue(object, "msgId");
	entry.fromId = IdValue(object, "fromId");
	entry.fromName = object.value("fromName").toString();
	entry.date = qint64(object.value("date").toDouble());
	entry.editedDate = qint64(object.value("editedDate").toDouble());
	entry.text = object.value("text").toString();
	entry.media = object.value("media").toString();
	entry.out = object.value("out").toBool();
	entry.read = object.value("read").toBool(true);
	entry.replyToMsgId = IdValue(object, "replyToMsgId");
	entry.replyToText = object.value("replyToText").toString();
	entry.forwardedFrom = object.value("forwardedFrom").toString();
	entry.forwardedDate = qint64(object.value("forwardedDate").toDouble());
	for (const auto &value : object.value("reactions").toArray()) {
		const auto reaction = value.toString();
		if (!reaction.isEmpty()) {
			entry.reactions.push_back(reaction);
		}
	}
	for (const auto &value : object.value("entities").toArray()) {
		const auto entity = value.toObject();
		const auto type = entity.value("type").toInt(-1);
		const auto offset = entity.value("offset").toInt(-1);
		const auto length = entity.value("length").toInt(-1);
		if (type < 0
			|| type > kMaxEntityType
			|| offset < 0
			|| length <= 0
			|| offset + length > entry.text.size()) {
			continue;
		}
		entry.entities.push_back({
			.type = type,
			.offset = offset,
			.length = length,
			.data = entity.value("data").toString(),
		});
	}
	if (!entry.peerId || !entry.msgId) {
		return std::nullopt;
	}
	return entry;
}

} // namespace Exteraless
