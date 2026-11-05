#include "exteraless/deleted_messages.h"

#include "exteraless/config.h"
#include "exteraless/deleted_messages_trim.h"

#include "data/data_message_reaction_id.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "main/main_session.h"
#include "storage/storage_account.h"
#include "ui/text/text_entity.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

#include <algorithm>
#include <rpl/rpl.h>

namespace Exteraless {
namespace {

constexpr auto kVersion = 2;

[[nodiscard]] QString IdString(qint64 value) {
	return QString::number(value);
}

[[nodiscard]] QString ReactionText(const Data::ReactionId &id) {
	if (const auto emoji = id.emoji(); !emoji.isEmpty()) {
		return emoji;
	}
	if (const auto custom = id.custom()) {
		return u"custom:%1"_q.arg(custom);
	}
	return {};
}

[[nodiscard]] bool ShouldSave(not_null<HistoryItem*> item) {
	return item->isRegular()
		&& !item->isService()
		&& !item->isScheduled()
		&& !item->isEphemeral()
		&& !item->isBusinessShortcut()
		&& !item->isAdminLogEntry()
		&& !item->isSending()
		&& !item->hasFailed()
		&& !item->ttlDestroyAt();
}

} // namespace

DeletedMessages &DeletedMessages::Instance() {
	static auto instance = DeletedMessages();
	return instance;
}

void DeletedMessages::attach(Main::Session *session) {
	if (!session) {
		return;
	}
	const auto path = QFileInfo(
		session->local().cachePath()).absolutePath()
		+ u"/exteraless/deleted_messages.json"_q;
	if (_path == path) {
		return;
	}

	save();
	_path = path;
	_entries.clear();
	load();

	session->data().itemsAboutToBeDestroyed(
	) | rpl::on_next([=](
			std::vector<not_null<HistoryItem*>> items) {
		onItemsDestroyed(items);
	}, session->lifetime());
}

int DeletedMessages::count(PeerId peerId) const {
	const auto key = IdString(peerId.value);
	auto result = 0;
	for (const auto &entry : _entries) {
		if (IdString(entry.peerId) == key) {
			++result;
		}
	}
	return result;
}

std::vector<DeletedMessages::Entry> DeletedMessages::list(
		PeerId peerId) const {
	const auto key = IdString(peerId.value);
	auto result = std::vector<Entry>();
	for (const auto &entry : _entries) {
		if (IdString(entry.peerId) == key) {
			result.push_back(entry);
		}
	}
	return result;
}

void DeletedMessages::clear(PeerId peerId) {
	const auto key = IdString(peerId.value);
	std::erase_if(_entries, [&](const Entry &entry) {
		return IdString(entry.peerId) == key;
	});
	save();
}

void DeletedMessages::onItemsDestroyed(
		const std::vector<not_null<HistoryItem*>> &items) {
	if (!Config::Instance().saveDeletedMessages()) {
		return;
	}
	auto changed = false;
	for (const auto &item : items) {
		if (!ShouldSave(item)) {
			continue;
		}
		const auto fullId = item->fullId();
		const auto peerId = qint64(fullId.peer.value);
		const auto msgId = qint64(fullId.msg.bare);
		const auto duplicate = std::any_of(
			_entries.begin(),
			_entries.end(),
			[&](const Entry &entry) {
				return entry.peerId == peerId && entry.msgId == msgId;
			});
		if (duplicate) {
			continue;
		}

		auto entry = Entry();
		entry.peerId = peerId;
		entry.peerName = item->history()->peer->name();
		entry.msgId = msgId;
		entry.fromId = item->from()
			? qint64(item->from()->id.value)
			: 0;
		entry.fromName = item->from() ? item->from()->name() : QString();
		entry.date = item->date();
		entry.text = item->originalText().text;
		if (const auto edited = item->Get<HistoryMessageEdited>()) {
			entry.editedDate = edited->date;
		}
		if (const auto reply = item->Get<HistoryMessageReply>()) {
			entry.replyToMsgId = qint64(reply->fields().messageId.bare);
			entry.replyToText = reply->fields().quote.text;
		}
		if (const auto forwarded = item->Get<HistoryMessageForwarded>()) {
			if (forwarded->originalSender) {
				entry.forwardedFrom = forwarded->originalSender->name();
			} else if (forwarded->originalHiddenSenderInfo) {
				entry.forwardedFrom = forwarded->originalHiddenSenderInfo->name;
			}
			entry.forwardedDate = forwarded->originalDate;
		}
		for (const auto &reaction : item->reactions()) {
			if (const auto text = ReactionText(reaction.id); !text.isEmpty()) {
				entry.reactions.push_back(text);
			}
		}
		for (const auto &entity : item->originalText().entities) {
			entry.entities.push_back({
				.type = int(entity.type()),
				.offset = entity.offset(),
				.length = entity.length(),
				.data = entity.data(),
			});
		}
		entry.media = item->media() ? u"media"_q : QString();
		entry.out = item->out();
		entry.read = !item->isUnreadMention() && !item->isUnreadMedia();
		_entries.push_back(std::move(entry));
		changed = true;
	}
	if (changed) {
		TrimDeletedEntries(
			&_entries,
			[](const Entry &entry) { return entry.peerId; });
		save();
	}
}

void DeletedMessages::load() {
	if (_path.isEmpty()) {
		return;
	}
	auto file = QFile(_path);
	if (!file.open(QIODevice::ReadOnly)) {
		return;
	}
	auto error = QJsonParseError{0, QJsonParseError::NoError};
	const auto document = QJsonDocument::fromJson(file.readAll(), &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return;
	}
	const auto messages = document.object().value("messages").toArray();
	for (const auto &value : messages) {
		if (auto entry = DeserializeDeletedEntry(value.toObject())) {
			_entries.push_back(std::move(*entry));
		}
	}
	const auto loaded = _entries.size();
	if (Config::Instance().saveDeletedMessages()) {
		TrimDeletedEntries(
			&_entries,
			[](const Entry &entry) { return entry.peerId; });
		if (_entries.size() != loaded) {
			save();
		}
	}
}

void DeletedMessages::save() {
	if (_path.isEmpty()) {
		return;
	}
	const auto directory = QFileInfo(_path).absolutePath();
	if (!QDir().mkpath(directory)) {
		return;
	}

	auto messages = QJsonArray();
	for (const auto &entry : _entries) {
		messages.append(SerializeDeletedEntry(entry));
	}

	auto root = QJsonObject();
	root["version"] = kVersion;
	root["messages"] = messages;

	auto file = QSaveFile(_path);
	if (!file.open(QIODevice::WriteOnly)) {
		return;
	}
	file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
	file.commit();
}

} // namespace Exteraless
