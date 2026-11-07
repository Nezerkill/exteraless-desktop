/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"
#include "data/data_peer_id.h"

#include <rpl/producer.h>

#include <vector>

class PeerData;
class History;

namespace Main {
class Session;
} // namespace Main

namespace Dialogs::Feed {

[[nodiscard]] bool IsEligible(const PeerData *peer);
[[nodiscard]] std::vector<not_null<History*>> Collect(
	not_null<Main::Session*> session);
[[nodiscard]] bool IsExcluded(PeerId peerId);
void SetExcluded(PeerId peerId, bool excluded);
void Invalidate();
[[nodiscard]] uint64 Generation();
[[nodiscard]] rpl::producer<> Changes();

} // namespace Dialogs::Feed
