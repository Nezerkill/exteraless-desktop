/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"
#include "data/data_msg_id.h"
#include "data/data_audio_msg_id.h"
#include "rpl/event_stream.h"

#include <optional>
#include <vector>

class SparseIdsMergedSlice;

namespace Media::Player {
class Instance;
} // namespace Media::Player

namespace Exteraless {

// WHY: the playlist slice is private to Media::Player::Instance, so the
// queue is fed the current slice tracks by its owner and keeps only the
// local order on top of them.
class PlayerQueue final {
public:
	[[nodiscard]] const std::vector<FullMsgId> &order() const {
		return _order;
	}
	[[nodiscard]] int size() const {
		return int(_order.size());
	}
	[[nodiscard]] bool empty() const {
		return _order.empty();
	}
	[[nodiscard]] std::optional<int> indexOf(FullMsgId id) const;
	[[nodiscard]] rpl::producer<> changed() const {
		return _changed.events();
	}

	void rebuild(const SparseIdsMergedSlice &slice);
	void rebuild(std::vector<FullMsgId> tracks);
	bool reorder(int from, int to);
	bool remove(FullMsgId id);

	void attach(
		not_null<Media::Player::Instance*> player,
		AudioMsgId::Type type,
		Fn<std::vector<FullMsgId>()> readTracks);
	void detach();

private:
	std::vector<FullMsgId> _order;
	rpl::event_stream<> _changed;
	rpl::lifetime _attached;

};

} // namespace Exteraless
