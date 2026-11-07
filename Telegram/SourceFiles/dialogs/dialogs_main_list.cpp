/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "dialogs/dialogs_main_list.h"

#include "dialogs/dialogs_feed.h"
#include "dialogs/dialogs_filters.h"
#include "data/data_changes.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_chat_filters.h"
#include "main/main_session.h"
#include "history/history_unread_things.h"
#include "history/history.h"

#include "base/flat_set.h"

namespace Dialogs {
namespace {

constexpr auto kFeedRefreshDelay = crl::time(1000);

void FillFeed(MainList &list, not_null<Main::Session*> session) {
	for (const auto &history : Feed::Collect(session)) {
		list.addEntry(Key(history));
	}
	list.setLoaded();
}

} // namespace

MainList::MainList(
	not_null<Main::Session*> session,
	FilterId filterId,
	rpl::producer<int> pinnedLimit)
: _session(session)
, _filterId(filterId)
, _all(SortMode::Date, filterId)
, _pinned(filterId, 1) {
	_unreadState.known = true;

	std::move(
		pinnedLimit
	) | rpl::on_next([=](int limit) {
		_pinned.setLimit(std::max(limit, 1));
	}, _lifetime);

	session->changes().realtimeNameUpdates(
	) | rpl::on_next([=](const Data::NameUpdate &update) {
		_all.peerNameChanged(_filterId, update.peer, update.oldFirstLetters);
	}, _lifetime);

	if (_filterId == kFeedFilterId) {
		_feedRefreshTimer = std::make_unique<base::Timer>([=] {
			rebuildFeed();
		});
		FillFeed(*this, session);
		std::move(
			rpl::merge(
				session->data().chatsListChanges() | rpl::to_empty,
				Feed::Changes()
			)
		) | rpl::on_next([=] {
			feedChanged();
		}, _lifetime);
	}
}

void MainList::feedChanged() {
	if (_feedRefreshing || !_feedRefreshTimer) {
		return;
	}
	const auto sinceLastRefresh = _feedRefreshElapsed.isValid()
		? crl::time(_feedRefreshElapsed.elapsed())
		: kFeedRefreshDelay;
	if (sinceLastRefresh >= kFeedRefreshDelay) {
		rebuildFeed();
	} else if (!_feedRefreshTimer->isActive()) {
		_feedRefreshTimer->callOnce(kFeedRefreshDelay - sinceLastRefresh);
	}
}

void MainList::rebuildFeed() {
	_feedRefreshElapsed.start();

	const auto desired = Feed::Collect(_session);
	auto keep = base::flat_set<PeerId>();
	for (const auto &history : desired) {
		keep.emplace(history->peer->id);
	}

	auto stale = std::vector<Key>();
	for (const auto &row : _all) {
		const auto history = row->history();
		if (!history || !keep.contains(history->peer->id)) {
			stale.push_back(row->key());
		}
	}
	auto changed = !stale.empty();
	for (const auto &key : stale) {
		removeEntry(key);
	}
	for (const auto &history : desired) {
		const auto key = Key(history);
		if (!_all.contains(key)) {
			addEntry(key);
			changed = true;
		}
	}
	if (!changed) {
		return;
	}
	_feedRefreshing = true;
	const auto guard = gsl::finally([&] {
		_feedRefreshing = false;
	});
	_session->data().chatsListChanged(nullptr);
}

bool MainList::empty() const {
	return _all.empty();
}

bool MainList::loaded() const {
	return _loaded;
}

void MainList::setLoaded(bool loaded) {
	if (_loaded == loaded) {
		return;
	}
	const auto recomputer = gsl::finally([&] {
		recomputeFullListSize();
	});
	const auto notifier = unreadStateChangeNotifier(true);
	_loaded = loaded;
}

void MainList::setAllAreMuted(bool allAreMuted) {
	if (_allAreMuted == allAreMuted) {
		return;
	}
	const auto notifier = unreadStateChangeNotifier(true);
	_allAreMuted = allAreMuted;
}

void MainList::setCloudListSize(int size) {
	if (_cloudListSize == size) {
		return;
	}
	_cloudListSize = size;
	recomputeFullListSize();
}

const rpl::variable<int> &MainList::fullSize() const {
	return _fullListSize;
}

void MainList::clear() {
	const auto recomputer = gsl::finally([&] {
		recomputeFullListSize();
	});
	const auto notifier = unreadStateChangeNotifier(true);
	_pinned.clear();
	_all.clear();
	_unreadState = UnreadState();
	_cloudUnreadState = UnreadState();
	_unreadState.known = true;
	_cloudUnreadState.known = true;
	_cloudListSize = 0;
}

RowsByLetter MainList::addEntry(Key key) {
	const auto result = _all.addToEnd(key);

	const auto unread = key.entry()->chatListUnreadState();
	unreadEntryChanged(unread, true);
	recomputeFullListSize();

	return result;
}

void MainList::removeEntry(Key key) {
	_all.remove(key);

	const auto unread = key.entry()->chatListUnreadState();
	unreadEntryChanged(unread, false);
	recomputeFullListSize();
}

void MainList::recomputeFullListSize() {
	_fullListSize = std::max(_all.size(), loaded() ? 0 : _cloudListSize);
}

void MainList::unreadStateChanged(
		const UnreadState &wasState,
		const UnreadState &nowState) {
	const auto useClouded = _cloudUnreadState.known && !loaded();
	// An entry can go from a known state to an unknown one, for example a
	// forum channel losing the Forum flag while its own unread count was
	// not loaded yet. The delta is meaningless then, so leave the cloud
	// counters alone, like unreadEntryChanged() does.
	const auto updateCloudUnread = _cloudUnreadState.known
		&& wasState.known
		&& nowState.known;
	const auto notify = !useClouded || wasState.known;
	const auto notifier = unreadStateChangeNotifier(notify);
	_unreadState += nowState - wasState;
	if (_unreadState.chatsMuted > _unreadState.chats
		|| _unreadState.messagesMuted > _unreadState.messages) {
		[[maybe_unused]] int a = 0;
	}
	if (updateCloudUnread) {
		_cloudUnreadState += nowState - wasState;
		finalizeCloudUnread();
	}
}

void MainList::unreadEntryChanged(
		const Dialogs::UnreadState &state,
		bool added) {
	if (!state.messages
		&& !state.chats
		&& !state.marks
		&& !state.mentions
		&& !state.reactions) {
		return;
	}
	const auto updateCloudUnread = _cloudUnreadState.known && state.known;
	const auto notify = !_cloudUnreadState.known || loaded() || state.known;
	const auto notifier = unreadStateChangeNotifier(notify);
	if (added) {
		_unreadState += state;
	} else {
		_unreadState -= state;
	}
	if (_unreadState.chatsMuted > _unreadState.chats
		|| _unreadState.messagesMuted > _unreadState.messages) {
		[[maybe_unused]] int a = 0;
	}
	if (updateCloudUnread) {
		if (added) {
			_cloudUnreadState += state;
		} else {
			_cloudUnreadState -= state;
		}
		finalizeCloudUnread();
	}
}

void MainList::updateCloudUnread(const MTPDdialogFolder &data) {
	const auto notifier = unreadStateChangeNotifier(!loaded());

	_cloudUnreadState.messages = data.vunread_muted_messages_count().v
		+ data.vunread_unmuted_messages_count().v;
	_cloudUnreadState.chats = data.vunread_muted_peers_count().v
		+ data.vunread_unmuted_peers_count().v;
	finalizeCloudUnread();

	_cloudUnreadState.known = true;
}

bool MainList::cloudUnreadKnown() const {
	return _cloudUnreadState.known;
}

void MainList::finalizeCloudUnread() {
	// Cloud state for archive folder always counts everything as muted.
	_cloudUnreadState.messagesMuted = _cloudUnreadState.messages;
	_cloudUnreadState.chatsMuted = _cloudUnreadState.chats;

	// We don't know the real value of marked chats counts in cloud unread.
	_cloudUnreadState.marksMuted = _cloudUnreadState.marks = 0;
}

UnreadState MainList::unreadState() const {
	const auto useCloudState = _cloudUnreadState.known && !loaded();
	auto result = useCloudState ? _cloudUnreadState : _unreadState;

	// We don't know the real value of marked chats counts in cloud unread.
	if (useCloudState) {
		result.marks = _unreadState.marks;
		result.marksMuted = _unreadState.marksMuted;
	}
	if (_allAreMuted) {
		result.messagesMuted = result.messages;
		result.chatsMuted = result.chats;
		result.marksMuted = result.marks;
	}
#ifdef Q_OS_WIN
	[[maybe_unused]] volatile auto touch = 0
		+ _unreadState.marks + _unreadState.marksMuted
		+ _unreadState.messages + _unreadState.messagesMuted
		+ _unreadState.chats + _unreadState.chatsMuted
		+ _unreadState.reactions + _unreadState.reactionsMuted
		+ _unreadState.mentions;
#endif // Q_OS_WIN
	return result;
}

rpl::producer<UnreadState> MainList::unreadStateChanges() const {
	return _unreadStateChanges.events();
}

not_null<IndexedList*> MainList::indexed() {
	return &_all;
}

not_null<const IndexedList*> MainList::indexed() const {
	return &_all;
}

not_null<PinnedList*> MainList::pinned() {
	return &_pinned;
}

not_null<const PinnedList*> MainList::pinned() const {
	return &_pinned;
}

} // namespace Dialogs
