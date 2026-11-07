#pragma once

#include "base/basic_types.h"
#include "base/flat_map.h"
#include "data/data_msg_id.h"
#include "data/data_peer_id.h"
#include "exteraless/deleted_entry.h"
#include "ui/text/text_entity.h"

#include <QString>
#include <optional>
#include <vector>

class History;
class HistoryItem;

namespace Main {
class Session;
} // namespace Main

namespace Exteraless {

class DeletedMessages final {
public:
	using Entry = DeletedEntry;
	using Entity = DeletedEntryEntity;

	static DeletedMessages &Instance();

	[[nodiscard]] static TextWithTags BuildQuotePrefix(
		const Entry &entry,
		const QString &authorName);

	void attach(Main::Session *session);

	[[nodiscard]] int count(PeerId peerId) const;
	[[nodiscard]] std::vector<Entry> list(PeerId peerId) const;
	void clear(PeerId peerId);

	void insertFor(not_null<History*> history);
	void removeFrom(not_null<History*> history);
	void refresh();
	[[nodiscard]] std::optional<Entry> entryForPlaceholder(
		const FullMsgId &placeholderId) const;
	[[nodiscard]] bool isPlaceholder(FullMsgId itemId) const;
	[[nodiscard]] bool isPlaceholder(not_null<HistoryItem*> item) const;

private:
	DeletedMessages() = default;

	void onItemsDestroyed(const std::vector<not_null<HistoryItem*>> &items);
	void load();
	void save();

	Main::Session *_session = nullptr;
	QString _path;
	std::vector<Entry> _entries;
	base::flat_map<FullMsgId, MsgId> _inserted;
	base::flat_map<FullMsgId, MsgId> _placeholders;
};

} // namespace Exteraless
