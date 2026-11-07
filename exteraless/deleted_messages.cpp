#include "exteraless/deleted_messages.h"

#include "exteraless/config.h"
#include "exteraless/deleted_messages_trim.h"

#include "base/flat_set.h"
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

TextWithTags DeletedMessages::BuildQuotePrefix(
		const Entry &entry,
		const QString &authorName) {
	const auto hasAuthor = !authorName.isEmpty();
	auto rich = TextWithEntities{ .text = hasAuthor
		? (authorName + u"\n"_q + entry.text + u"\n\n"_q)
		: (entry.text + u"\n\n"_q) };
	if (hasAuthor) {
		rich.entities.push_back(EntityInText(
			EntityType::Bold,
			0,
			int(authorName.size())));
	}
	const auto shift = hasAuthor ? int(authorName.size()) + 1 : 0;
	for (const auto &entity : entry.entities) {
		rich.entities.push_back(EntityInText(
			EntityType(entity.type),
			entity.offset + shift,
			entity.length,
			entity.data));
	}
	return {
		.text = rich.text,
		.tags = TextUtilities::ConvertEntitiesToTextTags(rich.entities),
	};
}

void DeletedMessages::attach(Main::Session *session) {
	if (!session) {
		return;
	}
	const auto path = QFileInfo(
		session->local().cachePath()).absolutePath()
		+ u"/exteraless/deleted_messages.json"_q;
	if (_path == path) {
		_session = session;
		return;
	}

	save();
	_path = path;
	_session = session;
	_entries.clear();
	_inserted.clear();
	_placeholders.clear();
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
	if (const auto history = _session
		? _session->data().historyLoaded(peerId)
		: nullptr) {
		removeFrom(history);
	}
	save();
}

void DeletedMessages::insertFor(not_null<History*> history) {
	if (!_session || (&history->peer->session() != _session)) {
		return;
	}
	if (!Config::Instance().saveDeletedMessages()
		|| !history->loadedAtTop()
		|| !history->loadedAtBottom()) {
		return;
	}
	auto &data = _session->data();
	const auto peerId = history->peer->id;
	const auto entries = list(peerId);
	if (entries.empty()) {
		return;
	}
	const auto &config = Config::Instance();
	const auto customMark = config.customDeletedMark();
	const auto mark = config.useDeletedIcon()
		? (customMark.isEmpty() ? u"🗑"_q : customMark)
		: customMark;
	auto inserted = false;
	for (const auto &entry : entries) {
		const auto msgId = MsgId(entry.msgId);
		const auto original = FullMsgId(peerId, msgId);
		if (!entry.date || _inserted.contains(original)) {
			continue;
		}
		auto text = TextWithEntities{ .text = entry.text };
		for (const auto &entity : entry.entities) {
			text.entities.push_back(EntityInText(
				EntityType(entity.type),
				entity.offset,
				entity.length,
				entity.data));
		}
		if (!mark.isEmpty()) {
			text.text = text.text.isEmpty()
				? mark
				: text.text + u"\n\n"_q + mark;
		}
		auto fields = HistoryItemCommonFields{
			.id = data.nextLocalMessageId(),
			.flags = MessageFlag::Local,
			.date = TimeId(entry.date),
		};
		if (const auto from = entry.fromId
			? data.peerLoaded(PeerId(BareId(entry.fromId)))
			: nullptr) {
			fields.from = from->id;
			fields.flags |= MessageFlag::HasFromId;
		}
		if (entry.out) {
			fields.flags |= MessageFlag::Outgoing;
		}
		const auto item = history->addLocalMessageInPlace(
			std::move(fields),
			text,
			msgId);
		_inserted.emplace(original, item->id);
		_placeholders.emplace(item->fullId(), msgId);
		inserted = true;
	}
	if (inserted) {
		data.notifyHistoryChangeDelayed(history);
	}
}

void DeletedMessages::removeFrom(not_null<History*> history) {
	if (!_session) {
		return;
	}
	const auto &data = _session->data();
	const auto peerId = history->peer->id;
	auto items = std::vector<not_null<HistoryItem*>>();
	for (const auto &entry : _placeholders) {
		const auto &fullId = entry.first;
		if (fullId.peer != peerId) {
			continue;
		}
		if (const auto item = data.message(fullId)) {
			items.push_back(item);
		}
	}
	for (const auto &item : items) {
		history->destroyMessage(item);
	}
	for (auto i = _placeholders.begin(); i != _placeholders.end();) {
		if (i->first.peer == peerId) {
			i = _placeholders.erase(i);
		} else {
			++i;
		}
	}
	for (auto i = _inserted.begin(); i != _inserted.end();) {
		if (i->first.peer == peerId) {
			i = _inserted.erase(i);
		} else {
			++i;
		}
	}
}

void DeletedMessages::refresh() {
	if (!_session) {
		return;
	}
	auto peers = base::flat_set<PeerId>();
	for (const auto &entry : _inserted) {
		peers.emplace(entry.first.peer);
	}
	for (const auto &entry : _entries) {
		peers.emplace(PeerId(BareId(entry.peerId)));
	}
	for (const auto &peerId : peers) {
		if (const auto history = _session->data().historyLoaded(peerId)) {
			removeFrom(history);
			insertFor(history);
		}
	}
}

std::optional<DeletedMessages::Entry> DeletedMessages::entryForPlaceholder(
		const FullMsgId &placeholderId) const {
	const auto original = _placeholders.find(placeholderId);
	if (original == _placeholders.end()) {
		return std::nullopt;
	}
	const auto peerId = qint64(placeholderId.peer.value);
	const auto msgId = qint64(original->second.bare);
	for (const auto &entry : _entries) {
		if (entry.peerId == peerId && entry.msgId == msgId) {
			return entry;
		}
	}
	return std::nullopt;
}

bool DeletedMessages::isPlaceholder(FullMsgId itemId) const {
	return _placeholders.contains(itemId);
}

bool DeletedMessages::isPlaceholder(not_null<HistoryItem*> item) const {
	return isPlaceholder(item->fullId());
}

void DeletedMessages::onItemsDestroyed(
		const std::vector<not_null<HistoryItem*>> &items) {
	if (Config::Instance().saveDeletedMessages()) {
		auto changed = false;
		for (const auto &item : items) {
			if (isPlaceholder(item) || !ShouldSave(item)) {
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
					entry.forwardedFrom
						= forwarded->originalHiddenSenderInfo->name;
				}
				entry.forwardedDate = forwarded->originalDate;
			}
			for (const auto &reaction : item->reactions()) {
				if (const auto text = ReactionText(reaction.id);
					!text.isEmpty()) {
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
	for (const auto &item : items) {
		const auto fullId = item->fullId();
		if (const auto original = _placeholders.take(fullId)) {
			_inserted.remove(FullMsgId(fullId.peer, *original));
		}
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
