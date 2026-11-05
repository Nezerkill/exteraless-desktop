#pragma once

namespace Exteraless {

enum class GhostStatus {
	Off,
	On,
	Partial,
};

[[nodiscard]] int GhostEnabledCount();
[[nodiscard]] bool IsGhostActive();
[[nodiscard]] bool IsGhostPartiallyActive();
[[nodiscard]] GhostStatus GetGhostStatus();
void SetGhostActive(bool active);
void ToggleGhostActive();

} // namespace Exteraless
