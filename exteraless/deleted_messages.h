#pragma once

#include "base/basic_types.h"
#include "data/data_peer_id.h"

#include <QString>
#include <vector>

class HistoryItem;

namespace Main {
class Session;
} // namespace Main

namespace Exteraless {

class DeletedMessages final {
public:
	struct Entry {
		qint64 peerId = 0;
		QString peerName;
		qint64 msgId = 0;
		qint64 fromId = 0;
		QString fromName;
		qint64 date = 0;
		QString text;
		QString media;
		bool out = false;
	};

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
