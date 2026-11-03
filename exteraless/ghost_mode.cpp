#include "exteraless/ghost_mode.h"

#include "exteraless/config.h"
#include "mtproto/mtp_instance.h"

#include <crl/crl.h>

#include <atomic>

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

std::atomic<int> Allowance = 0;
std::atomic<crl::time> AllowanceUntil = 0;

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
		if (ConsumeReadAllowance()) {
			return MTP::RequestFilterResult::Pass;
		}
		return config.ghostSendReadPackets()
			? MTP::RequestFilterResult::Pass
			: MTP::RequestFilterResult::Drop;
	}
	switch (info.type) {
	case kSetTyping:
		return config.ghostSendUploadProgress()
			? MTP::RequestFilterResult::Pass
			: MTP::RequestFilterResult::Drop;
	case kReadStories:
	case kIncrementStoryViews:
		return config.ghostSendReadStories()
			? MTP::RequestFilterResult::Pass
			: MTP::RequestFilterResult::Drop;
	default:
		return MTP::RequestFilterResult::Pass;
	}
}

void GhostMode::AllowReadPackets(int count) {
	Allowance.store(count);
	AllowanceUntil.store(crl::now() + kAllowanceTtl);
}

bool GhostMode::ConsumeReadAllowance() {
	if (Allowance.load() <= 0 || crl::now() > AllowanceUntil.load()) {
		return false;
	}
	return Allowance.fetch_sub(1) > 0;
}

bool IsGhostActive() {
	const auto &config = Config::Instance();
	return !config.ghostSendReadPackets()
		|| !config.ghostSendReadStories()
		|| !config.ghostSendOnline()
		|| !config.ghostSendUploadProgress();
}

void SetGhostActive(bool active) {
	auto &config = Config::Instance();
	config.setGhostSendReadPackets(!active);
	config.setGhostSendReadStories(!active);
	config.setGhostSendOnline(!active);
	config.setGhostSendUploadProgress(!active);
}

void InstallGhostMode(MTP::Instance &instance) {
	instance.setRequestFilter(&GhostMode::Filter);
}

} // namespace Exteraless
