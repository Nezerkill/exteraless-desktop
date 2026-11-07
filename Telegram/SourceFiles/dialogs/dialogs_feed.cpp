/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "dialogs/dialogs_feed.h"

#include "base/flat_set.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_channel.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "main/main_session.h"

namespace Dialogs::Feed {
namespace {

constexpr auto kExcludedKey = "feed_excluded";

struct Storage {
	base::flat_set<PeerId> excluded;
	bool loaded = false;
	uint64 generation = 0;
};

[[nodiscard]] Storage &Global() {
	static auto result = Storage();
	return result;
}

void WriteExcluded() {
	auto packed = QByteArray();
	for (const auto &id : Global().excluded) {
		if (!packed.isEmpty()) {
			packed.append(',');
		}
		packed.append(QByteArray::number(id.value));
	}
	Core::App().settings().writePref<QByteArray>(kExcludedKey, packed);
}

void Load() {
	auto &global = Global();
	if (global.loaded) {
		return;
	}
	global.loaded = true;
	const auto packed = Core::App().settings().readPref<QByteArray>(
		kExcludedKey);
	for (const auto &part : packed.split(',')) {
		auto ok = false;
		const auto value = part.toULongLong(&ok);
		if (ok) {
			global.excluded.emplace(PeerId(value));
		}
	}
}

} // namespace

bool IsEligible(const PeerData *peer) {
	return peer && peer->isBroadcast() && !peer->isMegagroup();
}

std::vector<not_null<History*>> Collect(not_null<Main::Session*> session) {
	Load();
	auto result = std::vector<not_null<History*>>();
	session->data().enumerateBroadcasts([&](not_null<ChannelData*> channel) {
		if (!IsEligible(channel) || IsExcluded(channel->id)) {
			return;
		}
		result.push_back(session->data().history(channel->id));
	});
	return result;
}

bool IsExcluded(PeerId peerId) {
	Load();
	return Global().excluded.contains(peerId);
}

void SetExcluded(PeerId peerId, bool excluded) {
	Load();
	auto &global = Global();
	if (excluded == global.excluded.contains(peerId)) {
		return;
	}
	if (excluded) {
		global.excluded.emplace(peerId);
	} else {
		global.excluded.remove(peerId);
	}
	WriteExcluded();
	Invalidate();
}

void Invalidate() {
	++Global().generation;
}

uint64 Generation() {
	return Global().generation;
}

} // namespace Dialogs::Feed
