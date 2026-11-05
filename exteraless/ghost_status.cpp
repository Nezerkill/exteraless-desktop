#include "exteraless/ghost_status.h"

#include "exteraless/config.h"

namespace Exteraless {

int GhostUnlockedCount() {
	const auto &config = Config::Instance();
	auto result = 0;
	result += config.ghostSendReadLocked() ? 0 : 1;
	result += config.ghostSendStoriesLocked() ? 0 : 1;
	result += config.ghostSendOnlineLocked() ? 0 : 1;
	result += config.ghostSendUploadProgressLocked() ? 0 : 1;
	return result;
}

int GhostEnabledCount() {
	const auto &config = Config::Instance();
	auto result = 0;
	result += (config.ghostSendReadLocked()
		|| config.ghostSendReadPackets()) ? 0 : 1;
	result += (config.ghostSendStoriesLocked()
		|| config.ghostSendReadStories()) ? 0 : 1;
	result += (config.ghostSendOnlineLocked()
		|| config.ghostSendOnline()) ? 0 : 1;
	result += (config.ghostSendUploadProgressLocked()
		|| config.ghostSendUploadProgress()) ? 0 : 1;
	return result;
}

bool IsGhostActive() {
	const auto unlocked = GhostUnlockedCount();
	return unlocked > 0 && GhostEnabledCount() == unlocked;
}

bool IsGhostPartiallyActive() {
	const auto count = GhostEnabledCount();
	return count > 0 && count < GhostUnlockedCount();
}

GhostStatus GetGhostStatus() {
	const auto unlocked = GhostUnlockedCount();
	const auto count = GhostEnabledCount();
	return (unlocked > 0 && count == unlocked)
		? GhostStatus::On
		: (count > 0)
		? GhostStatus::Partial
		: GhostStatus::Off;
}

void SetGhostActive(bool active) {
	auto &config = Config::Instance();
	if (!config.ghostSendReadLocked()) {
		config.setGhostSendReadPackets(!active);
	}
	if (!config.ghostSendStoriesLocked()) {
		config.setGhostSendReadStories(!active);
	}
	if (!config.ghostSendOnlineLocked()) {
		config.setGhostSendOnline(!active);
	}
	if (!config.ghostSendUploadProgressLocked()) {
		config.setGhostSendUploadProgress(!active);
	}
}

void ToggleGhostActive() {
	SetGhostActive(!IsGhostActive());
}

} // namespace Exteraless
