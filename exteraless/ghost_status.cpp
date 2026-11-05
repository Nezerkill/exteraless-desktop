#include "exteraless/ghost_status.h"

#include "exteraless/config.h"

namespace Exteraless {

int GhostEnabledCount() {
	const auto &config = Config::Instance();
	auto result = 0;
	result += config.ghostSendReadPackets() ? 0 : 1;
	result += config.ghostSendReadStories() ? 0 : 1;
	result += config.ghostSendOnline() ? 0 : 1;
	result += config.ghostSendUploadProgress() ? 0 : 1;
	return result;
}

bool IsGhostActive() {
	return GhostEnabledCount() == 4;
}

bool IsGhostPartiallyActive() {
	const auto count = GhostEnabledCount();
	return count > 0 && count < 4;
}

GhostStatus GetGhostStatus() {
	const auto count = GhostEnabledCount();
	return (count == 4)
		? GhostStatus::On
		: (count > 0)
		? GhostStatus::Partial
		: GhostStatus::Off;
}

void SetGhostActive(bool active) {
	auto &config = Config::Instance();
	config.setGhostSendReadPackets(!active);
	config.setGhostSendReadStories(!active);
	config.setGhostSendOnline(!active);
	config.setGhostSendUploadProgress(!active);
}

void ToggleGhostActive() {
	SetGhostActive(!IsGhostActive());
}

} // namespace Exteraless
