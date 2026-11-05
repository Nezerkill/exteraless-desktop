#pragma once

#include "base/basic_types.h"
#include "data/data_peer_id.h"
#include "exteraless/deleted_entry.h"

#include <QString>
#include <vector>

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

	void attach(Main::Session *session);

	[[nodiscard]] int count(PeerId peerId) const;
	[[nodiscard]] std::vector<Entry> list(PeerId peerId) const;
	void clear(PeerId peerId);

private:
	DeletedMessages() = default;

	void onItemsDestroyed(const std::vector<not_null<HistoryItem*>> &items);
	void load();
	void save();

	QString _path;
	std::vector<Entry> _entries;
};

} // namespace Exteraless
