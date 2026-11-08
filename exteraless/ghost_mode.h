#pragma once

#include "exteraless/ghost_status.h"
#include "mtproto/mtproto_request_filter.h"

#include "data/data_peer_id.h"

namespace MTP {
class Instance;
} // namespace MTP

namespace Exteraless {

class GhostMode final {
public:
	static MTP::RequestFilterResult Filter(const MTP::RequestFilterInfo &info);
	static void AllowReadPackets(int count = 1);
	static void AllowReadPacketsFor(PeerId peerId);
	static void AllowReadPacketsForSelf(PeerId selfId);
	static void AllowTypingPacketsFor(PeerId peerId);
	static void AllowStoryPacketsFor(PeerId peerId);

private:
	static bool ConsumeReadAllowance(qint64 peerKey);
	static bool ConsumeTypingAllowance();
	static bool ConsumeStoryAllowance();
	static void AllowTypingPackets(int count);
	static void AllowStoryPackets(int count);
};

void InstallGhostMode(MTP::Instance &instance);

} // namespace Exteraless
