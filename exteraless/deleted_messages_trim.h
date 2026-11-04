#pragma once

#include <QtGlobal>

#include <unordered_map>
#include <vector>

namespace Exteraless {

inline constexpr auto kDeletedMessagesMaxEntries = 1000;
inline constexpr auto kDeletedMessagesMaxPerPeer = 100;

template <typename Entry, typename PeerIdFn>
void TrimDeletedEntries(
		std::vector<Entry> *entries,
		PeerIdFn peerId,
		int maxEntries = kDeletedMessagesMaxEntries,
		int maxPerPeer = kDeletedMessagesMaxPerPeer) {
	if (maxEntries <= 0 || maxPerPeer <= 0) {
		entries->clear();
		return;
	}
	auto perPeer = std::unordered_map<qint64, int>();
	auto keep = std::vector<bool>(entries->size(), false);
	auto kept = 0;
	for (auto i = int(entries->size()); i != 0;) {
		--i;
		auto &seen = perPeer[peerId((*entries)[i])];
		if (seen >= maxPerPeer || kept >= maxEntries) {
			continue;
		}
		++seen;
		++kept;
		keep[i] = true;
	}
	if (kept == int(entries->size())) {
		return;
	}
	auto result = std::vector<Entry>();
	result.reserve(kept);
	for (auto i = 0; i != int(entries->size()); ++i) {
		if (keep[i]) {
			result.push_back(std::move((*entries)[i]));
		}
	}
	*entries = std::move(result);
}

} // namespace Exteraless
