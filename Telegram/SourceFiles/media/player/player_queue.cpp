/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "media/player/player_queue.h"

#include "base/algorithm.h"
#include "data/data_sparse_ids.h"
#include "media/player/media_player_instance.h"

namespace Exteraless {
namespace {

[[nodiscard]] std::vector<FullMsgId> SliceTracks(
		const SparseIdsMergedSlice &slice) {
	const auto count = slice.size();
	auto result = std::vector<FullMsgId>();
	result.reserve(count);
	for (auto i = 0; i != count; ++i) {
		result.push_back(slice[i]);
	}
	return result;
}

} // namespace

std::optional<int> PlayerQueue::indexOf(FullMsgId id) const {
	const auto i = ranges::find(_order, id);
	return (i != end(_order))
		? std::make_optional(int(i - begin(_order)))
		: std::nullopt;
}

void PlayerQueue::rebuild(const SparseIdsMergedSlice &slice) {
	rebuild(SliceTracks(slice));
}

void PlayerQueue::rebuild(std::vector<FullMsgId> tracks) {
	if (_order == tracks) {
		return;
	}
	_order = std::move(tracks);
	_changed.fire({});
}

bool PlayerQueue::reorder(int from, int to) {
	const auto count = size();
	if (from < 0 || from >= count || to < 0 || to >= count || from == to) {
		return false;
	}
	base::reorder(_order, from, to);
	_changed.fire({});
	return true;
}

bool PlayerQueue::remove(FullMsgId id) {
	const auto i = ranges::find(_order, id);
	if (i == end(_order)) {
		return false;
	}
	_order.erase(i);
	_changed.fire({});
	return true;
}

void PlayerQueue::attach(
		not_null<Media::Player::Instance*> player,
		AudioMsgId::Type type,
		Fn<std::vector<FullMsgId>()> readTracks) {
	const auto read = [=] {
		return readTracks ? readTracks() : std::vector<FullMsgId>();
	};
	_attached.destroy();
	rebuild(read());
	player->playlistChanges(type) | rpl::on_next([=] {
		rebuild(read());
	}, _attached);
}

void PlayerQueue::detach() {
	_attached.destroy();
}

} // namespace Exteraless
