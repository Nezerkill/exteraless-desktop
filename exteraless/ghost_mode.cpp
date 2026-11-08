#include "exteraless/ghost_mode.h"

#include "exteraless/config.h"
#include "mtproto/mtp_instance.h"

#include <crl/crl.h>

#include <array>
#include <atomic>
#include <mutex>

namespace Exteraless {
namespace {

constexpr auto kAllowanceTtl = crl::time(5000);

constexpr auto kReadHistory = mtpTypeId(0x0e306d3a);
constexpr auto kChannelsReadHistory = mtpTypeId(0xcc104937);
constexpr auto kReadContents = mtpTypeId(0x36a73f77);
constexpr auto kChannelsReadContents = mtpTypeId(0xeab5dc38);
constexpr auto kSetTyping = mtpTypeId(0x58943ee2);
constexpr auto kReadStories = mtpTypeId(0xa556dac8);
constexpr auto kIncrementStoryViews = mtpTypeId(0xb2028afb);
constexpr auto kReadDiscussion = mtpTypeId(0xf731a9f4);
constexpr auto kReadSavedHistory = mtpTypeId(0xba4a3b5b);
constexpr auto kReadMentions = mtpTypeId(0x36e5bf4d);

constexpr auto kInputPeerEmpty = mtpTypeId(0x7f3b18ea);
constexpr auto kInputPeerSelf = mtpTypeId(0x7da07ec9);
constexpr auto kInputPeerChat = mtpTypeId(0x35a95cb9);
constexpr auto kInputPeerUser = mtpTypeId(0xdde8a54c);
constexpr auto kInputPeerChannel = mtpTypeId(0x27bcbbfc);
constexpr auto kInputPeerUserFromMessage = mtpTypeId(0xa87b0a1c);
constexpr auto kInputPeerChannelFromMessage = mtpTypeId(0xbd2a0840);
constexpr auto kInputChannel = mtpTypeId(0xf35aec28);
constexpr auto kInputChannelFromMessage = mtpTypeId(0x5b934f9d);

constexpr auto kSelfPeerKey = qint64(-1); // WHY: inputPeerSelf carries no id.

struct ReadSlot {
	qint64 peerKey = 0;
	int remaining = 0;
	crl::time until = 0;
};

constexpr auto kReadSlots = 4;

std::mutex ReadAllowanceLock;
std::array<ReadSlot, kReadSlots> ReadSlots;

std::atomic<int> TypingAllowance = 0;
std::atomic<crl::time> TypingAllowanceUntil = 0;
std::atomic<int> StoryAllowance = 0;
std::atomic<crl::time> StoryAllowanceUntil = 0;

[[nodiscard]] qint64 PeerKey(PeerId peerId) {
	return qint64(peerId.value);
}

[[nodiscard]] BareId ReadLong(const mtpPrime *from) {
	return BareId(quint32(from[0])) | (BareId(quint32(from[1])) << 32);
}

[[nodiscard]] const mtpPrime *SkipInputPeer(
		const mtpPrime *from,
		const mtpPrime *end) {
	if (from >= end) {
		return end;
	}
	switch (mtpTypeId(*from)) {
	case kInputPeerEmpty:
	case kInputPeerSelf:
		return from + 1;
	case kInputPeerChat:
		return from + 3;
	case kInputPeerUser:
	case kInputPeerChannel:
		return from + 5;
	case kInputPeerUserFromMessage:
	case kInputPeerChannelFromMessage: {
		const auto nested = SkipInputPeer(from + 1, end);
		return (end - nested >= 3) ? (nested + 3) : end;
	}
	default:
		return end;
	}
}

[[nodiscard]] qint64 InputPeerKey(const mtpPrime *from, const mtpPrime *end) {
	if (from >= end) {
		return 0;
	}
	const auto available = end - from;
	switch (mtpTypeId(*from)) {
	case kInputPeerSelf:
		return kSelfPeerKey;
	case kInputPeerUser:
		return (available >= 5)
			? PeerKey(peerFromUser(UserId(ReadLong(from + 1))))
			: 0;
	case kInputPeerChat:
		return (available >= 3)
			? PeerKey(peerFromChat(ChatId(ReadLong(from + 1))))
			: 0;
	case kInputPeerChannel:
		return (available >= 5)
			? PeerKey(peerFromChannel(ChannelId(ReadLong(from + 1))))
			: 0;
	case kInputPeerUserFromMessage:
	case kInputPeerChannelFromMessage: {
		const auto nested = SkipInputPeer(from + 1, end);
		if (end - nested < 3) {
			return 0;
		}
		const auto id = ReadLong(nested + 1);
		return (mtpTypeId(*from) == kInputPeerUserFromMessage)
			? PeerKey(peerFromUser(UserId(id)))
			: PeerKey(peerFromChannel(ChannelId(id)));
	}
	default:
		return 0;
	}
}

[[nodiscard]] qint64 InputChannelKey(
		const mtpPrime *from,
		const mtpPrime *end) {
	if (from >= end) {
		return 0;
	}
	const auto available = end - from;
	switch (mtpTypeId(*from)) {
	case kInputChannel:
		return (available >= 5)
			? PeerKey(peerFromChannel(ChannelId(ReadLong(from + 1))))
			: 0;
	case kInputChannelFromMessage: {
		const auto nested = SkipInputPeer(from + 1, end);
		return (end - nested >= 3)
			? PeerKey(peerFromChannel(ChannelId(ReadLong(nested + 1))))
			: 0;
	}
	default:
		return 0;
	}
}

[[nodiscard]] qint64 ExtractPeerKey(const MTP::RequestFilterInfo &info) {
	if (!info.body || !info.bodySize) {
		return 0;
	}
	const auto end = info.body + info.bodySize;
	switch (info.type) {
	case kReadHistory:
	case kReadDiscussion:
		return InputPeerKey(info.body + 1, end);
	case kReadMentions:
		return InputPeerKey(info.body + 2, end);
	case kChannelsReadHistory:
		return InputChannelKey(info.body + 1, end);
	case kReadSavedHistory: {
		const auto second = SkipInputPeer(info.body + 1, end);
		return InputPeerKey(second, end);
	}
	default:
		return 0;
	}
}

[[nodiscard]] ReadSlot *FindReadSlot(qint64 peerKey, crl::time now) {
	for (auto &slot : ReadSlots) {
		if (slot.peerKey == peerKey
			&& slot.remaining > 0
			&& now <= slot.until) {
			return &slot;
		}
	}
	return nullptr;
}

[[nodiscard]] ReadSlot &ReserveReadSlot(qint64 peerKey, crl::time now) {
	ReadSlot *free = nullptr;
	auto oldest = &ReadSlots.front();
	for (auto &slot : ReadSlots) {
		if (slot.peerKey == peerKey) {
			return slot;
		}
		if (!free && (slot.remaining <= 0 || now > slot.until)) {
			free = &slot;
		}
		if (slot.until < oldest->until) {
			oldest = &slot;
		}
	}
	return *(free ? free : oldest);
}

[[nodiscard]] bool ConsumeReadSlot(qint64 peerKey, crl::time now) {
	const auto slot = FindReadSlot(peerKey, now);
	if (!slot) {
		return false;
	}
	--slot->remaining;
	return true;
}

void AllowReadSlot(qint64 peerKey, int count) {
	const auto now = crl::now();
	auto guard = std::lock_guard(ReadAllowanceLock);
	auto &slot = ReserveReadSlot(peerKey, now);
	slot.peerKey = peerKey;
	slot.remaining = count;
	slot.until = now + kAllowanceTtl;
}

[[nodiscard]] bool IsReadRequest(mtpTypeId type) {
	switch (type) {
	case kReadHistory:
	case kChannelsReadHistory:
	case kReadContents:
	case kChannelsReadContents:
	case kReadDiscussion:
	case kReadSavedHistory:
		return true;
	default:
		return false;
	}
}

} // namespace

MTP::RequestFilterResult GhostMode::Filter(
		const MTP::RequestFilterInfo &info) {
	if (!info.type) {
		return MTP::RequestFilterResult::Pass;
	}

	auto &config = Config::Instance();
	if (IsReadRequest(info.type)) {
		if (ConsumeReadAllowance(ExtractPeerKey(info))) {
			return MTP::RequestFilterResult::Pass;
		}
		return config.ghostSendReadPackets()
			? MTP::RequestFilterResult::Pass
			: MTP::RequestFilterResult::Drop;
	}
	switch (info.type) {
	case kSetTyping:
		if (ConsumeTypingAllowance()) {
			return MTP::RequestFilterResult::Pass;
		}
		return config.ghostSendUploadProgress()
			? MTP::RequestFilterResult::Pass
			: MTP::RequestFilterResult::Drop;
	case kReadStories:
	case kIncrementStoryViews:
		if (ConsumeStoryAllowance()) {
			return MTP::RequestFilterResult::Pass;
		}
		return config.ghostSendReadStories()
			? MTP::RequestFilterResult::Pass
			: MTP::RequestFilterResult::Drop;
	default:
		return MTP::RequestFilterResult::Pass;
	}
}

void GhostMode::AllowReadPackets(int count) {
	AllowReadSlot(0, count);
}

void GhostMode::AllowReadPacketsFor(PeerId peerId) {
	if (Config::Instance().ghostReadExcluded(PeerKey(peerId))) {
		AllowReadSlot(PeerKey(peerId), 1);
	}
}

void GhostMode::AllowReadPacketsForSelf(PeerId selfId) {
	if (Config::Instance().ghostReadExcluded(PeerKey(selfId))) {
		AllowReadSlot(kSelfPeerKey, 1);
	}
}

void GhostMode::AllowTypingPacketsFor(PeerId peerId) {
	if (Config::Instance().ghostTypingExcluded(PeerKey(peerId))) {
		AllowTypingPackets(1);
	}
}

void GhostMode::AllowStoryPacketsFor(PeerId peerId) {
	if (Config::Instance().ghostReadExcluded(PeerKey(peerId))) {
		AllowStoryPackets(1);
	}
}

void GhostMode::AllowTypingPackets(int count) {
	TypingAllowance.store(count);
	TypingAllowanceUntil.store(crl::now() + kAllowanceTtl);
}

void GhostMode::AllowStoryPackets(int count) {
	StoryAllowance.store(count);
	StoryAllowanceUntil.store(crl::now() + kAllowanceTtl);
}

bool GhostMode::ConsumeReadAllowance(qint64 peerKey) {
	const auto now = crl::now();
	auto guard = std::lock_guard(ReadAllowanceLock);
	if (peerKey && ConsumeReadSlot(peerKey, now)) {
		return true;
	}
	return ConsumeReadSlot(0, now); // WHY: no peer slot keeps the old global allowance.
}

bool GhostMode::ConsumeTypingAllowance() {
	if (TypingAllowance.load() <= 0 || crl::now() > TypingAllowanceUntil.load()) {
		return false;
	}
	return TypingAllowance.fetch_sub(1) > 0;
}

bool GhostMode::ConsumeStoryAllowance() {
	if (StoryAllowance.load() <= 0 || crl::now() > StoryAllowanceUntil.load()) {
		return false;
	}
	return StoryAllowance.fetch_sub(1) > 0;
}

void InstallGhostMode(MTP::Instance &instance) {
	instance.setRequestFilter(&GhostMode::Filter);
}

} // namespace Exteraless
